# Python Execution Topology Spike (Phase 2.6, deliverable 6)

**Status: executed and recorded.** This document holds the evidence behind the Phase 2.6 decision on how OVITO runs
user Python: a persistent worker process on a user-selected interpreter, or an embedded runtime linked into the
application. The prototype that produced the numbers is `src/ovito/core/automation/spike/` (`ovito-automation-spike`)
together with the worker it drives, `src/ovito/core/automation/python/ovito_worker.py`. The decision it supports is
recorded as **D49** in [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md) §7 and summarized in
[AUTOMATION_AND_PYTHON_PIPELINE_DESIGN.md](AUTOMATION_AND_PYTHON_PIPELINE_DESIGN.md) §6 item 1.

The question, from the design: *an embedded runtime gives direct access to native OVITO objects but couples the
application to one CPython ABI and needs careful GIL/thread integration; a persistent worker lets OVITO launch the
selected environment and isolates crashes, but needs an efficient bridge for large arrays, cancellation and errors.*
Neither topology may be frozen until "one representative particle/mesh benchmark and failure-recovery test pass".

---

## 1. What was measured, and how

The spike measures the **seam**: what one pipeline evaluation costs when the interpreter is a separate process. It
compares that against two baselines that an embedded runtime would also have to pay:

| Mode | What it does | Why it is in the comparison |
|---|---|---|
| `none` | Computes the arithmetic result only, builds no array | The pure compute cost with no data path at all |
| `in-process` | Builds the full particle and mesh arrays and hashes them, without serializing anything | **The cost model of an embedded runtime's data path**: compute plus in-memory access to the arrays |
| `framed` | Same arrays, sent as a length-prefixed raw little-endian buffer after the JSON header line | The design's candidate transport (no JSON, no pickle) |
| `shared-memory` | Same arrays in a POSIX shared-memory segment the client maps | The alternative transport for large arrays |
| `base64` | Same arrays base64-encoded inside the JSON message | The middle ground the design also rejects |
| `text` | Same arrays as JSON numbers | The documented anti-pattern, measured so the rule rests on numbers |

Every array-bearing mode is **verified, not trusted**: all modes must report the same checksum for the same input, the
client recomputes the SHA-256 of what it received and compares it with the worker's report, the framed bytes and the
shared-memory bytes must be identical, and the first values decoded from the JSON and base64 forms must equal the first
values of the raw form. The client also checks the formula of the first particle directly. 93 (numpy run) respectively
89 (no-numpy run) such checks pass in the recorded runs; a check failure makes the spike exit non-zero.

The payload is a particle position array (float64, 500 000 × 3 = 11.4 MB) plus a triangle index array (int32,
200 000 × 3 = 2.3 MB), i.e. 13.7 MB of raw scientific data per evaluation. Sizes are `--particles` / `--faces`.

### Environment of the recorded runs

| | |
|---|---|
| Host | Linux Mint 22.3, x86_64, AMD Ryzen 9 9950X (16 cores / 32 threads), 46 GB RAM |
| Build | `cmake --preset native` (Clang 18, `-O2`, `build-native`), uncommitted Phase 2.6 working tree |
| Interpreter A | `.venv-automation/bin/python` — CPython 3.12.13 with numpy 2.5.3, created with `uv venv` (see §6) |
| Interpreter B | `/usr/bin/python3` — CPython 3.12.3, **no numpy**, so the worker runs its pure-Python data path |
| Worker startup | `python -I <worker script>`: isolated, no user site, no `PYTHON*` environment variables |

The same binary and the same worker script produced both runs:

```bash
LD_LIBRARY_PATH="$PWD/.qt/6.10.2/gcc_64/lib:$PWD/build-native/lib/ovito/plugins" \
  ./build-native/bin/ovito-automation-spike --python .venv-automation/bin/python --json /tmp/spike_numpy.json
LD_LIBRARY_PATH="$PWD/.qt/6.10.2/gcc_64/lib:$PWD/build-native/lib/ovito/plugins" \
  ./build-native/bin/ovito-automation-spike --python /usr/bin/python3 --iterations 3 --json /tmp/spike_plain.json
```

---

## 2. Results — CPython 3.12.13 + numpy 2.5.3

500 000 particles + 200 000 triangles, 5 samples per mode (50 for the control round trip).

