# The Python Data Bridge and the Schema Preview (Phase 2.6, Deliverable 7)

> **Status**: Executed and verified. This is the evidence document of Phase 2.6 deliverable 7 — the AST-only schema
> preview (7a) and the first writable data bridge between OVITO and an external interpreter (7b) — and the basis of
> audit decision **D51** in [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md) §7. It follows the structure of
> [AUTOMATION_TOPOLOGY_SPIKE.md](AUTOMATION_TOPOLOGY_SPIKE.md) and [AUTOMATION_IPC_SPIKE.md](AUTOMATION_IPC_SPIKE.md).

---

## 1. What was built

Deliverable 7 asks for two things that answer two different questions about running user Python, and the deliverable
splits along that line:

| Half | Question | Artifact |
|---|---|---|
| 7a | *What does a user's file contain,* without running it? | `automation/python/ovito_schema.py`, `PythonSchemaPreview`, `PythonIntrospector`, `tst_python_schema_preview` |
| 7b | *How do arrays get in and out of it safely?* | `automation/python/ovito_worker.py` (the `arrays` operation), `PythonWorkerProtocol`, `PythonWorkerProcess`, `PythonDataBridge`, `tst_python_data_bridge` |

All of it lives in Core (`src/ovito/core/automation/python/`), takes part in both the release and the assertion-enabled
test run, and none of it inserts anything into a pipeline: this deliverable proves the seam, and the Python Function
Modifier that uses the seam is Phase 4 (design §5, [UI_PLAN.md](UI_PLAN.md) Phase 2.6).

One consequence of the split is worth stating up front, because it is the reason 7a and 7b are separate at all: **a
preview and a run are different promises.** A preview that executed the file to find out what it contains would be easier
to write and would break the rule the design fixes in §5 (a file the user has only selected must not run). The AST mode
is therefore the default everywhere, the import mode has to be asked for explicitly, and the test suite proves the
difference with a fixture whose top level writes a marker file.

## 2. The schema preview (7a)

`ovito_schema.py` reads one Python file with `ast` only and prints exactly one JSON object: the module docstring, the
imports, one entry per top-level function (name, line, docstring, decorators, status, parameters) and a list of
diagnostics. It exits 0 whenever it printed a report — a syntax error is a *report*, not a failing process, because the
caller needs the position of the error and not just its exit code.

The rules it implements, each pinned by a test case:

| Rule | Behaviour | Case |
|---|---|---|
| Nothing runs | The AST mode never imports, compiles or executes the module; a top-level side effect does not happen | `preview_reads_a_modifier_without_running_the_script`, `preview_imports_the_script_only_when_asked` |
| Malformed source is reported, not guessed | A `SyntaxError` becomes one `syntax_error` diagnostic with line, column, end position and the offending source text; the function list is empty and no function is invented | `preview_reports_a_syntax_error_with_its_position` |
| A decorator that is computed is not a decorator we know | `@registry[key]` or `@pick(make_name())` are reported as `dynamic_decorator_argument` (`Unsupported` for the first, a warning for the second); the report never claims a schema it could not read | `preview_reports_a_decorator_that_is_computed_at_run_time`, `preview_reports_a_decorator_argument_that_is_computed_at_run_time` |
| Annotations stay text | An annotation is the source text the author wrote; resolving it would mean executing the file | `preview_reads_a_modifier_without_running_the_script` |
| A missing file is a verdict | `unreadable_file` names the path; the caller can tell "your path is wrong" from "your file is broken" | `preview_reports_a_missing_file` |
| Import mode is explicit and mapped | Only `--mode import` executes the file; a failure becomes `import_failed` with a traceback whose frames carry file/function/line and a `fromUserScript` flag, and a traceback line in the user's file is also the diagnostic's line | `preview_maps_a_traceback_of_the_user_script` |
| An answer that is not a report is refused | A reply without the expected `schemaVersion` is a `protocol_error`, not a report read leniently | `preview_reports_an_answer_that_is_not_a_report` |
| The exchange has its own failures | No interpreter, no script, a timeout and a cancellation each produce one diagnostic (`interpreter_missing`, `script_missing`, `timeout`, `cancelled`), and a cancelled run kills its interpreter instead of leaving it behind | four cases in `tst_python_schema_preview` |

The interpreter is started with `-I`, like the environment probe, so the script's own directory is not on `sys.path`; the
schema module's path is passed explicitly. That is also why the probe and the schema reader are two separate programs:
`ovito_probe.py` answers "may this environment be used at all", `ovito_schema.py` answers "what is in this file".

## 3. The worker as a contract (7b)

The `arrays` operation turns `ovito_worker.py` from a spike artifact into the reference implementation of a protocol, so
the parts that a second implementation has to agree on were pulled out of the script and into Core:

