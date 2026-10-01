# Automation and Python Contracts

> **Status**: Normative. This is the prose contract of the machine-facing automation layer and of the Python seam that
> Phase 2.6 established; it is the document `tests/cpp/core/automation/tst_automation_contracts.cpp` calls "the contract
> it is the executable half of" (audit item O13, which this file closes). The vocabulary, the wire types, the identity
> grammar, the revision and dispatch rules, the operation catalog, the task/transaction/event schemas and the Python
> handshake, worker protocol, data bridge and schema report below are decisions of
> [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md) §7 (D39-D52); this document states their rules in one place, from the point of
> view of a client. Where a rule and an implementation disagreed while this was written, the implementation was
> corrected and the rule stated here (see §6 on the implied `task.control`).
>
> **Audience**: the author of the next client (a CLI in Phase 3, the Python package in Phase 4, an MCP adapter or an AI
> agent in Phase 8) and the author of the next operation. Neither should have to read `ovito/core/automation/` to know
> what is promised; both should read it to know where a rule is enforced.

## 1. What is normative here, and what is not yet

| Area | Implementation | Executable half | Evidence |
|---|---|---|---|
| Vocabulary, wire types, identity grammar, revision and dispatch rules, catalog, tasks, transactions, events | `src/ovito/core/automation/Automation{Contract,Protocol,ObjectId,ObjectRegistry,Session,Gateway,Task,Transaction,Event}.h/.cpp` | `tst_automation_contracts.cpp` (28 test functions, 30 cases) | [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md) §7, D39-D47 |
| Python package contract and runtime probe | `automation/python/Python{Contract,Handshake,EnvironmentProbe}.*`, `ovito_probe.py` | `tst_python_environment_probe.cpp` (20 test functions, 22 cases) | D48, [UI_TEST_ENV.md](UI_TEST_ENV.md) §4.2 |
| Python worker protocol and data bridge | `automation/python/PythonWorkerProtocol.h`, `PythonWorkerProcess.*`, `PythonDataBridge.*`, `ovito_worker.py` | `tst_python_data_bridge.cpp` (16 test functions, 18 cases) | D49, D51, [AUTOMATION_DATA_BRIDGE.md](AUTOMATION_DATA_BRIDGE.md) |
| Schema preview (AST first) | `automation/python/PythonIntrospector.*`, `PythonSchemaPreview.*`, `ovito_schema.py` | `tst_python_schema_preview.cpp` (15 test functions, 17 cases) | D51, [AUTOMATION_DATA_BRIDGE.md](AUTOMATION_DATA_BRIDGE.md) |
| Local transport and discovery | `AutomationSessionDescriptor` (Core, normative) + `automation/spike/ipc/` (prototype) | `tst_session_descriptor.cpp` (8 test functions, 10 cases); `ovito-automation-ipc-spike` self-test | D50, [AUTOMATION_IPC_SPIKE.md](AUTOMATION_IPC_SPIKE.md) |
| Execution topology | chosen: persistent worker, framed arrays | `ovito-automation-spike` | D49, [AUTOMATION_TOPOLOGY_SPIKE.md](AUTOMATION_TOPOLOGY_SPIKE.md) |

Not normative here, and deliberately so:

* **The transport endpoint.** `AutomationLocalEndpoint` and `AutomationLocalClient` are spike-local classes; §17 records
  what they demonstrated, and Phase 3 owns the production endpoint (O20). What *is* normative is the descriptor of §17.1,
  which lives in Core, and the two-layer error rule of §17.3.
* **Anything user-facing.** No frontend, CLI or Python package consumes the layer yet (O18), so this contract is about
  what a client can rely on, not about what a user can do. The feature phases are mapped in
  [UI_PLAN.md](UI_PLAN.md) §3, "Formal feature placement after Phase 2.6".
* **The Python function contract.** What a user's decorated function receives and may write (the `DataCollection`
  shape, partial results, streaming) is Phase 4; §15 fixes only how *bytes* cross the process boundary (O21).

How to run the suites is in [UI_TEST_ENV.md](UI_TEST_ENV.md) §4. In short: `ctest --preset native` runs all five, and
the schema, bridge and probe suites need a `python3` on `PATH` (they skip their environment cases without one) but never
a GUI, a window system or a graphics device.

## 2. The contract version and its compatibility rule

* The contract version is `AutomationContract::version()`, `"<major>.<minor>"`, currently **0.3**. Every
  `AutomationResult` carries it as `contractVersion`, including failures.
* A **new minor version may** add operations, parameters, capability names, error codes and result fields, and may relax
  a previously required parameter. It **may not** remove or rename any of them, require a new parameter, change a
  parameter's type, or change the meaning of an existing error code; that needs a new **major** version.
* `0.x` means the contract is still being completed by the phases that implement it and has one consumer at a time.
  0.2 added the task lifecycle vocabulary (`TaskState`, `EventKind`, `ActivityOrigin`) and the capability `TaskControl`;
  0.3 added the three read operations of §8 that a client needs to look at a session (`viewport.list`,
  `selection.describe`, `object.describe`). Both are exactly the additive change the rule allows.
* Enum numeric values are not part of the contract - wire names are. `TaskControl` was therefore appended after
  `ProcessExecute` instead of being inserted where it reads best, so that no existing name changes value.

## 3. Wire types

### 3.1 Operations

An operation is described by an `AutomationOperationDescriptor`: `id`, `kind`, `summary`, an optional `undoLabel`, its
`parameters`, and the `requiredCapabilities`.

* `kind` is `"query"` or `"command"`. A query inspects and changes nothing; a command changes the session, becomes a task
  and runs inside a transaction boundary.
* Two registration rules are programming errors, not runtime conditions (asserted while the catalog is built): a query
  may declare **read capabilities only**, and a command must declare **at least one capability that permits a change**.
  Together they are what makes "a read-only client can never change anything" a property of the catalog rather than a
  promise about handlers.
* `undoLabel` is what a command's changes appear as on the undo stack, e.g. "Add a test node". It defaults to the
  operation ID. There is no "transactional" flag on a descriptor: a command whose mutation is not undoable simply
  produces an empty compound operation, which commit() skips.
* An operation may be **declared without a handler** - that is how a later phase reserves its name in the catalog. A
  request for such an operation is answered `not_supported` (§7 step 3), and this is the *only* meaning of that code.

### 3.2 Parameters

