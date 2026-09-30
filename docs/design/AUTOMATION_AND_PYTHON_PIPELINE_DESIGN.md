# Python Pipeline and AI/CLI Automation Design

> **Status**: Accepted design basis for Phase 2.6, and executed: the machine-facing gateway (deliverables 1-4), the Python
> package contract and its runtime probe (deliverable 5), the execution-topology spike (deliverable 6), the Python seam of
> the schema preview and the data bridge (deliverable 7) and the local protocol with its discovery descriptor (deliverable
> 8) are implemented and verified, with their decisions recorded as D39-D51 in [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md) §7
> and the evidence in [AUTOMATION_TOPOLOGY_SPIKE.md](AUTOMATION_TOPOLOGY_SPIKE.md),
> [AUTOMATION_IPC_SPIKE.md](AUTOMATION_IPC_SPIKE.md) and [AUTOMATION_DATA_BRIDGE.md](AUTOMATION_DATA_BRIDGE.md). Sections
> 3.4.1, 6 and 7 below carry the resulting resolutions. Formal feature placement is unchanged: the user-facing
> capabilities remain gated by the later phase exits.
>
> **Scope**: Two related but distinct capabilities for the Qt Quick workbench: user-defined Python computation as part of
> OVITO's data pipeline, and a semantic CLI/automation interface through which a person or AI agent can inspect and operate
> a live OVITO session.

## 1. Intent

The goal is not to reproduce OVITO Pro's Python console or AI Agent feature set. The proposed direction is to combine:

1. **Python as a programmable pipeline node**: a user points OVITO at a Python file, selects a compatible interpreter, and
   OVITO discovers explicitly decorated, typed functions in that file. A selected function can become a reusable pipeline
   computation with parameters presented in the workbench.
2. **AI/CLI as a semantic workbench interface**: a command-line client or AI agent can inspect what the user currently has
   open, construct and configure pipelines, run work, and request outputs such as rendered PNG images. It operates on the
   same live session and domain operations as the GUI rather than simulating mouse and keyboard input.

The two capabilities should share stable descriptions of operations, parameters, objects, tasks, and results, but neither
depends on the other. Python pipeline support must be useful without an AI service; CLI automation must work without Python
being enabled.

## 2. Existing Architecture Constraints

The current OVITO pipeline is already a natural host for Python computation:

- A modifier receives a `ModifierEvaluationRequest` and a `PipelineFlowState` and returns a `Future<PipelineFlowState>`.
- `PipelineFlowState` carries a `DataCollection`, status, and time-validity interval. Its data collection supports
  copy-on-write mutation.
- Modifier evaluation is scheduled by the existing asynchronous task framework, with exceptions translated into pipeline
  status/errors by the evaluation path.
- The QML frontend depends on `gui/base` and `core`; it must not acquire a dependency on `gui/desktop`.
- `gui/base` already has frontend-neutral `Command` objects, but these are currently UI commands, not yet a complete
  machine-facing command/query protocol.
- The checkout does not contain an implemented `src/ovito/pyscript` runtime. A few conditional build hooks and Python
  packaging path variables are not evidence that a usable Python pipeline/runtime exists.

Accordingly, use the core pipeline contract and shared workbench/session state; do not make Python functions QML callbacks,
and do not turn UI `Command` objects directly into an unrestricted remote API.

## 3. Python Pipeline

### 3.1 User workflow

The intended authoring workflow is:

1. Choose a Python interpreter/environment using the Phase 4 modifier workflow's lightweight environment selector.
   It can save an application default without waiting for the complete Phase 7 Preferences dialog.
2. Point the Python-function modifier at an ordinary `.py` file.
3. OVITO previews decorated functions without importing the file; explicit activation imports it in the selected
   interpreter and validates the discovered metadata.
4. Choose a discovered function and add it to the pipeline.
5. OVITO displays the function's typed parameters as editable controls. Editing a parameter invalidates the relevant
   pipeline result and schedules normal reevaluation.
6. The script receives an isolated, writable upstream data state and mutates it according to a small, documented contract.
   Exceptions appear as errors on the Python modifier, not as an unhandled application failure.