* **`PythonWorkerProtocol`** holds the operation names (`handshake`, `arrays`, `cancel`, `quit`), the worker's error
  vocabulary (`unknown_operation`, `invalid_argument`, `unsupported_transfer`, `cancelled`, `internal_error`) and the
  client's own (`worker_unavailable`, `protocol_error`) — two disjoint vocabularies, so a caller can tell "your request
  was wrong" from "the worker is gone". The envelope has no version of its own: the handshake names the package protocol
  of D48 (`ovito.automation.handshake`, version `1.0`) and its **major** number is the compatibility gate for the framing
  as well.
* **`PythonWorkerProcess`** is the client, and it is Core code rather than spike code because Phase 4 reuses it. It
  starts the caller's interpreter (isolated with `-I` unless told otherwise, never searching for one), writes the header
  and its payload as one buffer, reads exactly the announced payload bytes before handing a reply out, reports a dead
  interpreter as `worker_unavailable` with its exit code and the tail of its standard error instead of waiting for a
  timeout, keeps at most one request outstanding, and lets a caller overtake a running request with `cancel()` while
  still receiving the answer of the cancelled one.
* **`PythonWorkerHandshake`** parses what the worker says about itself: the protocol identity, the implementation and
  version, the pid, the numpy availability, the features and the transfer modes. A feature this build does not know is
  kept (`unknownFeatures`) rather than dropped, and a worker whose name or major version differs is refused. This is the
  *runtime* handshake: the *package* handshake of `PythonHandshake` remains the authoritative compatibility check of an
  installation, performed by `PythonEnvironmentProbe` before anything runs.

## 4. The bridge: what makes the bytes trustworthy

`PythonDataBridge` defines `PythonArray` (name, type, shape, bytes, announced digest) and `PythonArrayBlock` (a set of
them), and `PythonArrayBridge::exchange()` is the round trip. The rules are the deliverable:

1. **Framed, never JSON.** The descriptors travel as JSON (name, type, shape, byte count, digest); the values travel as
   one raw payload after the header line, in both directions. The topology spike measured what the alternative costs
   (text: 539 ms for 31.6 MB against 13.0 ms framed for 13.7 MB), and the bridge is the same path.
2. **Copies, never shared memory.** A request is built from a copy of the caller's bytes and a reply arrives in its own
   buffer, so nothing in this layer hands an address of OVITO's memory to another process and neither side can change the
   other's data. The test asserts that the caller's block is byte-identical and its digest unchanged after an exchange.
3. **Both directions are proven.** Each descriptor carries a SHA-256 that the receiver checks, and the reply reports the
   digests *the worker received* (`inputSha256`), which is the half the topology spike never measured. A mismatch is a
   failure, not an array to compute with.
4. **A description that cannot be honoured is refused, with the array named.** An unknown type, a shape that disagrees
   with the byte count, a negative dimension, a duplicate name, a payload that is short or long, and a reply whose
   announced byte count disagrees with its descriptors are all errors that name the array they are about, on both sides
   of the seam. The suite includes one case that builds a descriptor by hand precisely so that the *worker's* defence is
   tested rather than the client's.
5. **No stride in the vocabulary.** A strided source is copied into a contiguous block by the caller. A stride the wire
   cannot express would be worse than the copy.
6. **One representation.** The bytes are the host's native representation. Every targeted platform is little-endian and
   the worker runs on the same machine, so nothing is converted; a big-endian host or a remote worker is not a supported
   combination of this phase and would have to fix a byte order here first.

The computation of the `arrays` operation is deliberately trivial and predictable: `scale` multiplies the first `float64`
array by a factor and echoes the remaining arrays byte for byte, `identity` only echoes. That is what makes the transfer
checkable — the suite compares the echoed bytes with what it sent and the scaled values with what it can compute itself —
without pretending that this is the Python Function Modifier.

## 5. Results

Both suites pass everywhere the phase is verified:

| Suite | Release build (`native`) | Assertion-enabled (`build-asserts`) |
|---|---|---|
| `tst_python_schema_preview` | 17 passed / 0 failed | 17 passed / 0 failed |
| `tst_python_data_bridge` | 18 passed / 0 failed | 18 passed / 0 failed |

`ctest --preset native` runs 11 test executables after this deliverable (it was 9), and the full native product still
builds with no new warning.

The one measurement the bridge contributes is the case that a pipe cannot hide: 8 MB out and 8 MB back in one exchange
(a million `float64` values, system CPython 3.12.3, three runs):

| Bytes out | Bytes back | Exchange |
|---|---|---|
| 8,000,000 | 8,000,000 | 47.6 ms / 48.3 ms / 49.1 ms |