| Measurement | min | median | max | Note |
|---|---|---|---|---|
| Startup (spawn + handshake) | 39.4 ms | **39.7 ms** | 40.7 ms | 5 runs |
| Control round trip (`ping`) | 0.00 ms | **0.013 ms** | 0.03 ms (p95 0.016) | 50 round trips |
| Evaluation, `none` (compute only) | 0.02 ms | **0.02 ms** | 0.03 ms | no payload |
| Evaluation, `in-process` | 10.4 ms | **10.8 ms** | 13.0 ms | 13.7 MB built + hashed, no transfer |
| Evaluation, `framed` | 8.4 ms | **13.0 ms** | 13.8 ms | payload 13.7 MB |
| Evaluation, `shared-memory` | 12.8 ms | **13.3 ms** | 14.4 ms | payload 13.7 MB |
| Evaluation, `base64` | 97.3 ms | **98.9 ms** | 99.9 ms | payload 18.3 MB (4/3 of raw) |
| Evaluation, `text` | 537.1 ms | **539.2 ms** | 551.2 ms | payload 31.6 MB (2.3× raw) |
| Evaluation per animation frame | 3.0 ms | **3.0 ms** | 6.6 ms | 10 frames, 200 000 particles |
| Cancellation latency | — | **1.3 ms** | — | from sending `cancel` to the `cancelled` answer |
| Crash noticed | — | **4.7 ms** | — | client sees the dead worker instead of a timeout |
| Recovery (restart + handshake) | — | **43.0 ms** | — | one restart, then the worker is usable |