| Wire type | Accepted value | Notes |
|---|---|---|
| `string` | `QString` | |
| `integer` | an integral value | A `double` is **rejected** even when it is whole: an index must not be silently truncated from `1.7` |
| `number` | an integral value or a `double` | |
| `boolean` | `bool` | |
| `string-list` | `QStringList` | |
| `object-id` | a `string` that parses as an object ID (§4) | Validated at dispatch, before the handler runs |
| `json` | anything | The operation documents what it expects; used by no builtin operation yet |

A parameter is **required** unless it declares a default value. Optional parameters declare a range
(`setRange(minimum, maximum)`) or an allowed-value list where the value is a name rather than a number. Validation
reports every problem at once, and an argument the operation does not declare is an error too, never ignored: a client
that misspells a parameter must hear about it instead of silently operating on the default.

### 3.3 Request

| Field | Required | Meaning |
|---|---|---|
| `operationId` | yes | The operation to run |
| `arguments` | no | A map of parameter name to value |
| `baseRevision` | no | The session revision this decision was based on (§5) |
| `requestId` | no | The client's own correlation id; it is copied into the task record and is the client's, not the contract's |

### 3.4 Result

| Field | Meaning |
|---|---|
| `ok` | Whether the operation was served |
| `contractVersion` | The version of the contract that answered |
| `revision` | The session revision the answer was computed from - present on **every** answer, success and failure alike |
| `data` | The operation's payload; present on a success and **dropped** when the result becomes a failure |
| `error` | `{code, message, details}` on a failure; the code is what a client switches on, never the message text |
| `warnings` | Non-fatal remarks, e.g. "the task had already finished" |
| `taskId`, `transactionId` | Present on a command's answer: the task and the boundary the command ran in |
| `artifacts` | Descriptors of files or images the operation produced: `id`, `mediaType`, `operationId`, optional `width`/`height`/`frame`/`filePath`. An artifact descriptor never carries the bytes - moving bytes is the transport's business |

A handler produces a failure **in place** (`result.setError(...)`), keeping the revision and warnings the result already
carries. This is why handlers are `void(const AutomationRequest&, AutomationResult&)`: an earlier design returned a
failure and a returning handler silently discarded it, which made a refusal look like a success.

### 3.5 Error codes

| Wire name | Meaning | What a client should do |
|---|---|---|
| `invalid_request` | The request is not well formed: no operation ID, an unparsable base revision | Fix the request |
| `unknown_operation` | No operation of that ID; `details.knownOperations` lists what this build has | Fix the request (or the version assumption) |
| `invalid_argument` | The arguments do not match the schema; `details.errors` names every problem | Fix the request |
| `missing_capability` | The client lacks a required capability; `details.missing` and `details.granted` list the wire names | Ask the user, or drop the operation |
| `stale_revision` | The base revision is not the current one; `details.baseRevision`, `details.currentRevision` | Re-query the session and re-plan |
| `unknown_object` | An object ID this session never issued | Fix the request |
| `invalidated_object` | The ID was issued, and its identity is gone: the data set was replaced, the object was deleted, or the task fell out of the bounded history | Re-query the session |
| `not_supported` | The operation is declared but not implemented in this build | Stop asking; do not look for a permission that would help |
| `cancelled` | The operation was cancelled | The client's own request, or the surrounding work |
| `internal_error` | Anything else, including an OVITO exception that escaped the handler; `details.messages` carries its text | Report a defect |

The vocabulary is closed: a client may switch on these ten names and treat an unknown one as a defect of the peer, not
as a new case to handle.

## 4. Identities

### 4.1 Grammar

| Object | Form | Example |
|---|---|---|
| Scene node | `scenenode:s<N>` | `scenenode:s7` |
| Pipeline | `pipeline:p<N>` | `pipeline:p42` |
| Modification node | `modifier:m<N>` | `modifier:m108` |
| Viewport | `viewport:v<N>` | `viewport:v2` |
| Property field of an object | `property:<owner-kind>:<owner-number>/<field>` | `property:m108/distance` |

`N` starts at 1, has no leading zeros, and is counted per kind. The grammar is strict: `p42`, `pipeline:`, `pipeline:p01`,
`modifier:p1` (a pipeline number where a modifier number is required), `property:m108` (no field) and
`property:pipeline:p1/distance` (an owner kind that is not an object kind) are all rejected - a malformed ID is never
completed or reinterpreted. `AutomationObjectId::parse()` is the reference implementation, and the `object-id` parameter
type calls it before a handler runs.

### 4.2 Allocation and lifetime

* A number is handed out **once per object** and **never reused**, per session. It keeps its meaning while the object
  lives, including across an undo/redo cycle that removes the object from the scene and puts it back.
* The registry holds its objects **weakly** and notices when an address is reused by a new object, so a released
  identity can never be attached to a different object.
* `wasAssigned(id)` distinguishes "never issued" from "issued and released", which is the difference between
  `unknown_object` and `invalidated_object`. This is also how the task and transaction registries answer for an ID that
  fell out of their bounded history.
* A **deleted** object releases its identity; a **data set replacement** releases every identity of the session;
  a **presentation refresh** (the viewport layout or active viewport, the selection, the current frame) releases none -
  it advances the revision and leaves every ID resolving to the object it named. The exit gate's rule is therefore:
  identities survive a refresh and an undo/redo, and are invalidated deterministically by deletion and by a new data set.
* Semantic aliases are not a substitute. An earlier draft used `viewport:v-perspective`; a viewport's view type is
  neither unique (two viewports may show the perspective view) nor stable (the user can change it). An alias may be added
  next to the numeric ID in a later phase and must not replace it.

## 5. The session and its revision

* A session owns one data set, one revision counter, one identity registry, one task registry, one transaction registry
  and one event log. Two clients of one session see the same identities and the same revision.
* The revision starts at **1** and is **monotonic**. It advances when the session changes in a way a client can see:
  another data set becomes current, the viewport layout, the active viewport, the selection, the current frame or the
  animation interval changes, and every command that ran successfully. Setting the **same** data set again is not a
  change: it neither advances the revision nor ends the validity of any identity.
* The revision is **coarse on purpose**: it says "something you may have looked at changed", not which operation did it.
  A client that needs to know what happened reads the events of §11.
* `acceptsRevision(baseRevision)` is the whole precondition rule: a request **without** a base revision is accepted
  (a client that does not care what it overwrites), a request naming the **current** revision is accepted, and one
  naming an older **or newer** revision is rejected with `stale_revision`. A client that claims a revision the session
  has not reached is as stale as one that is behind.
