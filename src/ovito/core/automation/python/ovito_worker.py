# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT
"""A persistent Python worker that the topology spike of Phase 2.6 measures.

This is a *spike artifact*, not the production worker of the Python pipeline track: it exists to answer the question the
design leaves open - can a user-selected interpreter, driven as a separate process, serve OVITO's pipeline evaluations
at interactive latency, and what does moving large numeric arrays between the two processes cost? It deliberately
mirrors the constraints of the real design so the numbers mean something:

  * **Serial execution.** One operation runs at a time, in one interpreter, on the thread that owns it. The only other
    thread is a reader that turns `cancel` messages into a flag, which is exactly the cooperative cancellation seam the
    design requires; nothing else may enter the interpreter while an operation runs.
  * **No JSON or pickle for scientific data.** JSON is the control channel. Numeric arrays travel either as a
    length-prefixed raw buffer on the same pipe (`framed`), through a shared-memory segment (`shared-memory`), or - to
    show what the design forbids - as JSON text and base64 (`text`, `base64`), which are measured for comparison only.
  * **Deterministic data.** The payload is derived from the request (a particle array and a mesh index array whose
    contents and checksums the caller can predict), so the caller verifies every transfer mode bit for bit instead of
    trusting the worker's own report. That is what makes the transfer comparison a check and not a measurement.
  * **No side effects.** The worker writes nothing to disk, opens no socket, and touches no user data. It is started with
    an explicit interpreter and the caller may run it isolated.

Protocol: one JSON object per line on stdin, one JSON object per line on stdout, each response repeating the request's
`id`. A request may declare `"bytes": N`, in which case exactly N raw bytes follow its header line and before the next
line - that is the `framed` transfer. Errors are reported as `{"ok": false, "error": {"code": ..., "message": ...}}`,
never as a crash, except for the deliberately crashing `crash` operation.

Operations: `handshake`, `ping`, `process` (the synthetic benchmark of the topology spike), `arrays` (the data
bridge: named numeric arrays in, derived and echoed arrays out, with checksums in both directions), `sleep`, `cancel`,
`crash` and `quit`. The file stays in the tree as the reference implementation of the protocol after the spike that
produced it: the data bridge of deliverable 7 speaks to it, and the installed package's own entry point (Phase 4)
replaces it on the same wire.
"""

import hashlib
import json
import os
import sys
import threading
import time

try:
    import numpy as np
except BaseException:  # noqa: BLE001 - a missing or broken numpy is the case the spike must survive
    np = None

try:
    from multiprocessing import shared_memory
except BaseException:  # noqa: BLE001
    shared_memory = None

HANDSHAKE_NAME = "ovito.automation.handshake"
HANDSHAKE_PROTOCOL_VERSION = "1.0"

#: The transfer modes this worker can serve. `none` and `in-process` are baselines: `none` computes and checksums but
#: returns no array, and `in-process` builds and hashes the arrays without serializing them, which is the cost model of
#: an embedded runtime that touches native arrays directly. A mode whose prerequisite is missing is not advertised at
#: all, because the caller must be able to tell "this environment cannot do it" from "this build is broken".
def _transfer_modes():
    modes = ["none", "in-process", "text", "base64", "framed"]
    if np is not None and shared_memory is not None and os.name == "posix":
        modes.append("shared-memory")
    return modes


TRANSFER_MODES = tuple(_transfer_modes())

_cancelled = {}          # request id -> threading.Event, set by the reader thread
_shm_segments = []       # shared-memory segments still alive for the caller to map
_shm_counter = 0         # monotonic: a name is never reused, because the caller may still have it mapped


def _features():
    """What this environment can do, in the vocabulary of the package contract (PythonContract::Feature)."""
    # The array.buffer feature is advertised unconditionally: the "arrays" op of the data bridge moves numeric arrays
    # over the framed transfer using the standard library's "array" module, so a numpy-free interpreter can do it too.
    # Only the shared-memory transport needs numpy, because filling a segment is what needs it.
    features = ["schema.introspection", "function.inplace", "parameter.scalar", "array.buffer", "traceback.mapping"]
    if np is not None and shared_memory is not None and os.name == "posix":
        features.append("array.shared-memory")
    features.append("task.cancellation")
    return sorted(features)


