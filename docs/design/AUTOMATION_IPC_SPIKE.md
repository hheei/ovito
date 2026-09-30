# Local Protocol Spike (Phase 2.6, deliverable 8)

**Status: executed and recorded.** This document holds the evidence behind the Phase 2.6 decision on how a local client -
a CLI, an AI agent, a script - finds a running workbench and talks to it. The prototype that produced the numbers is
`ovito-automation-ipc-spike`, built from `src/ovito/core/automation/spike/` (the endpoint, the client and the program
that drives them) on top of the value type `src/ovito/core/automation/AutomationSessionDescriptor.cpp`, which is part of
Core and has a test suite of its own. The decision it supports is recorded as **D50** in
[UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md) §7 and summarized in
[AUTOMATION_AND_PYTHON_PIPELINE_DESIGN.md](AUTOMATION_AND_PYTHON_PIPELINE_DESIGN.md) §6 item 6.

The question, from the design (§4.3 and §4.5): *can a local client discover one live workbench, query a bounded session
snapshot, observe task/scene events, and request a displayed-view PNG - with network listening disabled by default, no
arbitrary filesystem writes, and deterministic protocol errors?*

---

## 1. What was built, and what each piece is for

| Piece | Where it lives | What it is |
|---|---|---|
| Session descriptor | `core/automation/AutomationSessionDescriptor` (**Core**, tested) | The discovery contract: identity, owning process, socket path, start time, contract version; written with owner-only permissions into a per-user directory, scanned by a client, and classified as stale when its process is gone |
| Endpoint (server) | `automation/spike/ipc/AutomationLocalEndpoint` (spike) | A `QLocalServer` speaking one JSON object per line, one gateway per connection, capability policy per connection, event push, bounded snapshot composition, capture with an in-memory artifact |
| Client | `automation/spike/ipc/AutomationLocalClient` (spike) | Discovery, connect, handshake, request/reply by ID, subscription, pushed events, capture, plus the stale-descriptor rules a client has to apply |
| Spike program | `automation/spike/IpcSpikeMain.cpp` | `--serve` (host a session), `--list [--prune]` (what a client can see), and the default self-test that starts servers of its own and checks 24 rules against them |

The descriptor is deliberately the only piece in Core: discovery is a filesystem convention that a later CLI, an MCP
adapter or a helper tool needs whether or not it speaks the socket protocol, and it is testable without a socket, an
event loop or an application object (`tests/cpp/core/automation/tst_session_descriptor.cpp`, 10 cases). The endpoint and
the client are spike code and are expected to be replaced in Phase 3; what should survive them is the *contract*, which
is why it is written down here rather than only implemented.

## 2. The protocol

Every message is one JSON object on one line. The envelope of an answer is
`{"id": <from the request>, "type": "reply", "contractVersion": "0.2", "transport": "jsonl/1", "ok": true|false,
"result": {...}}` or, with `ok: false`, `"error": {"code": "...", "message": "...", "details": {...}}`.