There should be no requirement to author inside a built-in console or special editor. An editor may be offered later as a
convenience, but the source of truth remains the user's file. File changes should be detected and the user should be able to
reload the module explicitly or accept a detected update.

### 3.2 Initial node type: Python Function Modifier

Start with a single modifier type that represents one decorated function. It operates on the upstream data state and
produces a downstream data state. A source that creates data from scratch has different semantics and should be considered
later as a separate node type.

Illustrative API (names are provisional):

```python
import ovito
from ovito.data import DataCollection

@ovito.modifier
def add_distance(data: DataCollection, scale: float = 1.0) -> None:
    """Add distance from the origin as a particle property."""
    positions = data.particles.positions
    data.particles_.create_property(
        "DistanceFromOrigin",
        data=(positions ** 2).sum(axis=1) ** 0.5 * scale,
    )
```

This sample illustrates intent, not a committed Python API. The first modifier contract should be deliberately narrow:
OVITO passes a writable pipeline-data copy and the function mutates it in place, returning `None`. Do not initially infer
whether a function is a modifier, source, or analysis operation from its return value, and do not accept several alternative
return conventions. A future `@ovito.source` or `@ovito.analysis` decorator can define a different explicit contract.
Reject a non-`None` return value with a contextual pipeline error until another function kind is supported.

The decorator should attach discoverable metadata, not execute or register a GUI callback. Function discovery should have two
stages. First, OVITO may statically inspect the file with Python AST tooling to list decorated functions, signatures, literal
defaults, annotations, and source locations without importing the module. Second, explicit user/automation authorization
loads the selected module in the configured runtime and validates the final schema. Static inspection is advisory: dynamic
decorator expressions, imports, and non-literal defaults require runtime validation. Functions without the decorator are
ignored. Importing a module executes Python code, so selecting a file must not silently imply execution.

### 3.3 Decorator and type-hint responsibilities

The decorator and annotations should describe a constrained, portable parameter schema:

- Supported scalar values initially (Phase 4): `bool`, `int`, `float`, and `str`. OVITO-defined enums/vectors and other
  richer parameter types are Phase 8 extensions with explicit schemas.
- Defaults are taken from Python defaults when representable and type-compatible.
- Type hints describe the UI and validation contract; they are not a promise to support arbitrary Python typing
  constructs, runtime generics, or arbitrary object serialization.
- Optional metadata may describe labels, units, bounds, and choices, but the initial schema should remain intentionally
  small. Python's `typing.Annotated` is a suitable candidate for carrying this metadata. Unknown annotations should produce
  a clear unsupported-parameter diagnostic rather than silently degrade.
- Parameter values are stored in the OVITO modifier/session state, not only in a Python module global. Changing a value
  should follow normal property notification, undo, serialization, and pipeline invalidation rules.
- Code reload must preserve compatible parameter values and report removed/changed parameters rather than silently
  attaching old values to different parameters.

The dynamic parameters should be represented by an explicit `PythonParameterSet`-like model owned by the modifier. It should
contain the parameter schema, values, defaults, schema hash, validation state, change notifications, and serialization
logic. A saved modifier should retain at least the script path, qualified function name, script hash, schema hash, selected
environment identity, package/runtime compatibility information, and parameter values. It must not depend on Python module
globals to reconstruct the pipeline.

The declared type of the data argument and return value should be checked where feasible. Type hints are useful metadata,
not a security boundary or proof that arbitrary Python code behaves safely.

### 3.4 Interpreter selection, package delivery, and runtime weight

Interpreter selection is a first-class product decision, not an implementation detail. The user wants to use a chosen Python
environment while ensuring that the matching OVITO package is available. A fully bundled or "managed" runtime can be large,
but managed runtime and managed *environment* are separate ideas. OVITO need not ship a second interpreter if the selected
environment is a normal external CPython environment containing a compatible, repository-supplied `ovito` package. The
design should provide:

- An application setting for the interpreter executable/environment, plus a reliable way to select or reset to a supported
  environment. The UI should call this a **Python environment**, not promise that every arbitrary executable is compatible.
  Phase 4 supplies a lightweight selector in the modifier workflow, persisting the default through the shared settings
  facade and keeping explicit per-modifier environment identity. Phase 7 integrates it into the full Preferences UI;
  that dialog is not a prerequisite for the first Python modifier.