def _make_data(n, faces, seed):
    """The deterministic payload: `n` particle positions (float64, n x 3) and `faces` triangles (int32, 3 * faces).

    The position formula is spelled the same way in the C++ client, which verifies a few transferred values bit for bit
    instead of trusting the worker's report. The numbers are deliberately awkward decimals, because that is what makes
    a text transfer as expensive as it really is for scientific data.
    """
    if np is not None:
        x = np.arange(n, dtype=np.float64)
        positions = np.empty((n, 3), dtype=np.float64)
        positions[:, 0] = x * 0.1234567890123456 + seed * 0.987654321
        positions[:, 1] = x * -0.2718281828459045 + seed
        positions[:, 2] = x * 0.1414213562373095
        indices = np.arange(3 * faces, dtype=np.int32)
        return positions, indices
    # Without numpy the same arrays are plain lists; the pure-Python path is what an environment without numpy would
    # cost, and it is one of the comparisons the spike reports.
    positions = [[float(i) * 0.1234567890123456 + seed * 0.987654321, float(i) * -0.2718281828459045 + seed,
                  float(i) * 0.1414213562373095] for i in range(n)]
    indices = list(range(3 * faces))
    return positions, indices


def _checksums(positions, indices):
    """Sums the caller can predict from (n, faces, seed) alone."""
    if np is not None:
        return float(positions.sum()), int(indices.sum())
    return float(sum(value for row in positions for value in row)), int(sum(indices))


def _sha256(data):
    return hashlib.sha256(data).hexdigest()


def _serialize(arrays, mode):
    """Returns (header fields, raw bytes). The raw bytes are written after the header line for `framed`."""
    positions, indices = arrays
    if mode == "in-process":
        # No transfer at all: hash what a native caller would have accessed in place.
        if np is not None:
            pbytes, ibytes = positions.tobytes(), indices.tobytes()
        else:
            import array as _array

            pbytes = _array.array("d", [value for row in positions for value in row]).tobytes()
            ibytes = _array.array("i", indices).tobytes()
        return {"bytes": 0, "positionsBytes": len(pbytes), "indicesBytes": len(ibytes),
                "sha256positions": _sha256(pbytes), "sha256indices": _sha256(ibytes)}, b""

    if mode == "text":
        if np is not None:
            payload = positions.tolist(), indices.tolist()
        else:
            payload = positions, indices
        body = json.dumps(payload, separators=(",", ":"))
        # The array travels inside the JSON message here, which is exactly what the design forbids for scientific data:
        # it is measured so that the rule rests on numbers.
        return {"bytes": 0, "payloadBytes": len(body.encode("utf-8")), "payload": body}, b""

    if mode == "base64":
        import base64

        if np is not None:
            pbytes, ibytes = positions.tobytes(), indices.tobytes()
        else:
            import array as _array

            pbytes = _array.array("d", [value for row in positions for value in row]).tobytes()
            ibytes = _array.array("i", indices).tobytes()
        body = json.dumps({"positions": base64.b64encode(pbytes).decode("ascii"),
                           "indices": base64.b64encode(ibytes).decode("ascii")})
        return {"bytes": 0, "payloadBytes": len(body.encode("utf-8")), "payload": body}, b""

    if mode == "framed":
        if np is not None:
            pbytes, ibytes = positions.tobytes(), indices.tobytes()
        else:
            import array as _array

            pbytes = _array.array("d", [value for row in positions for value in row]).tobytes()
            ibytes = _array.array("i", indices).tobytes()
        payload = pbytes + ibytes
        return {"bytes": len(payload), "positionsBytes": len(pbytes), "indicesBytes": len(ibytes),
                "sha256positions": _sha256(pbytes), "sha256indices": _sha256(ibytes)}, payload

    if mode == "shared-memory":
        if shared_memory is None or os.name != "posix":
            raise ValueError("shared memory is not available on this platform")
        if np is None:
            raise ValueError("shared memory transfer requires numpy in this spike")
        # Keep the names short: macOS limits POSIX shared-memory names to 31 characters.
        global _shm_counter
        _shm_counter += 1
        name = "ovito_spk_{}_{}".format(os.getpid(), _shm_counter)
        pbytes, ibytes = positions.tobytes(), indices.tobytes()
        segment = shared_memory.SharedMemory(name=name, create=True, size=len(pbytes) + len(ibytes))
        segment.buf[: len(pbytes)] = pbytes
        segment.buf[len(pbytes):] = ibytes
        _shm_segments.append(segment)
        return {"bytes": 0, "sharedMemory": {"name": name, "positionsBytes": len(pbytes), "indicesBytes": len(ibytes),
                                             "sha256positions": _sha256(pbytes), "sha256indices": _sha256(ibytes)}}, b""

    raise ValueError("unknown transfer mode: {}".format(mode))