Worker-side time inside those round trips (from the worker's own report, median): `computeMs` 1.6 ms and `serializeMs`
6.7 ms for `framed`; `computeMs` 2.5 ms and `serializeMs` 410 ms for `text`; `serializeMs` 41 ms for `base64`.

## 3. Results — CPython 3.12.3 without numpy

Same payload and worker, 3 samples per mode. Without numpy the worker builds the arrays as Python lists, which dominates
everything else; the comparison between transfer modes is therefore only meaningful in §2.

| Measurement | median | Note |
|---|---|---|
| Startup (spawn + handshake) | **16.1 ms** | the distribution's interpreter starts faster than the uv-managed one |
| Control round trip (`ping`) | **0.010 ms** | 50 round trips |
| Evaluation, `in-process` | **226.5 ms** | pure-Python array construction dominates |
| Evaluation, `framed` | **200.6 ms** | payload 13.7 MB — the transfer hides inside the compute cost |
| Evaluation, `base64` | **304.9 ms** | payload 18.3 MB |
| Evaluation, `text` | **674.3 ms** | payload 31.6 MB |
| Evaluation per frame | **65.2 ms** | 10 frames, 200 000 particles |
| Cancellation latency | **3.9 ms** | |
| Crash noticed / recovery | **4.4 ms** / **19.3 ms** | |
| `shared-memory` | skipped | the worker does not advertise the mode without numpy |

---

## 4. Interpretation

1. **The seam is cheap.** For 13.7 MB per evaluation, `framed` costs 13.0 ms against the 10.8 ms in-process baseline:
   the whole inter-process data path adds about **2 ms (+20 %)** on top of computing and preparing the data. The control
   channel is a **10 µs** round trip and cancellation lands in **1–4 ms**. That is far inside an interactive budget, so
   the concern that a worker "cannot meet interactive pipeline latency" is not supported by the measurement.
2. **JSON and base64 must not carry arrays.** `text` is **41×** slower than `framed` and **2.3×** larger; `base64` is
   **7.6×** slower for **1.3×** the size. This is the empirical form of the design rule "JSON may describe schemas and
   small control messages; it should not carry routine full particle arrays".
3. **Framed raw beats a per-evaluation shared-memory segment** (13.0 ms vs 13.3 ms) at this size, because the segment
   pays creation, mapping, a copy-out on each side and unlinking, while a pipe streams the bytes once. The difference is
   small and this spike's shared-memory path is deliberately simple; see the limitations in §5. The conclusion is that
   the shared-memory path is an *optimisation behind the same API*, not a requirement.
4. **A worker is much cheaper than one interpreter per evaluation**, which is the alternative a worker competes with:
   startup is 16–40 ms, i.e. every evaluation would pay it, against 10 µs of control traffic and 2 ms of transfer in a
   persistent worker.
5. **Crash isolation is real and affordable.** A worker that dies on purpose is noticed in ~4.7 ms (the client reports
   the exit code instead of waiting for a timeout) and replaced in ~43 ms / ~19 ms, without the application having to
   survive the crash itself.
6. **numpy availability changes the compute cost by ~20×** (10.8 ms vs 226.5 ms for the same payload), not the transfer
   cost. The package contract therefore has to be honest about it per feature (`array.buffer`,
   `array.shared-memory`) rather than making numpy a hard requirement of the decorator contract.

## 5. Decision

**A persistent worker process on the user-selected interpreter is the chosen execution topology**, with a raw
length-framed binary transfer for arrays as the default and shared memory as an optional optimisation behind the same
package API. An embedded runtime is not selected: the measurement bounds what embedding could save (the ~2 ms transfer
and the ~10 µs control round trip of a 13.7 MB evaluation) and embedding would additionally couple the application to one
CPython ABI and give up the crash isolation the worker demonstrates. As the design requires, the *seam* stays open:
the package-level API is the same either way, so an embedded backend can be added later for a fixed, supported ABI
without changing user Python.

Consequences for the later phases (recorded in the placement table of
[UI_PLAN.md](UI_PLAN.md) §Phase 2.6 and in the audit decision D49):

- Phase 4 implements the package's worker entry point on this protocol: JSON Lines for control, one length-prefixed raw
  buffer for arrays, cooperative cancellation through a control message, and explicit artifact/error messages;
- the transport is verified by the same checks the spike performs (identical bytes across paths, recomputed hashes,
  predictable values), because a bridge that silently transfers the wrong data would be worse than a slow one;
- the shared-memory path is only worth implementing with a **pooled segment** and a zero-copy read; the simple
  per-evaluation version measured here is not faster than the pipe;
- JSON is limited to control metadata and small values; a text transfer of scientific data is a defect, not a fallback;
- a worker restart must be a supported, user-visible event (Phase 4 status/traceback UI), since the measurement shows
  recovery is cheap enough to be automatic but not invisible.

## 6. Limitations, and what is deliberately not claimed

- **An embedded runtime was not measured.** It needs a fixed CPython ABI and `Python.h` at build time, which the
  migration does not have for the four target platforms. What the spike can state, and does state, is that the part an
  embedded runtime would save is the small one (§4.1) — not that an embedded runtime would be equally fast or slow in
  general; its cost lies in GIL/thread integration and ABI coupling, which this spike does not quantify.
- **The single-segment shared-memory path** creates one segment per evaluation and copies out of the mapping (the client
  materializes the bytes into a buffer). A pooled segment with a zero-copy view is the production shape and was not
  measured; the recorded number is therefore an upper bound for that path.
- **The payload is synthetic**: two dense, contiguous arrays with a predictable layout. Structured per-type particle
  properties, non-contiguous selections, meshes with multiple per-face arrays and string properties will change the
  transfer profile, and Phase 4 must extend the benchmark to them before the bridge is called finished.
- **No GPU/rendering interaction and no GUI thread** was involved: the spike runs the worker from a plain
  `QCoreApplication`, and the question of running Python off the GUI thread (with OVITO's task system and the session's
  task records) is Phase 4 work.
- **One machine, one platform, one OS version.** The absolute numbers are those of the host in §1; the ratios and the
  failure behaviour are what the decision uses. macOS (Apple Silicon) and Windows (AMD64) runs remain unverified, as the
  migration's test environment rules require stating.
- **The spike is not production code.** `WorkerProcess` is a blocking, single-threaded client written for measurement;
  Phase 4 owns the real one (asynchronous, integrated with the task/event/cancellation model of the automation layer).
- **The `.venv-automation` environment is a measurement fixture**, not a supported configuration: it exists so that the
  numpy and no-numpy cases could both be recorded. Nothing in OVITO creates or modifies it.

## 7. Reproducing

```bash
# Optional: an interpreter with numpy, for the comparison in §2 (needs uv).
uv venv .venv-automation --python 3.12
uv pip install --python .venv-automation/bin/python numpy

# Build the spike (native preset) and run it against both interpreters.
cmake --build --preset native --target OvitoAutomationSpike
LD_LIBRARY_PATH="$PWD/.qt/6.10.2/gcc_64/lib:$PWD/build-native/lib/ovito/plugins" \
  ./build-native/bin/ovito-automation-spike --python .venv-automation/bin/python --json /tmp/spike_numpy.json
LD_LIBRARY_PATH="$PWD/.qt/6.10.2/gcc_64/lib:$PWD/build-native/lib/ovito/plugins" \
  ./build-native/bin/ovito-automation-spike --python /usr/bin/python3 --json /tmp/spike_plain.json

# Fast pass for a smoke check, and a narrow one for a single transport.
./build-native/bin/ovito-automation-spike --quick
./build-native/bin/ovito-automation-spike --modes none,framed --iterations 3
```

Exit codes: `0` all checks passed, `1` a check failed (measurements still print), `2` there was no interpreter to
measure, so a caller can tell a failure from a skip. Without `--python` the spike probes the first `python3` on the path
through `PythonEnvironmentProbe::findInterpreter()`, and it prints the verdict of the environment probe of deliverable 5
next to the numbers, so a recorded run always says which environment it belongs to.