That number includes the worker's pure-Python scale loop over a million values and the digests on both sides, so it is an
end-to-end figure and not a transfer rate. It matters because the payload is two orders of magnitude larger than a pipe
buffer: a client that assumed its whole message had reached the pipe would deadlock here, and one that started reading
before the payload was written would read its own bytes back as a header line. The case is in the suite for that reason;
it asserts the data, and prints the duration rather than asserting it, because a duration is not a correctness criterion.

## 6. Decision (D51)

**The bridge's contract is a framed byte transfer with copies and digests, the schema preview never executes the file it
reads, and both are Core code.** Consequences:

* Arrays travel as one length-framed raw payload with JSON descriptors; a text or base64 transfer of scientific data is a
  defect, not an option, and the shared-memory path stays an optional optimisation of the same API (D49, O17).
* Ownership is copy-based: no pointer into OVITO's data crosses the process boundary, and an exchange does not modify
  what it was given. The adapter that turns an OVITO property buffer into a `PythonArray` is Phase 4 work; the bridge
  deliberately knows nothing about OVITO's data objects, which is also what makes it testable with no data set.
* A receiver validates before it uses: descriptors against the payload, digests against the bytes, types against the
  vocabulary. An unknown type is refused rather than converted.
* The AST preview is the default and the only mode a file selection may use; the runtime import is explicit, its failures
  are mapped tracebacks, and neither mode ever falls back to another interpreter.
* The worker protocol's operation names, error vocabularies and handshake live in `PythonWorkerProtocol`, and the runtime
  handshake is validated against the package contract instead of a second copy of the names, so Phase 4's installed
  package replaces the script without changing the client.

## 7. Limitations

* **The bridge moves bytes, not OVITO data.** There is no adapter between a `PropertyObject`'s buffers and a
  `PythonArray` yet (recorded as O21), so the ownership rules are proven for owned byte blocks and not for a live pipeline
  buffer.
* **No user code runs in the bridge.** The `arrays` operation is fixed; cancellation, progress, traceback mapping and
  partial-result rules from the design's §3.5 are Phase 4's subject.
* **Bounds instead of streaming.** One request carries at most 512 MB in at most 64 arrays; a larger transfer needs the
  streaming form of a later phase, and exceeds what a single `QByteArray` per side would want to hold anyway.
* **Native representation.** The wire is the host's byte order; a big-endian host is unsupported and would need a
  conversion here (all targeted platforms are little-endian).
* **The schema preview knows one decorator vocabulary.** It recognizes the last dotted component of a decorator name
  against `python_script`/`python_modifier`/`modifier`, and it cannot know what a decorator does with its arguments. The
  authoritative schema is the one the package's decorator computes at runtime, which arrives with Phase 4's package.
* **One machine.** Both suites ran on Linux x86_64 with CPython 3.12.3 (system) and CPython 3.12.13 (the uv venv) as the
  worker interpreter; macOS and Windows runs are Phase 4 work like the rest of O17.
* **The worker is still the reference implementation, not the shipped package.** It lives in the source tree behind the
  build-time path of O16, and Phase 4 replaces it with the installed package's entry point on the same wire.

## 8. Reproducing

```bash
# Both suites, in the release build
LD_LIBRARY_PATH="$(pwd)/.qt/6.10.2/gcc_64/lib:$(pwd)/build-native/lib/ovito/plugins" \
QT_QPA_PLATFORM=offscreen ./build-native/tests/cpp/core/automation/tst_python_schema_preview
LD_LIBRARY_PATH="$(pwd)/.qt/6.10.2/gcc_64/lib:$(pwd)/build-native/lib/ovito/plugins" \
QT_QPA_PLATFORM=offscreen ./build-native/tests/cpp/core/automation/tst_python_data_bridge

# The one measurement of section 5 (it prints the duration of the 8 MB exchange)
LD_LIBRARY_PATH="$(pwd)/.qt/6.10.2/gcc_64/lib:$(pwd)/build-native/lib/ovito/plugins" \
QT_QPA_PLATFORM=offscreen ./build-native/tests/cpp/core/automation/tst_python_data_bridge 2>&1 | grep 'bridge moved'

# The interpreter can be chosen the same way the spike chooses one, e.g. the uv environment of UI_TEST_ENV.md §4.2:
# both suites find their interpreter through PythonEnvironmentProbe::findInterpreter(), which prefers PATH, so run them
# with PATH pointing at the environment whose behaviour is to be checked.

# The schema reader on its own, which is how it is debugged
python3 -I src/ovito/core/automation/python/ovito_schema.py --path some_modifier.py
python3 -I src/ovito/core/automation/python/ovito_schema.py --path some_modifier.py --mode import
```

A test run leaves no worker behind (the suites stop their workers and assert the exit code); after an interrupted run,
`pgrep -fl ovito_worker.py` finds a leftover interpreter, and [UI_TEST_ENV.md](UI_TEST_ENV.md) §4.2 records the rest of
the environment recipe.