def _release_segments():
    """Unlinks every segment but the newest one: the caller has mapped it by the time the next request arrives."""
    while len(_shm_segments) > 1:
        segment = _shm_segments.pop(0)
        try:
            segment.close()
            segment.unlink()
        except BaseException:  # noqa: BLE001 - a segment the caller already removed is not an error here
            pass


def _close_all_segments():
    """Unlinks everything at shutdown, so the worker leaves no resource behind (multiprocessing reports leaks on exit)."""
    while _shm_segments:
        segment = _shm_segments.pop(0)
        try:
            segment.close()
            segment.unlink()
        except BaseException:  # noqa: BLE001
            pass


def _op_handshake(request):
    return {
        "ok": True,
        "implementation": getattr(getattr(sys, "implementation", None), "name", "unknown"),
        "version": "{}.{}.{}".format(*sys.version_info[:3]),
        "executable": sys.executable,
        "pid": os.getpid(),
        "numpy": None if np is None else np.__version__,
        "features": _features(),
        "transferModes": list(TRANSFER_MODES),        "protocolVersion": HANDSHAKE_PROTOCOL_VERSION,
        "handshake": HANDSHAKE_NAME,
    }


def _op_process(request):
    n = int(request.get("particles", 0))
    faces = int(request.get("faces", 0))
    seed = float(request.get("seed", 0))
    mode = request.get("transfer", "none")
    if mode not in TRANSFER_MODES:
        raise ValueError("unknown transfer mode: {}".format(mode))
    if n < 0 or faces < 0:
        raise ValueError("particles and faces must not be negative")

    event = _cancelled.get(request.get("id"))
    started = time.perf_counter()
    if mode == "none":
        # The compute baseline: build nothing, sum nothing, only pay for the arithmetic an evaluation would do.
        total = (float(n * (n - 1)) / 2.0) * 1.5 + float(n) * seed
        compute = time.perf_counter() - started
        return {"ok": True, "computeMs": compute * 1000.0, "checksum": total, "indicesChecksum": None,
                "bytes": 0, "mode": mode}, b""

    positions, indices = _make_data(n, faces, seed)
    if event is not None and event.is_set():
        return {"ok": False, "error": {"code": "cancelled", "message": "the operation was cancelled"}}, b""
    checksum, indices_checksum = _checksums(positions, indices)
    computed = time.perf_counter()
    header, payload = _serialize((positions, indices), mode)
    serialized = time.perf_counter()
    _release_segments()

    response = {
        "ok": True,
        "checksum": checksum,
        "indicesChecksum": indices_checksum,
        "computeMs": (computed - started) * 1000.0,
        "serializeMs": (serialized - computed) * 1000.0,
        "mode": mode,
    }
    response.update(header)
    return response, payload


def _op_sleep(request):
    """A long operation that only ends when it is cancelled or its time is up: the cancellation seam to measure."""
    seconds = float(request.get("seconds", 1.0))
    step = 0.02
    event = _cancelled.get(request.get("id"))
    deadline = time.perf_counter() + seconds
    while time.perf_counter() < deadline:
        if event is not None and event.is_set():
            return {"ok": False, "error": {"code": "cancelled", "message": "the operation was cancelled"}}, b""
        time.sleep(min(step, 0.002))
    return {"ok": True, "sleptMs": seconds * 1000.0}, b""


#: The numeric types the bridge can move, with the `array` module's type code and the element size in bytes. The
#: vocabulary is fixed by `PythonDataBridge` on the C++ side; an unknown name is refused rather than guessed at.
BRIDGE_DTYPES = {
    "float64": ("d", 8),
    "float32": ("f", 4),
    "int64": ("q", 8),
    "int32": ("i", 4),
    "uint32": ("I", 4),
    "int8": ("b", 1),
    "uint8": ("B", 1),
}


def _describe_array(name, dtype, shape, data):
    """One output descriptor: what the caller needs to rebuild the array it is being handed."""
    return {"name": name, "dtype": dtype, "shape": list(shape), "bytes": len(data), "sha256": _sha256(data)}