- A package supplied by this repository/distribution that exposes the decorator, supported data bindings, and runtime bridge.
- An explicit compatibility check before loading a script: Python implementation/version, package version, native extension
  ABI, platform, architecture, and required features. Explain precisely what is missing and how to repair it.
- No silent fallback to a different interpreter. The active executable, `sys.executable`, imported `ovito` path/version,
  and native extension identity should be inspectable in the UI and logs.
- Reproducible behavior across a GUI session and CLI batch execution; where possible, both should use the same configured
  environment.

The recommended first product path is therefore:

```text
user .py file + external editor
             ↓
selected compatible Python environment
             ↓
repository-supplied ovito package and native bridge
```

This is lighter than shipping a separate managed Python distribution, while retaining control over the package/API contract.
It is also honest about the boundary: `pyproject.toml` can constrain which environments pip/uv/another installer considers
compatible, but it cannot by itself guarantee that an already-running process has the right ABI, that a user has not mixed
packages, or that Python code is safe. OVITO must still perform an import/ABI capability probe and report its result.

The custom package should use a strict `pyproject.toml` as one layer of enforcement. It can specify `requires-python`, exact
or bounded dependencies, platform/architecture markers, optional feature groups, and a package version coupled to the OVITO
native bridge. Prefer a package layout in which the public decorator and type hints are lightweight Python code and the
native extension is versioned and checked explicitly. A strict project file is not a sandbox, and dependency resolution is
not sufficient to enforce the ABI contract at runtime.

The package should avoid silently installing or upgrading anything in a user's environment. Any setup helper must be
explicit, opt-in, and show the target environment, package version, and files that will change before modifying it.

### 3.4.1 Recommended lightweight package model

The recommended alternative to a bundled managed runtime is **a managed package contract with an external worker**:

```text
OVITO GUI/C++ process
        │ persistent local IPC
        ▼
selected user Python interpreter
        │
        └── repository-supplied ovito package
             ├── decorator and type/schema metadata
             ├── worker entry point
             └── compatible native bridge, if required
```

The package can be split into two installable layers:

- a lightweight, mostly pure-Python `ovito` API containing decorators, type hints, schema extraction, protocol definitions,
  and the worker entry point;
- an optional platform-specific bridge wheel containing native bindings or optimized array-transfer support.

This keeps the application download small and lets users use their own virtualenv, conda environment, uv environment, or
system Python. The persistent worker avoids paying interpreter startup cost on every pipeline evaluation and prevents most
user Python failures from bringing down the GUI. It does **not** make arbitrary environments safe or automatically make
arbitrary `DataCollection` objects available: the worker protocol and data-transfer model remain part of the design.

The package's strict `pyproject.toml` is appropriate for declaring the compatibility envelope. For example, it can use
`requires-python`, exact or bounded dependencies, platform/architecture markers, optional feature groups, and a package
version coupled to the OVITO protocol/native-bridge version. A package version or local build tag should identify the
compatible application/protocol pair. A runtime probe must still verify `sys.executable`, Python implementation/version,
`ovito.__file__`, package version, protocol version, native-extension identity, and a small handshake/capability exchange.

`pyproject.toml` and dependency resolution can reject many incompatible installations, but cannot prove that an existing
environment has not been mixed, that an extension was built against the right ABI, or that Python code is safe. Treat the
metadata as an admission filter and the handshake as the authoritative compatibility check. Do not silently install,
upgrade, or fall back to another environment.

For the first worker prototype, expose a deliberately narrow data bridge rather than pretending that every C++ object is
remotely transparent. Define which arrays/properties can be transferred, use shared memory or a buffer-oriented format for
large numeric arrays, and return explicit mutation operations or a validated result packet. JSON may describe schemas and
small control messages; it should not carry routine full particle arrays. If the benchmark shows that a worker cannot meet
interactive pipeline latency, retain the same package-level API but add an embedded-runtime backend for a fixed, supported
Python ABI.