* Every answer names its revision, so a client can tell whether its view was current even when the operation failed.

## 6. Clients, identity, capabilities and permissions

* A client is registered by the session and gets an ID `client:c<N>`, allocated once and never reused. A gateway is one
  client: its own ID, its own origin, its own capability set.
* The **origin** is a category, not an identity: `user`, `qml`, `cli`, `ai` or `python`. It is what lets a later client
  tell a user's edit from its own, and every task and every event carries it.
* Capabilities are named permissions an operation requires. The **read capabilities form a closed set** -
  `session.read`, `scene.read`, `selection.read`, `file.read` - and only they may appear on a query. The others permit a
  change or an external effect: `pipeline.write`, `selection.write`, `animation.write`, `python.execute`, `file.write`,
  `network.access`, `process.execute` and `task.control`.
* A client gets the **read-only defaults** on connect (`session.read`, `scene.read`, `selection.read`, `file.read`) and
  everything else only by explicit grant. `python.execute`, `file.write`, `network.access` and `process.execute` are never
  granted by default, by any code path in this layer.
* **`task.control` is implied by every capability that permits a change**: whoever may start work may stop it again, and
  `task.control` grants no authority of its own - a task of another client cannot be cancelled with it either (§9). A
  read capability implies nothing. Revoking a capability explicitly does not revoke the implied one; `clear()` removes
  everything.
* Permission changes have exactly one path: `grantCapability`, `grantCapabilities`, `grantReadOnlyDefaults`,
  `revokeCapability` and `clearCapabilities` on the gateway. The accessor `permissions()` is read-only, so an operation
  cannot grant itself anything - a descriptor that required, say, `file.write` in order to hand it out would make the
  client's own authority the source of the grant.
* Every grant and every revocation is recorded as an `activity` event naming the capability, and granting what the client
  already has is not a change and is not recorded twice. Nothing is persisted in this phase; persistence and the consent
  dialog are Phase 8 (design §6 item 9).

## 7. The dispatch order

A request is answered in this order, and each step is a refusal, not a fallback:

1. **Well formed?** No operation ID, or an unparsable base revision: `invalid_request`.
2. **Known?** No operation of that ID: `unknown_operation`, with `details.knownOperations`.
3. **Implemented in this build?** A declared operation with no handler: `not_supported`, answered *before* the argument
   and capability checks, because what the build does not have cannot be granted by a capability or fixed by a better
   argument.
4. **Arguments match the schema?** `invalid_argument`, with every problem in `details.errors`.
5. **Capabilities?** `missing_capability`, with `details.missing` and `details.granted`.
6. **Revision current?** `stale_revision`, with both revisions.
7. **Run it.** A query's handler runs and answers in one step. A command first gets a task record and a transaction
   boundary (§9, §10), then its handler runs with the ambient task installed for progress and cancellation. An OVITO
   `OperationCanceled` becomes `cancelled`; any other exception becomes `internal_error` with its messages.
8. **Finish.** A command's success advances the revision, commits its boundary and closes its task; a failure aborts the
   boundary the command owns (or reverts only the command's own share of a boundary someone else opened) before the task
   is closed.
9. **Log.** Every dispatched operation that passed the checks writes an `activity` event naming the operation and the
   task/transaction IDs - and no argument value (§11).

Two properties follow and are tested: a failed command leaves **no** partial change behind, and a client cannot gain
authority by being refused (dispatching the whole catalog without a capability leaves the client with none).

## 8. The operation catalog of this phase

| Operation | Kind | Capability | Arguments | Result payload |
|---|---|---|---|---|
| `session.describe` | query | `session.read` | - | `hasDataSet`, `contractVersion`, `revision`, `sessionFilePath`, `objectCount`, `clientId`, `origin`, `capabilities`, `viewports[]` (`id`, `viewType`, `active`), `sceneNodeCount`, `selectedObjects[]`, `currentFrame`, `taskCount`, `openTransactionId` |
| `scene.list_nodes` | query | `scene.read` | - | `nodes[]`: `id`, `title`, and `pipelineId` when the node holds a pipeline |
| `pipeline.describe` | query | `scene.read` | `pipelineId` (object-id, required) | `id`, `items[]` in evaluation order: `id` (modification nodes only), `type`, `title`, `className` |
| `viewport.list` | query | `session.read` | - | `viewports[]`: `id`, `viewType`, `title`, `active`, `maximized`, `fieldOfView`, `gridVisible`, `renderPreviewMode`, `sceneNodeCount`; `activeViewportId`, `maximizedViewportId` |
| `selection.describe` | query | `selection.read` | - | `sceneNodes[]`: `id`, `title`, `pipelineId`; `count` |
| `object.describe` | query | `scene.read` | `objectId` (object-id, required) | `id`, `kind`, `className`, `title`, plus the payload of the kind: `pipelineId` (scene node), `items[]` (pipeline, as `pipeline.describe`), `modifierType`, `enabled`, `pipelineIds[]`, `properties[]` (modification node), `viewport` and `properties[]` (viewport), `ownerId`, `ownerTitle`, `ownerClass`, `name`, `label`, `type`, `value` (property) |
| `task.list` | query | `session.read` | `limit` (integer, optional) | `tasks[]` newest first, `count`, `lastSequence` |
| `task.describe` | query | `session.read` | `taskId` (string, required) | The task record of §9.1 |
| `task.cancel` | **command** | `task.control` | `taskId` (string, required) | The task record; see §9.2 |
| `transaction.list` | query | `session.read` | `limit` (integer, optional) | `transactions[]` newest first, `count` |
| `event.list` | query | `session.read` | `since` (integer, optional), `limit` (integer, optional), `kinds` (string-list, optional) | `events[]`, `lastSequence`, `firstRetainedSequence` |

Notes that a client needs and the table cannot carry:

* "The scene" means the **active viewport's** scene, falling back to the first viewport's scene: a data set can hold
  several scenes, and the one a client means by "the session" is the one it is looking at.
* `pipeline.describe` distinguishes three refusals: an unparsable ID or one this session never issued is `unknown_object`,
  an ID whose object is gone is `invalidated_object`, and an ID of the wrong kind - asking a viewport for its modifiers -
  is `invalid_argument` ("is not a pipeline").
* The source node at the end of a pipeline's chain has no ID of its own yet: it is reported by `title` and `className`.
* `session.describe` is the first call a client makes: it answers whether there is anything to look at, which contract
  is being spoken, and which revision the next request should be based on.
* `viewport.list` is how a client learns to address a viewport: the ID of a viewport cannot be derived from anything a
  client knew before it saw it once, so the list is its entry point to `object.describe`, to the captured images of
  Phase 5 and to the viewport operations that follow.
* `selection.describe` is the one operation that needs `selection.read` and nothing else. A client that may look at what
  is selected need not be allowed to read the whole scene, which is what makes the capability meaningful; it is also why
  the selection payload is small (the selected scene nodes) and not a second `scene.list_nodes`.
* `object.describe` is the generic by-ID read: one operation instead of one per kind, and the only one that answers
  about a **property** - `property:v1/fieldOfView`, the owner's local name plus the field name (see §4). A modification
  node and a viewport report their parameters with the property IDs a client passes back when it changes one later, so
  a client can read what it will be able to write without a second catalog.
* A property's `value` is a JSON value: a field whose type has no JSON representation (a `Vector3`, an
  `AffineTransformation`) is reported by its `type` name with a null `value`, so that the parameter is visible instead of
  silently missing the way a field without read accessors would be (§14.1).
