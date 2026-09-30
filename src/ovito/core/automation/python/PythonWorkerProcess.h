// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/automation/python/PythonWorkerProtocol.h>

#include <QByteArray>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>

QT_BEGIN_NAMESPACE
class QProcess;
QT_END_NAMESPACE

namespace Ovito {

/**
 * \brief The client side of the Python worker protocol: one persistent interpreter and the two pipes it speaks over.
 *
 * This is a Core class, not a spike: the spike's client measured the seam and is allowed to be awkward, while this one
 * is the client the first data bridge (deliverable 7) and later the Python Function Modifier (Phase 4) use. What it
 * promises, and what the test suite pins:
 *
 *  - **The interpreted environment is the user's.** The worker is started with an interpreter the caller names, with the
 *    environment the caller gives, isolated with `-I` by default (no `PYTHONPATH`, no user site directory, no current
 *    directory on `sys.path`). This class never searches for an interpreter or installs anything; choosing the
 *    environment is \ref PythonEnvironmentProbe's job, and its verdict is not repeated here.
 *  - **A framed transfer, never JSON for data.** A payload is written verbatim after the header line and its size is
 *    announced in the header; the reply's announced payload is read in full before the reply is handed out. JSON carries
 *    control metadata and small scalars only.
 *  - **Death is a reported fact, not a timeout.** A read that ends because the interpreter exited answers
 *    `worker_unavailable` with the exit code and the tail of the worker's standard error, instead of waiting for the
 *    timeout. A worker that died is never restarted implicitly: the caller decides whether a restart is right (a
 *    restart is a user-visible event in the design, not a retry).
 *  - **One exchange at a time.** The worker runs one operation at a time and answers in order, so this client allows at
 *    most one unanswered request; \ref send "send()" returns the id that \ref cancel "cancel()" takes, which is how a
 *    long evaluation is abandoned. The asynchronous, event-driven form of this client belongs to Phase 3's transport.
 *
 * Not thread-safe, like the rest of the automation layer: one worker process per thread that uses it, and the caller
 * keeps it on one thread (in the workbench, the main thread).
 *
 * The shared-memory transport that audit decision D49 leaves as an optional optimisation of the same API is *not*
 * implemented here: the spike showed that a segment per evaluation costs more than it saves, so a pooled zero-copy
 * version belongs to the phase that implements the production worker (open item O17). The framed path is the contract.
 */
class OVITO_CORE_EXPORT PythonWorkerProcess
{
public:

    PythonWorkerProcess();
    ~PythonWorkerProcess();

    PythonWorkerProcess(const PythonWorkerProcess&) = delete;
    PythonWorkerProcess& operator=(const PythonWorkerProcess&) = delete;

    /**
     * \brief The worker script of this build, or an empty string when this build cannot locate it.
     *
     * Phase 2.6 runs the reference implementation that lives in the source tree (`ovito_worker.py` in the same
     * directory as the Python package contract's probe). Phase 4 replaces this seam with the installed package's own
     * entry point; see open item O16. Returning an empty string is deliberate: a caller that cannot find the worker
     * must say so, not silently use another interpreter's module.
     */
    static QString defaultWorkerScript();

    /// The interpreter to run. Required: this class never looks one up on its own.
    void setInterpreter(const QString& interpreter) { _interpreter = interpreter; }
    const QString& interpreter() const { return _interpreter; }

    /// The worker script to run. Defaults to \ref defaultWorkerScript.
    void setWorkerScript(const QString& script) { _workerScript = script; }
    const QString& workerScript() const { return _workerScript; }

    /// The environment of the worker process. Defaults to this process's environment.
    void setEnvironment(const QProcessEnvironment& environment) { _environment = environment; }
    const QProcessEnvironment& environment() const { return _environment; }

    /// Whether the interpreter runs isolated (`-I`), which is the default and what the design requires.
    void setIsolated(bool isolated) { _isolated = isolated; }
    bool isIsolated() const { return _isolated; }

    /// Additional interpreter arguments, inserted after the isolation flag. For test harnesses, not for callers.
    void setExtraArguments(const QStringList& arguments) { _extraArguments = arguments; }
    const QStringList& extraArguments() const { return _extraArguments; }