There remains an execution-topology choice. An embedded runtime gives direct access to native OVITO objects but couples the
application binary to one CPython ABI and requires careful GIL/thread integration. A persistent worker lets OVITO launch the
selected environment and isolates crashes, but requires an efficient bridge for large arrays, task cancellation, errors, and
object identity. A worker must not use JSON or pickle as the default transport for full scientific datasets.

**Resolved in Phase 2.6** (audit decision D49 of [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md) §7, evidence in
[AUTOMATION_TOPOLOGY_SPIKE.md](AUTOMATION_TOPOLOGY_SPIKE.md)): the topology is a **persistent worker process on the
user-selected interpreter**, with arrays as raw length-prefixed buffers and shared memory as an optional optimisation. The
spike measured the seam against the in-process baseline an embedded runtime would have to beat - for 500 000 particles plus
200 000 triangles (13.7 MB), computing the same data in process costs 10.8 ms and the worker's framed transfer costs
13.0 ms, while JSON costs 539 ms for 2.3× the bytes, cancellation lands within 1.3 ms and a deliberately killed worker is
noticed within 4.7 ms and replaced within 43 ms. Embedding could save only that ≈2 ms while giving up the crash isolation
and freezing one ABI, so it stays a possible additive backend behind the same package API rather than the first choice.

### 3.5 Runtime and pipeline execution contract

The Python modifier is a normal pipeline modifier from the perspective of downstream nodes. Its runtime adapter must:

- Preserve the pipeline's animation time and validity semantics. The first Python modifier should be treated as
  frame-dependent by default and reevaluated when the frame changes. A later explicit caching declaration may optimize this,
  but arbitrary Python must not silently claim time invariance.
- Run Python off the GUI thread. Respect the GIL and Python interpreter rules; do not assume OVITO's C++ task scheduler can
  execute multiple calls concurrently in one interpreter.
- Propagate cancellation and task progress where the Python call can cooperatively yield/check cancellation. Document that
  arbitrary Python/native extension code may not be interruptible safely.
- Map Python tracebacks to a useful modifier status including filename, function, and line number.
- Ensure failed or cancelled execution cannot publish partially mutated pipeline output as a successful result.
- Define module caching, reload behavior, and dependency/import path behavior explicitly.
- Invalidate the modifier when its script, imported package identity, selected environment, or relevant schema changes.
- Avoid retaining raw pointers/references to transient upstream data beyond the invocation. If a worker process is used,
  define the supported array/data transfer model and avoid unnecessary full copies.

No promise of a security sandbox should be made. User-authored Python has the capabilities of the selected interpreter and
user account. GUI and CLI should make this clear and require explicit consent before importing/running a newly selected
file, especially when triggered by an AI plan.

### 3.6 Future Python node types

After the modifier workflow is proven, consider separate capabilities:

- Python source/generator for creating data without upstream input;
- user-defined modifiers with a richer parameter editor and reusable packaging;
- viewport overlays or rendering hooks, only after their threading and renderer ownership contracts are designed;
- standalone Python/batch integration.

These are not prerequisites for the first useful Python Function Modifier.

## 4. AI/CLI Workbench Automation

### 4.1 Product goal and differentiation

The CLI is not merely a headless alternative to the GUI and not just a wrapper around Python scripts. Its central purpose is
to let a human or AI agent understand and operate the *current workbench session*:

- discover the active dataset, pipelines, modifiers, parameters, selection, current frame, and viewport state;
- create or alter pipeline structure and parameters through OVITO's domain operations;
- ask the running application to evaluate, render, save, export, or produce a PNG screenshot;
- retrieve structured results, errors, task progress, and generated output locations.

This live-session, inspectable, machine-oriented workflow is the intended differentiation from a conventional scripting
console or an AI chat panel that only emits text. It may later support headless execution too, but the initial contract must
make GUI-attached operation clear.

### 4.2 Semantic protocol, not UI automation

Provide a local automation endpoint owned by the running OVITO session and a CLI client that connects to it. Use a
versioned, structured protocol (JSON-RPC over stdio or a local socket are candidate transports; select one after platform
and lifecycle review). The protocol has three kinds of operations:

**Queries**

- inspect session and scene;
- list pipelines, nodes, modifiers, available modifier types, and valid parameters;
- inspect the selected item and its property schema;
- inspect viewport/camera/frame state;
- inspect running/completed tasks and output artifacts;
- request image capture or a rendered PNG with explicit dimensions/options.