* **There is no catalog query.** A client learns this catalog from this document, from the C++ API, or from the
  `knownOperations` list of an `unknown_operation` failure. A `catalog.list` operation is still not built; the
  declare-without-handler mechanism of §3.1 exists so that it can be reserved before it works.

Phase 2.6 added one command, `task.cancel`; Phase 3 added the three read operations above. Every other operation is a
query, which is what "a local client can perform only the bounded read-only snapshot" of the exit gate means: the write
capabilities exist in the vocabulary and are checked by the gateway, but no builtin operation requires one, and a later
phase adds the operations that do.

## 9. Tasks

### 9.1 The record

A task is created for every **command**, not for a query: a query is answered in one step, so there is nothing to watch,
and `this_automation_task::get()` is null inside a query handler (where `progress()` and the activity helpers are
documented no-ops).

| Field | Meaning |
|---|---|
| `id` | `task:t<N>`, allocated once and never reused |
| `state` | `pending`, `running`, `completed`, `failed` or `cancelled` |
| `operationId`, `clientId`, `origin` | Who asked for what |
| `baseRevision`, `revision` | The revision the request was based on, and the session revision of the answer |
| `requestId` | The client's own correlation id, if it sent one |
| `progress`, `progressText` | A fraction and a line of text, only while the handler reports them |
| `startedAt`, `finishedAt` | ISO 8601 timestamps with milliseconds, UTC |
| `transactionId` | The boundary the command ran in |
| `grantedCapabilities`, `requiredCapabilities` | A **snapshot** of the client's capabilities at dispatch time, plus what the operation declared: a later revocation must not rewrite what a finished task was allowed to do |
| `cancellationRequested` | Whether a cancellation was asked for (see below) |
| `warnings`, `errorCode`, `errorMessage` | The outcome as the result carried it |
| `result` | The full result of the operation, so `task.describe` can hand a client the answer it may have missed |

The terminal state is **derived from the result**: `cancelled` when the result's error code is `cancelled`, `failed` for
any other error, `completed` otherwise. A task that answered successfully was not cancelled, whatever was requested
while it ran - which is why `cancellationRequested` is observable but does not by itself make a task `cancelled`.

The registry keeps a **bounded history** (`defaultHistoryLimit`, 64) of finished tasks and their full results. A task ID
that fell out of that history is answered `invalidated_object`, not `unknown_object`: the ID was real, and the client is
meant to re-query rather than to fix its request. Running tasks are never dropped.

### 9.2 Cancellation

* Cancellation is **cooperative**. `task.cancel` sets a flag; a handler observes it through the ambient task
  (`isCancellationRequested()`, `throwIfCancellationRequested()`) and answers `cancelled` when it gives up. A handler
  that never checks cannot be interrupted, and the layer does not pretend otherwise.
* Cancelling a task that has **already finished** is a **success with a warning** and no state change: the client wanted
  it to stop and it has.
* Cancelling a task of **another client** is `invalid_argument`, naming both client IDs. `task.control` allows a client
  to stop its own work, not someone else's.
* `task.cancel` is a command like any other, so it has its own task and its own transaction boundary.

### 9.3 Waiting

In this phase a client waits by **polling** `task.describe` (or by watching the events of §11). A blocking `await` and a
push subscription belong to the Phase 3 transport.

## 10. Transactions and undo

* **Every command runs in a transaction boundary.** A client that opened none implicitly gets one around its single
  command, and it becomes **one undo step** under the descriptor's `undoLabel`. A command that fails leaves no partial
  change: the boundary it owns is aborted, and inside a boundary someone else opened only its own share is taken back
  (`mark()` / `revertTo()`), because the commands before it are theirs to keep.
* A **session-level group** (`beginTransaction(label)` … `commitTransaction()` / `abortTransaction()`) collects several
  commands - from any client, not only the one that opened it - into one undo step, which is what a multi-command plan of
  a later phase needs.
* A boundary records `id` (`transaction:x<N>`), `state` (`open`, `committed`, `aborted`), `label`, `clientId`, `origin`,
  `commands[]` (the operation IDs that joined it), `undoable`, `recordedOperations`, `openRevision` and `closeRevision`.
  `closeRevision` is the revision the closing result carried, because the revision is bumped before the boundary is
  committed.
* `undoable` means "recorded through a user interface": a session without one (a batch job, a contract test) still has
  boundaries and records, but `recordedOperations` is zero and nothing reaches an undo stack. A command whose mutation
  is not undoable produces an empty compound operation, which commit skips - it does not fail.
* The recording scope is released **before** the boundary is committed: `UndoStack::push()` asserts that nothing is being
  recorded.
* A boundary object stays alive after it closed, so a caller holding it does not hold a dangling pointer. Its record
  belongs to the registry from then on, and a caller that needs the record keeps the ID it read before closing and looks
  it up again - after `close()`, the object itself carries no record.

## 11. Events and observability

| Field | Meaning |
|---|---|
| `kind` | `session.changed`, `task.started`, `task.progress`, `task.finished` or `activity` |
| `sequence` | A monotonic number starting at 1, assigned by the log and never reused, even after older entries are dropped |
| `revision` | The session revision at the time |
| `timestamp` | ISO 8601 with milliseconds, UTC |
| `origin` | `user`, `qml`, `cli`, `ai` or `python` |
| `clientId`, `taskId`, `transactionId`, `operationId` | Present when they apply |
| `summary` | A short human-readable line, e.g. `Command 'pipeline.add_modifier' was dispatched.` |
| `details` | Structured extras, e.g. the capability a grant named |