def _op_arrays(request):
    """The data bridge: arrays in, arrays out, over the framed transfer.

    This is the first *writable* bridge between OVITO and the interpreter, and it is deliberately narrow. It shows the
    three rules the design fixes for the seam, and it shows them with data the caller can verify:

      * **Byte-exact in both directions.** Every input descriptor carries the sender's SHA-256, which is checked here,
        and the response reports the hash of what actually arrived - so a caller can prove its bytes were not
        reinterpreted on the way in, not only that the answer is well formed.
      * **Copies, never shared memory.** The bytes arrive in this process's own buffer; nothing the caller owns is
        reachable from here, so neither side can change the other's data or outlive it. (The shared-memory transport
        measured by the topology spike is an optimisation *of the same API*, not a different one - and it is not part of
        this op, because a segment per evaluation is exactly what the spike showed is not worth it.)
      * **A deterministic result.** `scale` multiplies a float64 array by a factor and echoes the remaining input
        arrays byte for byte, so the caller can predict the output exactly. A real pipeline evaluation returns what a
        modifier produced; this one exists to be checkable.
    """
    descriptors = request.get("arrays")
    payload = request.get("payload") or b""
    if not isinstance(descriptors, list) or not descriptors:
        raise ValueError("the request must describe at least one array")
    operation = request.get("operation", "scale")
    if operation not in ("scale", "identity"):
        raise ValueError("unknown operation: {}".format(operation))
    factor = float(request.get("factor", 1.0))

    arrays = []
    received = {}
    offset = 0
    for descriptor in descriptors:
        name = descriptor.get("name")
        dtype = descriptor.get("dtype")
        shape = descriptor.get("shape") or []
        if not name:
            raise ValueError("an array was sent without a name")
        if dtype not in BRIDGE_DTYPES:
            raise ValueError("the array \"{}\" has the unsupported type {}".format(name, dtype))
        code, element_size = BRIDGE_DTYPES[dtype]
        elements = 1
        for dimension in shape:
            elements *= int(dimension)
        needed = elements * element_size
        if int(descriptor.get("bytes", needed)) != needed:
            raise ValueError('the array "{}" declares {} bytes but its shape needs {}'.format(name, descriptor.get("bytes"), needed))
        data = payload[offset:offset + needed]
        if len(data) != needed:
            raise ValueError('the payload is too short for the array "{}"'.format(name))
        offset += needed
        announced = descriptor.get("sha256")
        digest = _sha256(data)
        if announced and announced != digest:
            raise ValueError('the array "{}" arrived changed ({} instead of {})'.format(name, digest, announced))
        received[name] = {"dtype": dtype, "shape": list(shape), "bytes": needed, "sha256": digest}
        arrays.append((name, dtype, code, element_size, list(shape), data))

    if offset != len(payload):
        raise ValueError("the payload has {} bytes more than the described arrays".format(len(payload) - offset))

    outputs = []
    blocks = []
    if operation == "scale":
        first = arrays[0]
        name, dtype, code, element_size, shape, data = first
        if dtype != "float64":
            raise ValueError('the array "{}" to scale must be float64, not {}'.format(name, dtype))
        import array as _array

        values = _array.array("d")
        values.frombytes(data)
        for index in range(len(values)):
            values[index] = values[index] * factor
        scaled_bytes = values.tobytes()
        outputs.append(_describe_array(name + "_scaled", dtype, shape, scaled_bytes))
        blocks.append(scaled_bytes)
        # The remaining arrays come back exactly as they arrived, which is what makes the transfer verifiable: the
        # caller compares bytes it sent with bytes it received.
        for entry in arrays[1:]:
            echoed = _describe_array(entry[0] + "_echo", entry[1], entry[4], entry[5])
            outputs.append(echoed)
            blocks.append(entry[5])
    else:
        for entry in arrays:
            echoed = _describe_array(entry[0] + "_echo", entry[1], entry[4], entry[5])
            outputs.append(echoed)
            blocks.append(entry[5])

    payload_out = b"".join(blocks)
    return {
        "ok": True,
        "arrays": outputs,
        "bytes": len(payload_out),
        "operation": operation,
        "factor": factor,
        "inputSha256": received,
        "elements": sum(int(descriptor["bytes"]) // BRIDGE_DTYPES[descriptor["dtype"]][1] for descriptor in outputs),
    }, payload_out


def _execute(request):
    """Runs one request and returns (response, raw payload) - the payload is written after the header line."""
    op = request.get("op")
    if op == "handshake":
        return _op_handshake(request), b""
    if op == "ping":
        return {"ok": True, "pid": os.getpid()}, b""
    if op == "arrays":
        return _op_arrays(request)
    if op == "process":
        return _op_process(request)
    if op == "sleep":
        return _op_sleep(request)
    if op == "crash":
        # A deliberate hard exit: the caller measures how fast it notices a dead worker and how well it recovers.
        sys.stdout.flush()
        os._exit(3)
    raise KeyError("unknown operation: {}".format(op))


def _error_response(code, message):
    return {"ok": False, "error": {"code": code, "message": message}}


def _reader(queue, lock):
    """Reads requests. `cancel` is handled here and nowhere else, so it reaches a running operation.

    Everything else is queued for the single execution thread, which is what makes this worker serial: an operation
    never runs concurrently with another one, exactly as the interpreter on the other side of the pipe requires. The
    cancellation flag is registered here, before the request is queued, so a cancel that arrives while the request is
    still waiting cannot be lost.

    Reading happens on the *binary* stream, because a request may announce a raw payload that follows its header line:
    the array bridge sends its bytes that way, and a text-oriented reader would eventually mistake array bytes for a
    message. The payload is read here, before the request is queued, so the execution thread never touches the stream.
    """
    stream = sys.stdin.buffer
    while True:
        raw = stream.readline()
        if not raw:
            return                      # the caller closed the pipe: the worker ends with the loop below
        line = raw.strip()
        if not line:
            continue
        try:
            request = json.loads(line)
        except BaseException as exc:  # noqa: BLE001
            # A header that cannot be parsed leaves the stream unusable if a payload was announced, which is why the
            # caller (OVITO) never builds one: this is a defect report, not a supported way to talk to the worker.
            with lock:
                sys.stdout.write(json.dumps(_error_response("invalid_argument", str(exc))) + "\n")
                sys.stdout.flush()
            continue
        payload_size = int(request.get("bytes") or 0)
        if payload_size:
            request["payload"] = stream.read(payload_size)
        if request.get("op") == "cancel":
            event = _cancelled.get(request.get("target"))
            if event is not None:
                event.set()
            with lock:
                sys.stdout.write(json.dumps({"id": request.get("id"), "ok": True, "cancelled": request.get("target")}) + "\n")
                sys.stdout.flush()
            continue
        _cancelled[request.get("id")] = threading.Event()
        queue.put(request)


def main():
    import queue as _queue

    requests = _queue.Queue()
    lock = threading.Lock()
    reader = threading.Thread(target=_reader, args=(requests, lock), daemon=True)
    reader.start()

    while True:
        # Blocking, not polling: a worker that sleeps between requests would add its poll interval to every round trip,
        # and the control latency is one of the numbers this spike exists to measure.
        request = requests.get()
        request_id = request.get("id")
        if request.get("op") == "quit":
            _close_all_segments()
            with lock:
                sys.stdout.write(json.dumps({"id": request_id, "ok": True, "quitting": True}) + "\n")
                sys.stdout.flush()
            # os._exit, not a return: the reader thread is blocked in a read on standard input, and letting the
            # interpreter finalize while that thread holds the buffered-reader lock aborts the process
            # ("Fatal Python error: _enter_buffered_busy ... possibly due to daemon threads") - which a caller sees as a
            # worker that had to be killed after it answered. The answer is flushed and the segments are unlinked, so
            # there is nothing left to finalize.
            os._exit(0)
        started = time.perf_counter()
        try:
            response, payload = _execute(request)
        except KeyError as exc:
            response, payload = _error_response("unknown_operation", str(exc)), b""
        except ValueError as exc:
            response, payload = _error_response("invalid_argument", str(exc)), b""
        except BaseException as exc:  # noqa: BLE001 - a user-code failure must not take the worker down
            response, payload = _error_response("internal_error", "{}: {}".format(type(exc).__name__, exc)), b""
        _cancelled.pop(request_id, None)
        response["id"] = request_id
        response["wallMs"] = (time.perf_counter() - started) * 1000.0
        with lock:
            sys.stdout.write(json.dumps(response) + "\n")
            sys.stdout.flush()
            if payload:
                # The framed transfer: exactly the announced number of bytes, immediately after the header line.
                sys.stdout.buffer.write(payload)
                sys.stdout.buffer.flush()


if __name__ == "__main__":
    sys.exit(main())