| Message | Direction | Meaning |
|---|---|---|
| `hello` with `client`, `origin`, `protocol`, `capabilities` | client → endpoint | Identity, the origin the activity is attributed to, and the capabilities the client asks for |
| `dispatch` with `operation`, `arguments`, optional `baseRevision` | client → endpoint | One operation of the automation contract (Phase 2.6's gateway catalog) |
| `snapshot` with optional `maxNodes` | client → endpoint | A bounded composition of `session.describe`, `scene.list_nodes` and one `pipeline.describe` per pipeline, plus the retained tasks and events |
| `subscribe` with `since` / `unsubscribe` | client → endpoint | Hand over the retained events and push new ones, or stop pushing |
| `capture` with `width`, `height` | client → endpoint | An image artifact (below) |
| `ping` | client → endpoint | A round trip for latency measurements |
| `quit` / `bye` | client → endpoint | Stop the endpoint (only when it was started with `--allow-quit`), or end this connection |
| `event` with `event` | endpoint → client | A pushed session event, in the form `AutomationEvent::toJson()` defines |
| `reply` | endpoint → client | The answer to a request, by ID |

Two error vocabularies, deliberately disjoint:

* **Transport errors** (`invalid_message`, `too_large`, `not_connected`, `unsupported_message`, `too_many_clients`,
  `render_unavailable`, `internal_error`): the endpoint did not understand the message, or cannot answer at all.
* **Contract errors** (`unknown_operation`, `invalid_argument`, `missing_capability`, `stale_revision`,
  `unknown_object`, `invalidated_object`, `not_supported`, `cancelled`, `internal_error`): the message was understood
  and the *operation* was refused. These arrive inside a successful reply's embedded contract result, never as the
  envelope of the exchange. The distinction is what lets a client tell "I sent nonsense" from "the workbench refused".

The rule D49 fixed for arrays holds here without exception: this endpoint has no array transport. It carries control
messages and small values, and the one artifact it can return is bounded, encoded and in memory.

## 3. Discovery, and why the socket is a path

A running workbench writes its descriptor into a per-user directory (`QStandardPaths::RuntimeLocation`, or
`OVITO_AUTOMATION_SESSION_DIR` when set) as `<sessionId>.session.json`, with the directory at `0700` and the file at
`0600`, by writing a sibling file and renaming it, so a scanner sees either nothing or a complete descriptor (`stale` is
decided by asking the operating system whether the owning process still exists).

The endpoint's socket lives in the same directory, and that is a decision rather than an accident: `QLocalServer::listen()`
places a socket created from a **relative** name in the system's temporary directory, which every user of the machine can
write to, so a client that connected to such a name could be talking to whoever got there first. The descriptor therefore
carries the complete socket path inside the owner-only directory, and a client connects to exactly that path. `0700` on
the directory, `srwx------` on the socket and `0600` on the descriptor are what keep other users out; the design's
"secure permissions so other local users cannot control the session by default" is that, and nothing else.

## 4. Results

Recorded on x86_64 Linux (Linux Mint 22.3, AMD Ryzen 9 9950X), native release build, Qt 6.10.2, one process per server,
`--scope` in a private temporary directory. The self-test ran **24 checks, 24 passed, 0 failed**.

| Measurement | Value |
|---|---|
| Connect (discover → connected) | 0.057 ms |
| Round trip, median of 20 | 0.010 ms |
| Bounded session snapshot (session + 1 node + 1 pipeline + tasks + 13 events) | 0.18 ms |
| One dispatched operation, end to end | 0.047 ms |
| Event push latency after the triggering dispatch | 0.0016 ms |
| Capture request → verified artifact (64×48 synthetic image) | 0.38 ms |
| Artifact | 213 bytes of PNG, SHA-256 recomputed by the client, dimensions verified |

The 24 checks are the protocol's rules, each stated as an assertion rather than a description: a client discovers the
session and its descriptor is not stale; a request before the handshake is refused with `not_connected`; the handshake
reports the client ID, the contract version and the protocol, and grants exactly the approved capabilities while naming
every refusal (a capability that does not exist, one that the policy does not approve); a client with nothing granted
cannot mutate anything (`missing_capability` naming `task.control`); a snapshot describes the session, its scene and its
events, and describes at most the number of nodes the endpoint allows; an operation is dispatched and its contract result
comes back; a stale `baseRevision` answers `stale_revision`; an unknown operation answers `unknown_operation` with the
catalog in its details; bytes that are not a message answer `invalid_message`; a message type outside the table answers
`unsupported_message`; a subscription hands over the retained events and then pushes new ones; a capture returns a
bounded artifact, and one with no image source answers `render_unavailable`; a capture beyond the accepted size answers
`invalid_argument`; the endpoint refuses a client beyond its limit with `too_many_clients`; a client can leave without
disturbing the session; a graceful shutdown removes the descriptor; a killed workbench leaves a stale descriptor that
`--prune` removes.

Two further observations, taken by hand because they need two live processes:

```text
$ ovito-automation-ipc-spike --serve --scope /tmp/ipctwo &   # twice
$ ovito-automation-ipc-spike --list --scope /tmp/ipctwo
  two sessions, the newer first, both "stale": false, no unusable files
$ ovito-automation-ipc-spike --list --scope /tmp/ipctwo --prune
  sessions: 2   pruned: 0
$ ls -la /tmp/ipctwo
  drwx------  .                                  (the session directory)
  srwx------  ovito-automation-...-6d108c7a     (a socket, owner only)
  -rw-------  ovito-automation-...-6d108c7a.session.json
```

## 5. Interpretation

* **A local client can do all five things the design asked for, at interactive latency.** Discovery, a bounded snapshot,
  a dispatched operation, events and an artifact all complete in well under a millisecond on this machine, and the whole
  self-test - three servers, two shutdown modes and 24 rules - runs in a few seconds.
* **The transport is local by construction, not by configuration.** There is no TCP listener to disable: the endpoint is
  a UNIX domain socket (a named pipe on Windows) inside a directory only its owner can enter, and the only way to reach
  it is to read a descriptor that has the same permissions.
* **The two error vocabularies earn their place.** The self-test found this the hard way: a client that treats every
  failure as a contract error cannot tell "I sent nonsense" from "you refused me", and the one case that mixes them (a
  connection refused before the handshake) is exactly where a client would otherwise wait forever for an answer that had
  already arrived. Keeping the transport envelope and the contract result separate is what makes the wait bounded.
* **The discovery convention is the part worth keeping.** It is a value type, it is tested without a socket, and it
  already answers the questions a real client asks: which sessions are here, which one is newest, which are leftovers,
  and which files are not ours at all.
* **What the artifact measurement does *not* show.** A generated image is not a rendered view, and this spike cannot
  produce one: rendering needs a QRhi, the tree has no headless QPA plugin (the audit's O1), and this program never
  creates a graphics device. What is measured is the *transport* half - PNG encoding, a bound on the artifact, the client
  verifying bytes and dimensions, and no file being written on the server side. The rendering half already exists and is
  exercised by the Qt Quick frontend's offscreen check under `xvfb`, which is where a displayed view is actually rendered
  and photographed; joining the two into one capture service is Phase 5 work on the shared offscreen service (D34).

## 6. Decision

Recorded as **D50**: the local client protocol of the automation track is a **per-user local socket (JSON Lines control
messages, artifacts bounded and in memory) discovered through a session descriptor in a per-user runtime directory**,
with **capabilities per connection** and **two disjoint error vocabularies**, and with the **production endpoint,
the CLI and any remote transport left to Phases 3-4**. Consequences for the later phases:

* The endpoint and the client of this spike are prototypes; Phase 3 implements the production transport, and the pieces
  it should keep are the descriptor (already in Core), the message table of §2, the capability handshake and the split
  between transport and contract errors.
* Discovery must stay a filesystem convention so that a helper tool can list sessions without connecting to one, and the
  socket must stay an absolute path inside the owner-only directory.
* The snapshot stays a *composition* of contract answers (session, nodes, pipelines, tasks, events) with explicit
  `truncated` flags, rather than a second implementation of what a session contains.
* A capture operation belongs to the same catalog as every other operation and returns bounded in-memory artifacts; the
  rendering behind it is the shared offscreen service, and the file is written by the *client* that asked for it, never
  by the endpoint.
* Nothing here weakens D49: arrays never travel as JSON, and a client that wants particle or mesh data gets it through
  the worker protocol's framed transport, not through this endpoint.

## 7. Limitations, and what is deliberately not claimed

* **No view is rendered, and no QRhi is created.** The capture path is measured with a generated image. Everything that
  depends on a graphics device - including "the displayed view" as opposed to "a render of the active viewport" - is
  unproven here and stays Phase 5 (see §5).
* **The endpoint is a prototype.** It serves one connection at a time, has no backpressure beyond the socket buffer, no
  request batching, no reconnect/resume beyond a `since` sequence, and no authentication beyond the file permissions of
  the directory and socket. A client that stops reading is noticed only when its buffer fills.
* **Only JSON-carried control and artifacts were measured.** The snapshot is composed of the built-in read operations of
  the Phase 2.6 catalog; a snapshot of a large scene is bounded by `maxNodes`, but the *cost* of describing a thousand
  nodes was not measured, and a client that needs a full scene dump is not what this spike certifies.
* **Results are from one machine and one platform.** Linux x86_64 with Qt 6.10.2; macOS (where the socket path length
  limit is 104 characters and `RuntimeLocation` is a per-user temporary directory) and Windows (named pipes, no
  permission bits) remain unverified. The descriptor's length limit is enforced, but no test runs on those platforms.
* **The capability policy is a command-line flag.** It stands in for the user's consent; the dialog, the persistence and
  the "who approved this" workflow are Phase 8 (design item 9), and the spike does not pretend to answer them.
* **Stale detection is best-effort by design.** On POSIX a descriptor is stale when its process is gone; elsewhere this
  build reports "alive" and lets the connection attempt decide, because deleting a live session's descriptor is worse
  than trying to reach it. A PID that was recycled makes a leftover look alive.

## 8. Reproducing

```bash
# Build the spike (native preset).
cmake --build --preset native --target OvitoAutomationIpcSpike

# The whole self-test: starts its own servers, runs the 24 checks, prints a summary and a JSON report.
LD_LIBRARY_PATH="$PWD/.qt/6.10.2/gcc_64/lib:$PWD/build-native/lib/ovito/plugins" \
  ./build-native/bin/ovito-automation-ipc-spike --json /tmp/ipc-spike.json

# Discovery by hand, with two live workbenches in one scope.
LD_LIBRARY_PATH="$PWD/.qt/6.10.2/gcc_64/lib:$PWD/build-native/lib/ovito/plugins" \
  ./build-native/bin/ovito-automation-ipc-spike --serve --scope /tmp/ipctwo &
LD_LIBRARY_PATH="$PWD/.qt/6.10.2/gcc_64/lib:$PWD/build-native/lib/ovito/plugins" \
  ./build-native/bin/ovito-automation-ipc-spike --serve --scope /tmp/ipctwo &
LD_LIBRARY_PATH="$PWD/.qt/6.10.2/gcc_64/lib:$PWD/build-native/lib/ovito/plugins" \
  ./build-native/bin/ovito-automation-ipc-spike --list --scope /tmp/ipctwo | python3 -m json.tool
pkill -f 'ovito-automation-ipc-spike --serve'; rm -rf /tmp/ipctwo
```

Exit codes: `0` when every check passed, `1` when one failed, `2` when no server could be started. The discovery rules
have their own CTest case (`tst_session_descriptor`), which needs no socket and no application object.