* The log is **in memory only** and holds the last 256 events by default. Sequence numbers keep increasing after older
  entries are dropped, so a client can tell "nothing happened" from "I missed something":
  `firstRetainedSequence` and `lastSequence` are in every `event.list` answer and `events(since, limit)` takes `since`
  as **exclusive**.
* An event carries **IDs, counts, operation names and the summaries above - never an argument value, a file path or a
  data-derived value**. This is a rule with a test
  (`events_and_task_records_keep_no_argument_values`): an operation dispatched with a path-like argument leaves no trace
  of it in any event, in any task record, or in the payloads of `task.describe` and `event.list`.
* Origin and client attribution exist so that a later AI agent can read "who changed this, and did I do it". What is
  *not* recorded is raw mouse and keyboard input, which is what the design requires and this log simply cannot express.
* One signal, `AutomationEventLog::eventAppended(sequence)`, is the hook a Phase 3 endpoint turns into a subscription;
  nothing else in the layer pushes.

## 12. The viewport view-type vocabulary

A client switches on the view type of a viewport, so each value the user interface can set has exactly one name:
`top`, `bottom`, `front`, `back`, `left`, `right`, `ortho`, `perspective`, `scene-node`. The vocabulary is frozen and
`QMetaEnum`'s own key is deliberately not used (it would be `VIEW_SCENENODE`). `Viewport::VIEW_NONE` is the placeholder
of an unset property and no viewport a client can observe has it; the mapping still answers `none` for it. A new enum
value must get a name here and in the mapping rather than fall through to a default a client cannot distinguish from a
real answer.

## 13. The Python package contract

### 13.1 Identity and envelope

| Item | Value |
|---|---|
| Handshake message name | `ovito.automation.handshake` |
| Package protocol version | `1.0` (`protocolVersionMajor` 1, `protocolVersionMinor` 0) |
| Package name | `ovito` |
| Interpreters | CPython 3.10 - 3.13 inclusive; another implementation or version is `python_unsupported` |
| Platforms | `linux`, `darwin`, `win32`; architectures `x86_64`, `AMD64`/`amd64`, `arm64`/`aarch64` |

The major version must match; a newer minor is acceptable because §2's additive rule applies to this protocol too.
Features, not versions, decide what may be used:

| Feature | Wire name | Meaning |
|---|---|---|
| `SchemaIntrospection` | `schema.introspection` | Extract decorator metadata without executing the file |
| `FunctionInplace` | `function.inplace` | Run a decorated function with writable input and a `None` return |
| `ParameterScalar` | `parameter.scalar` | Declare and transmit `bool`/`int`/`float`/`str` parameters |
| `ArrayBuffer` | `array.buffer` | Transfer numeric arrays through the buffer protocol, without JSON or pickle |
| `SharedMemoryArray` | `array.shared-memory` | Transfer numeric arrays through a shared-memory segment both sides map |
| `TaskCancellation` | `task.cancellation` | Observe a cancellation request cooperatively |
| `TracebackMapping` | `traceback.mapping` | Report an exception as structured file/function/line information |

The application requires `schema.introspection`, `function.inplace` and `array.buffer` by default. A feature name this
build does not know is **retained** in `PythonPackageInfo::unknownFeatures` rather than dropped, so that a newer package
can be described without being misread.

### 13.2 What the environment has to answer

The probe starts the selected interpreter (isolated with `-I` unless a caller explicitly asks otherwise) and reads the
package probe script's handshake:

```
{ "handshake": "ovito.automation.handshake", "protocolVersion": "1.0",
  "python":  { "implementation", "version", "versionMajor", "versionMinor", "versionMicro", "releaseLevel",
               "executable", "prefix", "basePrefix", "platform", "machine", "architecture", "cacheTag",
               "freeThreaded", "isolated", "maxsize" },
  "package": { "name", "found", "imported", "moduleFile", "version", "protocolVersion", "features", "bridge", "error" },
  "features": [ ... ] }
```

The package side of it comes from `from ovito.automation import handshake` returning that `package` mapping - the
installed package's own entry point, which Phase 4 ships (O16). A package that is *found* but cannot be *imported* is
reported with its error text, never as absent: "not installed" and "installed and broken" call for different repairs.

### 13.3 Validation order and probe statuses

`PythonHandshake::fromJson` only reports **structural** problems (a missing or wrong protocol name, an unparsable
protocol version, an incomplete interpreter description) and never applies compatibility rules; those belong to
`PythonEnvironmentProbe`, which validates in this fixed order and answers with exactly one status:

`not_configured` → `interpreter_missing` → `interpreter_failed` → `timeout` → `protocol_error` → `interpreter_mismatch` →
`python_unsupported` → `platform_unsupported` → `package_missing` → `package_incompatible` → `feature_missing` →
`compatible` (plus `cancelled`, which is the caller's own outcome rather than a verdict about the environment).
A caller that gets a status therefore knows that everything before it in the list held.

### 13.4 What the probe promises, and what it refuses to do

* It **never installs, upgrades or falls back**. It does not substitute another interpreter, does not retry with a
  different flag set, and does not treat a missing package as an invitation to use the system Python. A caller that
  wants a different environment selects one and probes it.
* It identifies the interpreter by **canonical path**, and accepts a virtual environment whose executable lies inside a
  prefix differing from `base_prefix` (that is what a venv looks like) while rejecting a different interpreter that
  merely answered on the same command line.
* It lists the metadata first (`importlib.util.find_spec`) as an admission step and treats the import as the
  authoritative check.
* The probe is about an **installation**, and it decides whether an environment may be used at all. The worker handshake
  of §14.3 is about a **process** and decides what that process can do. Neither substitutes for the other.

## 14. The Python worker protocol

### 14.1 Framing

* One UTF-8 JSON object per line over the worker's standard input and output. A request or a reply may announce
  `"bytes": N` in its header line, in which case exactly `N` **raw** bytes follow it. The reader consumes those bytes
  before it queues the request, which is what keeps array bytes from ever being mistaken for a message; a header that
  cannot be parsed while a payload was announced leaves the stream desynchronized and is a defect, not a supported
  state.
* The header's protocol fields (`id`, `op`, `bytes`) are written **last**, so an argument map cannot accidentally
  redefine them. `id` correlates an answer with its request; `op` names the operation.
* **No version of its own**: the envelope's compatibility gate is the major number of the *package* protocol of §13.1,
  because a framing change an older client could not tolerate has to move that number.
* No array ever travels as JSON or base64 (D49), and the protocol has no file, socket or network access of its own.

### 14.2 Two error vocabularies

| Kind | Codes | Meaning |
|---|---|---|
| Worker | `unknown_operation`, `invalid_argument`, `unsupported_transfer`, `cancelled`, `internal_error` | The worker understood the envelope and refused the work |
| Client (`PythonWorkerProcess`) | `worker_unavailable`, `protocol_error` | The exchange itself failed: the interpreter died, the answer was not JSON, the announced payload never arrived |

A caller that conflates them cannot tell "your request was wrong" from "the worker is gone", which is exactly the
distinction a frontend has to show.

### 14.3 The worker handshake

The handshake is the runtime flavour of the package handshake: same protocol identity, different subject. It answers
`implementation`, `version`, `executable`, `pid`, `numpy` (null when absent), `features`, `transferModes`,
`protocolVersion` and `handshake`. A client validates it against the package contract by major version and a newer minor
is acceptable. Its transfer modes are **advertised from availability** - `array.shared-memory` only with numpy,
`multiprocessing.shared_memory` and a POSIX platform - so a client skips an unadvertised mode instead of failing a
check, and `array.buffer` is advertised unconditionally because the framed transfer needs only the standard library's
`array` module.

### 14.4 Operations

| Operation | Meaning |
|---|---|
| `handshake` | What this process is and can do (§14.3) |
| `ping` | A round trip, for latency measurements |
| `process` | The topology spike's synthetic computation |
| `sleep` | Sleep for a duration; the cancellation test's target |
| `arrays` | The data bridge of §15 |
| `cancel` | Abandon one in-flight request, answered by the reader thread so it overtakes the operation it stops |
| `crash` | Exit abnormally, on purpose: the crash-recovery check |
| `quit` | Stop cleanly, exit code 0 |

`ovito_worker.py` stays in the tree as the protocol's reference implementation; the installed package's own entry point
replaces it on the same wire in Phase 4 (O16). The script is a spike-shaped worker in one respect only: it serves one
process, one interpreter, serially, with the reader thread handling `cancel` inline and queueing everything else.

## 15. The data bridge

### 15.1 The array descriptor

| Field | Meaning |
|---|---|
| `name` | The array's name; unique within one block, because it is how a result is matched to an input |
| `dtype` | One of `float64`, `float32`, `int64`, `int32`, `uint32`, `int8`, `uint8` |
| `shape` | The dimensions, in row-major order |
| `bytes` | The payload length, which must equal the shape's element count times the element size |
| `sha256` | The digest of the bytes |

There is deliberately **no stride**: a wire cannot express one, and a wire that cannot express something is worse than a
copy. Bytes are in the host's native representation, which every supported platform (x86_64, ARM64) makes little-endian.
An unknown dtype is **refused, never converted** - silently truncating a type a receiver does not understand is how a
pipeline produces wrong numbers.

### 15.2 The exchange

* An exchange sends a block of named arrays with an `operation` (`scale` or `identity`) and, for `scale`, a `factor`;
  it returns `arrays` (descriptors), `bytes`, `operation`, `factor`, `inputSha256` (the digest of what actually arrived
  for each input) and `elements`.
* **`scale` requires the first input array to be `float64`**, returns it as `<name>_scaled` multiplied by `factor`, and
  echoes every remaining input **byte for byte** as `<name>_echo`. That determinism is the point: a caller can predict
  the output exactly and therefore verify the whole exchange rather than only its envelope.
* Bounds, all refused before anything is written: 64 arrays, 512 MB of payload, a finite `factor` within 1e12, at least
  one array, every array valid.
* The **digest is computed fresh** every time an array is written and verified on arrival, in both directions. It is
  what makes "the bytes that arrived are the bytes that were sent" a fact rather than an assumption.
* **Ownership is a copy, in both directions.** No pointer into OVITO's buffers, no shared memory and no ownership
  transfer crosses this boundary in this phase: inputs are a read-only copy of the caller's buffers, outputs are copied
  into the caller's own block, and an exchange leaves the caller's block untouched. The shared-memory transport of the
  topology spike is not part of this client; the pooled zero-copy version belongs to Phase 4 (O17).
* Every refusal **names the array it is about** - on both sides of the seam. A missing name, an unknown type, a shape
  that disagrees with the byte count, a duplicate name, a truncated or over-long payload, a digest that does not match:
  each answers `invalid_argument` with the offending array named, and the client's own block validation answers
  `invalid_block` / `invalid_reply` alongside the worker's codes (§14.2).
* The adapter from an OVITO property buffer to a `PythonArray` is **not** part of this contract yet: what a modifier
  receives and may write is Phase 4 (O21).

### 15.3 The client

`PythonWorkerProcess` is the synchronous half: blocking, single-threaded, main-thread-only, one interpreter chosen by the
caller, isolated with `-I`, at most one unanswered request at a time (a second `send()` before `receive()` is a
programming error), `stop()` asking the worker to quit before it kills it, and `exitCode()` reporting a stopped worker's
last code so that a clean stop is distinguishable from a kill. The asynchronous event-driven client belongs to the
Phase 3 transport.

## 16. The schema preview

* `ovito_schema.py` prints **exactly one** JSON object per run: `schemaVersion` (currently **1**), `ok`, `mode`
  (`ast` or `import`), `path`, `moduleDocstring`, `imports`, `functions`, `diagnostics`, plus an `import` section in
  import mode only. It exits 0 whenever a report was printed; a non-zero exit means the script could not run at all.
* `PythonSchemaPreview::fromJson` accepts only a report whose `schemaVersion` is present and equal to 1 and that has the
  `functions` and `diagnostics` lists; anything else is a `protocol_error`. The version rule is deliberate: a report
  format this build does not know is not guessed at.
* **Diagnostics are the single failure channel.** A syntax error, an unreadable file, a dead interpreter and a refusal
  are all diagnostics; an `error`-severity diagnostic makes `isOk()` false, and `summary()` then returns that reason.
  A report can therefore never be "not ok" without saying why.
* Each function is `plain`, `supported` or `unsupported`, with its decorators and its parameters. A parameter carries
  `kind` (`positional_only`, `positional_or_keyword`, `keyword_only`, `var_positional`, `var_keyword`), `annotation`,
  `default`, `required` and `defaultDynamic`.
* **Annotations stay the author's written source text** and are never resolved to type objects: resolving them would
  execute the module. A default that is computed at run time is not reported as a value - the parameter is required and
  `defaultDynamic` is true.
* A decorator whose identity or configuration is computed at run time is reported `unsupported` with a reason
  (`dynamic_decorator_argument` is the warning code) rather than guessed.
* Diagnostic codes: `syntax_error` (with `line`, `column`, `endLine`, `endColumn` and the offending text),
  `unreadable_file`, `import_failed` (with traceback frames carrying `file`, `line`, `function`, `text` and
  `fromUserScript`), `not_importable`, `internal_error` from the script, plus the introspector's own
  `interpreter_missing`, `script_missing`, `interpreter_failed`, `protocol_error`, `timeout` and `cancelled`.
* **The AST mode is the default and the only mode a file selection may use**: reading a schema executes nothing, which
  is proven by a fixture whose top level writes a marker file. The explicit `--import` mode states that the caller has
  the user's consent to execute the file's top-level code, and it is what maps a traceback of the user's own frames.

## 17. The local transport (prototype) and the discovery convention

### 17.1 The descriptor (normative, in Core)

| Field | Meaning |
|---|---|
| `sessionId` | The session's identity, and the name of its endpoint; only `[A-Za-z0-9_-]`, at most 96 characters |
| `processId` | The process that owns the session, for the liveness question |
| `endpoint` | The **absolute** path of the local socket, inside the owner-only directory |
| `startedAt` | When the session started |
| `contractVersion` | The automation contract version the descriptor was written by |
| `discoveryVersion` | `1` - the convention's own version, independent of the contract version (files: `*.session.json`) |

* The directory is a per-user runtime location, overridable with `OVITO_AUTOMATION_SESSION_DIR` for tests and for a
  private discovery scope. It is created with owner-only permissions, and a descriptor is written through a sibling
  temporary file with owner-only permissions and read with a size cap.
* `discover()` lists descriptors newest-first and reports unusable files **by name** instead of failing.
* Staleness is decided by asking whether the owning process still exists (POSIX `kill(pid, 0)`, where a permission error
  counts as alive; a platform without that check answers "alive") **and** by what happens when a client tries to connect.
  A descriptor can outlive its session - a killed workbench cannot clean up - and the convention does not pretend to be a
  liveness guarantee.
* The socket is an absolute path inside that directory rather than a bare name: Qt resolves a relative `QLocalServer`
  name under the shared temporary directory, where another user could squat on the name. The path stays short enough for
  the 104-character limit of a UNIX socket path on macOS.

### 17.2 The wire

One UTF-8 JSON object per line. Request types: `hello`, `ping`, `dispatch`, `snapshot`, `subscribe`, `unsubscribe`,
`capture`, `bye` (and `quit`, which the endpoint accepts only when started with that permission). Answers are
`{type: "reply", ...}`, pushed messages are `{type: "event", ...}`, and a client routes by that `type`. Limits and their
refusals: 64 KB per request line, 4 simultaneous connections, 200 scene nodes in a snapshot (with an explicit
truncation flag), 256 events per reply, a capture of at most 4096×4096 and an artifact of at most 4 MB.

* The transport is a local socket by construction - a named pipe on Windows, a UNIX domain socket elsewhere. There is no
  TCP listener and no way to configure one, so "network listening is disabled by default" is not a setting that can be
  wrong.
* A client asks for capabilities in its `hello`; the endpoint grants the intersection with its policy and answers with
  **both** the grant and the refusal, because a client that is silently given less than it asked for will misreport.
* The `snapshot` is composed of the contract's own read operations (`session.describe`, `scene.list_nodes`,
  `pipeline.describe`, `task.list`, `event.list`) rather than reimplementing them, with the node clamp above.