**Commands**

- import/open data;
- add, remove, enable/disable, and reorder pipeline nodes;
- set parameter values;
- change selection, frame, or camera state;
- evaluate a pipeline, save a session, export data, or render an image.

**Events**

- scene/session revision changed;
- task started/progressed/completed/cancelled/failed;
- confirmation requested;
- artifact created.

In addition to a point-in-time snapshot, expose a bounded semantic activity stream for clients that need to understand what
the user has been doing. Record meaningful operations such as import, modifier insertion, parameter changes, selection/frame
changes, and task completion; do not record raw mouse movement or keystrokes. Events should identify their origin (`user`,
`qml`, `cli`, `ai`, or `python`), affected stable object IDs, and session revision. Apply normal privacy filtering to paths
and data-derived values, and retain only a configurable recent window. Candidate queries are `context.snapshot`,
`context.recent_activity`, and `context.subscribe`.

Operations should be domain-oriented (for example `pipeline.add_modifier`, `property.set`, `viewport.capture_png`), not
widget-oriented (`click`, `find_button`, QML object names, pixel coordinates). The API may expose a screenshot as an
observation/output, but must not require screenshot interpretation to understand the scene state.

### 4.3 Session discovery and connection

The CLI needs to find or explicitly target a running workbench without guessing. The design should specify:

- a per-user local endpoint and a session descriptor containing PID, protocol version, and endpoint;
- secure permissions so other local users cannot control the session by default;
- explicit `connect`, `list-sessions`, and `disconnect` behavior;
- stale descriptor cleanup and multiple-session selection;
- opt-in startup/configuration of automation (not an unexpectedly open network listener);
- local-only by default. Remote access, if added, requires authentication and transport security as a separate design.

Illustrative CLI (names provisional):

```text
ovito-cli sessions
ovito-cli connect --session <id>
ovito-cli inspect scene --json
ovito-cli pipeline add-modifier --pipeline <id> --type SliceModifier --json-args '{...}'
ovito-cli pipeline set-parameter --node <id> --name distance --value 3.0
ovito-cli viewport capture --format png --output result.png
ovito-cli task wait <task-id>
```

All machine-consumable output should have a stable JSON mode, explicit protocol/schema version, deterministic error codes,
and exit statuses. Human-readable output is an additional presentation, not a separate implementation.

### 4.4 AI interaction and safety

The AI client should follow an inspect-plan-confirm-execute-observe pattern:

1. Query current state and available capabilities.
2. Build a plan referencing stable object IDs and valid parameter schemas.
3. Show the user the objects and values that will change.
4. Request confirmation for mutations according to permission policy.
5. Execute a bounded operation/transaction.
6. Observe task completion and inspect the resulting state/artifacts.

The API must not grant arbitrary process execution, arbitrary filesystem access, or Python execution implicitly. Use
capabilities such as `scene.read`, `pipeline.edit`, `viewport.control`, `files.write`, and `python.execute`; default to
least privilege. Destructive operations, overwrites, external file access, and execution of user scripts should require
explicit authorization. Provide cancellation and a route to undo supported mutations.

AI operations should use the same validation and domain operations as GUI users. A request must fail clearly when a
target has been deleted/replaced or a parameter is no longer valid; never reinterpret a stale row index as a different
object. Stable identifiers and a session revision/optimistic concurrency check are required.

### 4.5 Commands, transactions, tasks, and outputs

Do not treat the existing `gui/base::Command` class as the whole automation API: it describes UI actions and exposes
presentation state. Instead, share underlying domain operations and introduce a machine-facing registry/catalog with:

- stable method ID, description, parameter schema, capability requirements, and side-effect classification;
- input validation and structured success/error results;
- session revision and object IDs;
- transaction/undo metadata for mutations where the existing undo model supports it;
- task ID for asynchronous work, progress, cancellation, and final result;
- output artifact metadata (path, media type, dimensions, frame, originating request).

Keep the layering explicit:

```text
core domain operation
        ↓
automation operation catalog/controller
        ↓
QML / QtWidgets / CLI / AI or MCP adapter
```