    /// Starts the worker. Returns false with a reason when the interpreter or the script cannot be started.
    bool start(QString* error = nullptr);

    bool isRunning() const;

    /// The interpreter's process id, or -1.
    qint64 processId() const { return _processId; }

    /// The command line of the running worker, for a diagnostic message.
    QString commandLine() const;

    /**
     * \brief Stops the worker the polite way: `quit`, then a kill if it does not oblige.
     *
     * A worker that is left behind holds the interpreter's memory and, in a test, the interpreter's startup cost; the
     * destructor therefore stops it as well, so nothing can leak by forgetting this call.
     */
    void stop(int timeoutMs = 3000);

    /// Kills the worker immediately, without asking it to stop.
    void kill();

    /// Waits for the worker to exit on its own.
    bool waitForFinished(int timeoutMs);

    /// The exit code of the worker, also after \ref stop "stop()" released the process object. -1 while it runs.
    int exitCode() const;

    /// What the worker wrote to standard error so far. A traceback of the worker itself ends up here.
    QString standardError();

    /**
     * \brief Asks the worker what it is, and returns its answer.
     *
     * `error` receives a reason when no answer arrived; `ok` is the worker's own verdict. A handshake that answers a
     * different protocol name or a major-version mismatch is reported as `protocol_error`, because using a worker this
     * client does not understand is not something a caller can recover from by trying again.
     */
    PythonWorkerHandshake handshake(int timeoutMs = 15000, QString* error = nullptr);

    /// Sends one request and waits for its answer. This is the whole round trip, timed.
    PythonWorkerReply exchange(const PythonWorkerRequest& request, int timeoutMs = 60000);

    /**
     * \brief Sends a request without waiting for its answer, and returns the request id it was given.
     *
     * Returns 0 when the request could not be sent. Only one request may be outstanding: a second \ref send "send()"
     * before \ref receive "receive()" is a programming error and answers 0 with \ref lastSendError() set.
     */
    quint64 send(const PythonWorkerRequest& request);

    /// Reads the answer of a request that was sent earlier.
    bool receive(PythonWorkerReply& reply, int timeoutMs = 60000);

    /// Whether a request has been sent whose answer has not been read yet.
    bool hasOutstandingRequest() const { return _outstandingId != 0; }

    /**
     * \brief Asks the worker to abandon a request that is still running, and waits for its acknowledgement.
     *
     * Cancellation is cooperative: the worker sees the request and stops at the next point it checks, which is why this
     * returns the worker's acknowledgement rather than a guarantee that nothing happened. The caller still reads the
     * answer of the cancelled request (with `cancelled` as its error code) through \ref receive "receive()".
     */
    bool cancel(quint64 targetId, int timeoutMs = 15000);

    /// Why the last \ref send "send()" failed, if it did.
    const QString& lastSendError() const { return _sendError; }

    /// The id the next request will be given. Monotonic within one worker process.
    quint64 nextRequestId() { return ++_nextRequestId; }

private:

    /// Reads one '\n'-terminated line, or fails with a reason.
    bool readLine(QByteArray& line, int timeoutMs, QString* failure);

    /// Reads exactly `count` bytes.
    bool readExact(qint64 count, QByteArray& out, int timeoutMs, QString* failure);

    /// Turns "nothing arrived" into a reason: the worker exited, or the read timed out.
    QString failureReason() const;

    /// Drains what the worker wrote to standard error into the member, truncated from the front.
    void collectStandardError();

    std::unique_ptr<QProcess> _process;
    QByteArray _buffer;
    QString _standardError;
    QString _interpreter;
    QString _workerScript;
    QProcessEnvironment _environment;
    QStringList _extraArguments;
    QString _sendError;
    /// The answer cancel() read while it waited for its acknowledgement, handed out by the next receive().
    std::optional<PythonWorkerReply> _deferredReply;
    quint64 _nextRequestId = 0;
    quint64 _outstandingId = 0;
    qint64 _processId = -1;
    qint64 _lastExitCode = -1;
    bool _isolated = true;
};

}   // namespace Ovito