* A `capture` returns a bounded, in-memory PNG artifact (`mediaType`, `width`, `height`, `bytes`, `sha256`, `base64`)
  or the deterministic `render_unavailable`. **No file is written by the endpoint**; a client may write the artifact to
  disk itself, which is where the permission for that belongs.
* Arrays never travel over this endpoint: D49 governs them, and the endpoint has no array transport at all.

### 17.3 Two error layers (normative rule)

A reply is either a **transport failure** (`ok: false` with a transport code: `invalid_message`, `too_large`,
`not_connected`, `unsupported_message`, `too_many_clients`, `render_unavailable`, `internal_error`) or a successful
exchange carrying the contract's own result, whose `ok` may still be false with an error code of §3.5. The two
vocabularies are disjoint on purpose: the first means "the session did not understand the request", the second means
"the session understood it and refused the operation".

### 17.4 The command line client (`ovito --automation`)

The first client of this transport is a mode of the shipped binary rather than a second program (audit decision D60):

```
ovito --automation list      [--json] [--session <id>]
ovito --automation status    [--json] [--session <id>] [--limit <n>]
ovito --automation describe  <object-id> [--json] [--session <id>]
ovito --automation snapshot  [--json] [--session <id>] [--max-nodes <n>]
ovito --automation events    [--json] [--session <id>] [--since <n>] [--limit <n>]
```

* **It is a read-only client.** It asks for `session.read`, `scene.read`, `selection.read` and `file.read`, and for
  nothing else: there is no option that widens the request, and no verb that changes anything, so it cannot execute
  Python or write a file even against a session that would grant it.
* **`list` never connects.** Discovery is a filesystem convention (§17.1), so a user can see what is running - including a
  session that does not answer - without disturbing any of it.
* **Discovery is per session directory**, taken from `OVITO_AUTOMATION_SESSION_DIR` when it is set, which is how a test
  gets a private scope of its own.
* **`--json` prints one JSON object per invocation** (`{"ok": true|false, "verb": ..., ...}`), with the contract's own
  result payload under `session` / `viewports` / `selection` / `tasks` / `snapshot` / `object` / `events` and the
  capabilities the session granted. Without `--json` the same answer is printed as text for a human.
* **The exit code says what happened**: `0` the verb answered, `1` the session refused the request or the exchange broke,
  `2` the request could not be carried out at all (no running session, no session matching `--session`, an unknown verb or
  a missing argument). A script reads the code and, on failure, the `error.code` of the JSON answer - which is how
  `no_session`, a transport failure and a contract error stay distinguishable.