An MCP adapter may be added later as another protocol client, but MCP should not become a dependency of the pipeline or
automation core. The CLI and any MCP tools must invoke the same operation catalog, validation, capability checks, task
tracking, and revision preconditions.

Operations should be atomic where practical. Multi-step AI plans should either use an explicit transaction when supported or
report partial completion and the operations that succeeded. Do not promise rollback for computations or file writes that
cannot actually be rolled back.

Multi-step plans should carry a `baseRevision` obtained from a snapshot. The controller rejects the plan if the session
revision has changed, unless the client explicitly re-queries and replans. Stable object IDs remain necessary, but revision
preconditions prevent an otherwise valid ID from being used against a scene whose meaning changed underneath the client.

For image output distinguish at least:

- **viewport capture**: capture the current displayed view as seen by the user;
- **scene render**: invoke OVITO rendering with explicit size/settings and save a PNG.

They may share rendering infrastructure but are different requests. Results should report which path was used, the frame,
viewport/render settings, and output path. Reuse the shared offscreen rendering service where appropriate; do not implement
a separate AI renderer.

## 5. Shared Contracts and Separation

The two capabilities should converge on reusable metadata without merging their runtimes:

```text
Core scene / pipeline / task / rendering operations
            ↑                       ↑
Python modifier adapter     Automation query/command gateway
            ↑                       ↑
User Python file            CLI client / AI agent / QML panels
```

Shared concepts include stable object IDs, parameter schemas, validation, task lifecycle, structured errors, session
revision, artifact results, and capability checks. Python function metadata may contribute to the automation catalog, but
an AI command to create a Python modifier must still require `python.execute` and explicit script authorization.

QML should present shared state and invoke shared operations. It should not own Python execution, CLI protocol parsing, AI
planning, or a second copy of pipeline mutation logic. An eventual AI panel is a client of the same local automation
service, not a privileged special case.

## 6. Design Decisions Still Open

Resolve these through small prototypes before freezing user-facing APIs. Items 1-3, 5, 6 and 7 are resolved or
partly resolved by the Phase 2.6 decisions D39-D51 (`src/ovito/core/automation/`, with the evidence documents indexed in
[UI_PLAN.md](UI_PLAN.md) under Phase 2.6), and items 4, 8 and 9 belong to the phases that first attach the layer. The
*gateway-side* half of items 6 and 9 is settled since Phase 2.6's first slice — the operation catalog, the ID grammar, the session revision, the dispatch order and
the capability names are decisions D39-D43 of [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md) §7 — so what stays open in those two
items is the transport and the consent/user-interface half.

1. **Python execution topology**: embedded runtime versus persistent worker process; measure throughput and data-transfer
   cost on representative particle and mesh inputs. Do not use JSON/pickle for routine full-dataset transfer.
   **Resolved in Phase 2.6** (D49): the persistent worker is chosen, with raw length-framed array transfer as the default
   and a pooled shared-memory path left as an optimisation; the measurements, the caveats (one machine, synthetic payload,
   pooled segment and embedded runtime unmeasured) and the reproduction recipe are in
   [AUTOMATION_TOPOLOGY_SPIKE.md](AUTOMATION_TOPOLOGY_SPIKE.md). The public Python signatures are still not frozen - the
   decorator and the data contract are items 3 and 5 below.
2. **Interpreter/package compatibility**: exact supported CPython versions/ABIs, packaging per platform, and whether the
   project supplies only a compatible package or also an optional managed environment. **Partly resolved in Phase 2.6**
   (D48): the runtime half exists as `automation/python/` - the handshake protocol `ovito.automation.handshake` version
   `1.0`, the CPython 3.10-3.13 envelope, the platform/architecture matrix, the feature vocabulary
   (`schema.introspection`, `function.inplace`, `parameter.scalar`, `array.buffer`, `array.shared-memory`,
   `task.cancellation`, `traceback.mapping`) and a probe that validates one environment without installing or falling
   back. What remains open is the *shipping* half: which wheels are built per platform and ABI, what the strict
   `pyproject.toml` says, and whether an optional managed environment is offered at all (Phase 4 for the package, Phase 8
   for a managed environment).
3. **Python data contract**: first modifier is writable in-place `DataCollection` plus `None` return; later source/analysis
   contracts remain separate. **The transfer and ownership half is settled in Phase 2.6** (D51, evidence in
   [AUTOMATION_DATA_BRIDGE.md](AUTOMATION_DATA_BRIDGE.md)): arrays travel as one length-framed raw payload with JSON
   descriptors and a checked SHA-256 in *both* directions, the bytes are owned copies rather than shared memory (a
   strided source is copied, because the wire has no stride), the type vocabulary is fixed (`float64`, `float32`,
   `int64`, `int32`, `uint32`, `int8`, `uint8`) and an unknown type is refused rather than converted, and a refusal names
   the array it is about. What stays open for Phase 4 is the *shape* a modifier sees: the adapter from an OVITO property
   buffer to an array (O21), what a modifier may write and what a partial or failed result means, and a transfer larger
   than the 512 MB / 64 array bound of one request.
4. **Python concurrency/cancellation**: GIL strategy, serial execution policy, cooperative cancellation, and application
   shutdown behavior.
5. **Decorator schema**: supported annotations and metadata, module reload, code-file change detection, and parameter
   persistence. **The reading half is settled in Phase 2.6** (D51): a schema is read with `ast` alone by default, so
   selecting a file does not execute it - proven by a fixture that writes a marker from its top level - and the report
   carries the function, its decorators, its parameters (kind, annotation as source text, default, requiredness) and
   diagnostics for a syntax error (line/column), a decorator or decorator argument computed at run time, and an
   unreadable file; an explicit import mode executes the file and maps the traceback of the user's own frames. What
   stays open for Phase 4 is the authoritative schema of the *package's* decorator (what it accepts and defaults to),
   module reload and code-file change detection, and how a schema change invalidates persisted parameter values.
6. **Automation transport**: stdio broker vs local socket, Windows/macOS/Linux endpoint lifecycle, and client discovery (the
   operation catalog and its framing-independent error rules are already fixed by D39/D42). **Partly settled in Phase 2.6**
   (D50, evidence in [AUTOMATION_IPC_SPIKE.md](AUTOMATION_IPC_SPIKE.md)): the transport is a per-user **local socket**
   carrying JSON Lines control messages, discovery is a session descriptor in a per-user runtime directory
   (`AutomationSessionDescriptor`, in Core and tested without a socket), the socket is an **absolute path inside that
   owner-only directory** so no other user can own the name, capabilities are granted per connection with every refusal
   named, the snapshot is a composition of contract answers with explicit truncation flags, artifacts are bounded and in
   memory, and the transport's error codes are disjoint from the contract's. What is *not* settled and belongs to Phase 3:
   the production endpoint (backpressure, resume, a client that stops reading), the CLI and its JSON mode, and the
   Windows/macOS runs (O20). Remote access is not designed here and would need real authentication.
7. **GUI capture vs scene rendering**: exact semantics and whether current viewport capture is available on all target
   platforms while hidden/minimized. **Partly settled in Phase 2.6**: the protocol half is measured (a capture returns a
   bounded in-memory PNG artifact, no file is written, the client verifies the bytes) while the rendering half is not,
   because a core-only process has no graphics device (O19); the design's own split - the currently displayed view comes
   from the frontend's retained frame graph, an explicit scene render from the shared offscreen service (D34) - is what
   Phase 5 implements, and the frontend's offscreen check is today's evidence that a view can be rendered headlessly.
8. **Undo and transaction scope**: which scene mutations are undoable and how a multi-command AI plan interacts with the
   existing undo stack.
9. **Permission UX**: local process trust model, script execution consent, file-write confirmation, and policy persistence
   (the capability vocabulary and the read-only connect default are fixed by D43; what is open is who may grant what, and
   how the user is asked).

## 7. Delivery Slices Mapped to `UI_PLAN.md`

The normative mapping is now recorded in [UI_PLAN.md](UI_PLAN.md) under **Phase 2.6: Automation & Python Architecture
Adaptation**, and summarized below. Phase 2.6 owns adaptation and evidence; it does not make the later user-facing features
available.