* **`status` reports its optional parts as far as the session grants them.** `session.describe` is required, while the
  viewport list, the selection and the tasks are asked for and included when they are answered; a refused
  `selection.describe` appears as `count: 0` with `unavailable: true` and the session's own `error` rather than as an
  empty selection, so a caller that reads only the payload can tell "nothing is selected" from "not allowed to look".
* **The session it talks to must have been asked to serve.** A workbench publishes its session only when it was started
  with `--automation-serve`, in which case it grants connecting clients the read capabilities of D43 (audit decision
  D59); the command line says so when it finds nothing.

### 17.5 What is not proven here

The rendering half of a capture is unproven: a real image needs a graphics device and this tree has no headless QPA
plugin, so the artifact's transport, bounds and bytes are certified while the render behind them is not (O19, Phase 5).
The endpoint serves one connection at a time, has no backpressure beyond the socket buffer, no resume beyond a `since`
sequence, and no authentication beyond the permissions of the directory and the socket; it has been measured on Linux
x86_64 only (macOS caps a socket path at 104 characters, Windows uses named pipes without permission bits) - O20, whose
remaining half is a production transport for a *remote* client, which no phase has designed.

## 18. What this contract does not promise

* **No rollback of computations or side effects.** A transaction rolls back *scene mutations* recorded through a user
  interface. A file a command wrote, a Python function that already ran, a network request that was made: none of them
  come back. A client that needs an atomicity it cannot get here must design around it.
* **No security sandbox.** User Python runs with the selected interpreter's full capabilities; the layer's protection is
  that `python.execute`, `file.write`, `network.access` and `process.execute` are never granted by default and need
  explicit consent (design §3.5).
* **No interruption of arbitrary code.** Cancellation is cooperative; a Python function that never yields cannot be
  stopped from the outside.
* **No remote access.** Everything here is a local client of a local session; there is no authentication, no
  authorization server and no transport encryption. Remote access is undesigned (design §6 item 6).
* **No persistence.** Capabilities, tasks, transactions and events live and die with the session process.
* **No array transfer over the control transport**, and no promise of zero-copy anywhere in this phase.
* **No unqualified "implemented" for a later feature.** An operation declared without a handler is `not_supported`, and
  no capability makes it work; no later phase's feature is marked implemented by this phase's exit gate.

## 19. Verification, and the exit gate of Phase 2.6

The six suites run in `ctest --preset native` (12/12, 107 cases) and again in the assertion-enabled tree of
[UI_TEST_ENV.md](UI_TEST_ENV.md) §4:

| Suite | Cases | Covers |
|---|---|---|
| `tst_automation_contracts` | 31 functions, 33 cases | Vocabulary, wire types, identity grammar and lifetime, revision preconditions, dispatch order, the catalog, tasks, transactions, permissions, undo/redo identity survival, a presentation refresh, and the rule that events carry no argument values |
| `tst_python_environment_probe` | 20 functions, 22 cases | Every probe status, in the fixed order of §13.3, and the absence of silent fallback |
| `tst_python_data_bridge` | 16 functions, 18 cases | The array descriptor rules, the framed round trip with recomputed digests, an 8 MB payload, the refusals of both sides, a dead interpreter reported by exit code, a graceful stop, a cancellation |
| `tst_python_schema_preview` | 15 functions, 17 cases | The report value type, AST mode executing nothing, an import mode that does, diagnostics with positions and mapped tracebacks, every exchange failure |
| `tst_session_descriptor` | 8 functions, 10 cases | The discovery convention of §17.1: allocation, round trip, permissions, staleness, unusable files |
| `tst_automation_cli` | 5 functions, 7 cases | The command line of §17.5: its read-only capability request, an empty and a stale discovery scope, the exit code of every failing invocation, and one JSON object per invocation |

Two spike programs provide the evidence the suites cannot: `ovito-automation-spike` (topology and transfer: 93 checks with numpy and 89 without,
all passing), `ovito-automation-ipc-spike` (`24/24` checks: discovery, handshake and refusals, a
bounded snapshot, dispatch, a stale revision, an unknown operation, a missing capability, malformed and unsupported
messages, a subscription, an artifact verified byte for byte, `render_unavailable`, an oversized capture, the client
limit, both shutdown modes and a stale prune).

The exit gate of the phase, item by item:

| Exit-gate requirement | Where it is answered |
|---|---|
| The shared contracts are versioned and documented | §2 (version and compatibility rule) with this document as the prose half and the suites as the executable half (O13 closed) |
| IDs survive a presentation refresh and re-resolve after undo/redo | `ids_survive_a_presentation_refresh`, `ids_survive_undo_and_redo`, `registry_keeps_ids_stable` |
| Deleted objects and data-set replacement invalidate deterministically | `registry_invalidates_all_without_reusing_numbers`, `registry_survives_a_deleted_object_becoming_a_new_one`, `session_revision_tracks_the_data_set` |
| Stale `baseRevision` requests are rejected | `gateway_checks_capabilities_and_revisions` |
| Task, event, transaction and capability schemas have deterministic tests | `task_lifecycle_reports_progress_and_cancellation`, `task_cancel_requires_control_and_ownership`, `event_log_is_ordered_and_bounded`, `transaction_*`, `permission_grants_are_recorded_and_cannot_escalate` |
| The selected Python environment can be probed without silent fallback | §13.4, `tst_python_environment_probe` |
| The worker/embedded benchmark has evidence | [AUTOMATION_TOPOLOGY_SPIKE.md](AUTOMATION_TOPOLOGY_SPIKE.md), D49 |
| A local client can perform only the bounded read-only snapshot/PNG spike | §17, [AUTOMATION_IPC_SPIKE.md](AUTOMATION_IPC_SPIKE.md), the `24/24` self-test |
| No later feature is marked implemented by this gate | §18, and the "Formal feature placement after Phase 2.6" table of [UI_PLAN.md](UI_PLAN.md) |

What the phase leaves open is recorded in [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md) §5 rather than hidden here: O13 (this
document) is closed; O14 is resolved; O15 is a pre-existing numbering defect of the decision record; O16, O17 and O21
(the package-location seam, the unmeasured pooled shared-memory and embedded halves, the missing property-array adapter)
belong to Phase 4; O18 (the layer's missing caller) was closed by Phase 3, which attached the session to every workbench
and put the command line and the serving frontend on top of it; O19 (the render half of a capture) belongs to Phase 5 and
the remaining half of O20 (a transport for a remote client) belongs to no phase yet.