1. **Phase 2.6 architecture adaptation**: validate the custom package's `pyproject.toml` constraints plus runtime ABI probe;
   compare an embedded runtime with a worker using one representative data bridge. Separately validate a local client can
   attach to one running session, inspect it, and request a bounded in-memory PNG as an internal feasibility probe, not a
   released feature. No AI UI required.
2. **Phase 3 automation foundation**: stable session/object identifiers, query catalog, structured results/errors, revision
   preconditions, task events, bounded semantic activity context, local endpoint, and CLI JSON mode. Ship read-only queries;
   writable pipeline operations belong to Phase 4 and public viewport capture to Phase 5.
3. **Phase 4 Python Function Modifier MVP and pipeline automation**: environment/file/function selection, AST preview
   followed by authorized import, one compatible environment, `@ovito.modifier`, writable in-place data plus `None` return,
   scalar parameter controls, `PythonParameterSet`, pipeline execution, traceback display, reload, frame-aware invalidation,
   and session persistence. CLI pipeline construction/parameter editing/evaluation and task wait/cancel consume the same
   shared operations and revision/permission checks.
4. **Phase 5 interactive automation**: viewport/selection/frame control and production displayed-view PNG capture, with
   asynchronous completion, artifact metadata and explicit file-write authorization where output is saved.
5. **Phase 7 output and batch automation**: authorized session save, data export, explicit scene render and artifact
   retrieval, plus headless Python batch/CI execution without QML/QWindow.
6. **Phase 8 advanced workbench capabilities**: optional AI plan/review panel; richer parameter types, source/analysis
   decorators, an optional managed environment/setup helper, Python Console, MCP adapter,
   external tool integrations, and remote automation only after their own security and lifecycle designs.

The execution topology is decided by the Phase 2.6 spike (D49, [AUTOMATION_TOPOLOGY_SPIKE.md](AUTOMATION_TOPOLOGY_SPIKE.md)):
a persistent worker on the user-selected interpreter, driven over JSON-lines control with raw length-framed array
payloads. Later phases consume the stable package and automation contracts regardless; an embedded runtime remains an
additive backend behind the same package API for a fixed, supported ABI.

## 8. Initial Acceptance Criteria

### Python Function Modifier

- A user can select a configured compatible environment and a normal Python file, and see a clear package/ABI compatibility
  check. A separately bundled managed environment is optional, not required for the first implementation.
- Static preview lists decorated functions without executing the file; explicit activation performs runtime validation.
- A function can be inserted into a pipeline, parameterized in the UI, evaluated, saved/reopened, and recomputed after
  edits.
- The first function contract mutates writable input and returns `None`; source and analysis return contracts are separate
  future features.
- A representative function adds/modifies data and downstream native modifiers consume its output correctly.
- Syntax/import/runtime errors include actionable file/function/line traceback context and do not crash the application.
- Cancellation, repeated evaluation, frame changes, module reload, dataset replacement, and application shutdown are
  tested.
- The selected runtime and package are visible; incompatible environments do not silently fall back.

### AI/CLI

- A CLI can explicitly discover/connect to a running session, inspect the current scene and pipeline as structured JSON,
  and identify selected objects using stable IDs.
- It can add a modifier and set a validated parameter in the live GUI session, then report the resulting state/revision.
- It can request a viewport PNG capture and a scene render separately, and report artifact metadata.
- Long operations expose task IDs, progress, completion/error, and cancellation.
- Invalid, stale, unauthorized, and destructive requests fail with deterministic machine-readable errors.
- Default operation is local-only; an untrusted client cannot silently execute Python or write files.
- A multi-step plan carries a revision precondition and is rejected when the observed session is stale.
- A bounded semantic activity stream distinguishes user, GUI, CLI, AI, and Python-originated operations without recording raw
  input events.

## 9. External Reference

OVITO's public Python API demonstrates that a data pipeline can be manipulated and evaluated programmatically, and its
Python Script Modifier demonstrates the usefulness of user-defined computations. These are references for capability, not
requirements to copy their UI, licensing model, interpreter packaging, or API. This proposal's distinctive emphasis is a
user-selected compatible environment, a repository-supplied package with a strict metadata/ABI contract, and decorated
functions in ordinary files, combined with an AI/CLI interface for observing and operating a live workbench session.

