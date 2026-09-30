// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/automation/python/PythonWorkerProcess.h>

#include <ovito/core/automation/python/PythonContract.h>

#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QVersionNumber>

namespace Ovito {

/// The tail of a worker's standard error that a diagnostic message keeps.
static constexpr int maximumStandardErrorLength = 2000;

/******************************************************************************
* Life cycle.
******************************************************************************/
PythonWorkerProcess::PythonWorkerProcess()
    : _environment(QProcessEnvironment::systemEnvironment())
{
}

PythonWorkerProcess::~PythonWorkerProcess()
{
    stop();
}

QString PythonWorkerProcess::defaultWorkerScript()
{
#ifdef OVITO_AUTOMATION_PYTHON_DIR
    // Phase 2.6 runs the reference implementation from the source tree; Phase 4 replaces this with the installed
    // package's entry point (open item O16). An installation without the sources gets an empty string, which the
    // caller reports instead of substituting another module.
    const QFileInfo script(QStringLiteral(OVITO_AUTOMATION_PYTHON_DIR "/ovito_worker.py"));
    if(script.exists())
        return script.absoluteFilePath();
#endif
    return {};
}

QString PythonWorkerProcess::commandLine() const
{
    QStringList parts;
    parts << _interpreter;
    if(_isolated)
        parts << QStringLiteral("-I");
    parts << _extraArguments << _workerScript;
    return parts.join(QLatin1Char(' '));
}

bool PythonWorkerProcess::start(QString* error)
{
    if(isRunning())
        stop();

    _buffer.clear();
    _standardError.clear();
    _outstandingId = 0;
    _processId = -1;
    _lastExitCode = -1;

    if(_interpreter.isEmpty()) {
        if(error)
            *error = QStringLiteral("No Python interpreter was given. Select an environment before running Python.");
        return false;
    }
    const QString script = _workerScript.isEmpty() ? defaultWorkerScript() : _workerScript;
    if(script.isEmpty() || !QFileInfo::exists(script)) {
        if(error)
            *error = QStringLiteral("The Python worker script \"%1\" does not exist.").arg(script);
        return false;
    }
    _workerScript = script;

    _process = std::make_unique<QProcess>();
    _process->setProcessEnvironment(_environment);
    _process->setProcessChannelMode(QProcess::SeparateChannels);

    QStringList arguments;
    if(_isolated)
        arguments << QStringLiteral("-I");
    arguments << _extraArguments << script;
    _process->start(_interpreter, arguments);
    if(!_process->waitForStarted(10000)) {
        if(error)
            *error = QStringLiteral("The Python interpreter \"%1\" could not be started (%2).").arg(_interpreter, _process->errorString());
        _process = nullptr;
        return false;
    }
    _processId = _process->processId();
    return true;
}

bool PythonWorkerProcess::isRunning() const
{
    return _process && _process->state() == QProcess::Running;
}

void PythonWorkerProcess::kill()
{
    if(_process && _process->state() != QProcess::NotRunning)
        _process->kill();
    _outstandingId = 0;
}

void PythonWorkerProcess::stop(int timeoutMs)
{
    if(!_process)
        return;
    if(_process->state() != QProcess::NotRunning) {
        // Ask politely first: the worker unlinks its shared-memory segments and flushes its answer on `quit`.
        PythonWorkerRequest request;
        request.id = nextRequestId();
        request.operation = QLatin1String(PythonWorkerProtocol::quitOperation);
        if(send(request)) {
            PythonWorkerReply reply;
            receive(reply, timeoutMs);
        }
        if(_process->state() != QProcess::NotRunning) {
            _process->kill();
            _process->waitForFinished(timeoutMs);
        }
    }
    collectStandardError();
    // The exit status outlives the process object: a caller asking afterwards whether the worker stopped cleanly or had
    // to be killed is asking a question that must not answer -1 for both.
    _lastExitCode = _process->exitCode();
    _process.reset();
    _processId = -1;
    _outstandingId = 0;
}

bool PythonWorkerProcess::waitForFinished(int timeoutMs)
{
    if(!_process)
        return false;
    const bool finished = _process->waitForFinished(timeoutMs);
    collectStandardError();
    return finished;
}

int PythonWorkerProcess::exitCode() const
{
    return _process ? _process->exitCode() : static_cast<int>(_lastExitCode);
}

QString PythonWorkerProcess::standardError()
{
    collectStandardError();
    return _standardError;
}

void PythonWorkerProcess::collectStandardError()
{
    if(!_process)
        return;
    const QString text = QString::fromUtf8(_process->readAllStandardError());
    if(text.isEmpty())
        return;
    _standardError += text;
    if(_standardError.size() > maximumStandardErrorLength)
        _standardError = _standardError.right(maximumStandardErrorLength);
}

/******************************************************************************
* Reading and writing.
******************************************************************************/
QString PythonWorkerProcess::failureReason() const
{
    if(!_process)
        return QStringLiteral("no worker process is running");
    if(_process->state() == QProcess::NotRunning)
        return QStringLiteral("the worker exited with code %1").arg(_process->exitCode());
    return QStringLiteral("the worker did not answer in time");
}

bool PythonWorkerProcess::readLine(QByteArray& line, int timeoutMs, QString* failure)
{
    QElapsedTimer timer;
    timer.start();
    for(;;) {
        const qsizetype index = _buffer.indexOf('\n');
        if(index >= 0) {
            line = _buffer.left(index);
            _buffer.remove(0, index + 1);
            return true;
        }
        if(!_process) {
            if(failure)
                *failure = failureReason();
            return false;
        }
        const int remaining = timeoutMs - static_cast<int>(timer.elapsed());
        // A read that returns false must not be trusted blindly: the bytes of the line may already have arrived with a
        // previous read, and a worker that exited right after answering has answered.
        if(remaining <= 0 || !_process->waitForReadyRead(remaining)) {
            _buffer += _process->readAllStandardOutput();
            const qsizetype lateIndex = _buffer.indexOf('\n');
            if(lateIndex >= 0) {
                line = _buffer.left(lateIndex);
                _buffer.remove(0, lateIndex + 1);
                return true;
            }
            if(failure)
                *failure = failureReason();
            return false;
        }
        _buffer += _process->readAllStandardOutput();
    }
}

bool PythonWorkerProcess::readExact(qint64 count, QByteArray& out, int timeoutMs, QString* failure)
{
    QElapsedTimer timer;
    timer.start();
    while(_buffer.size() < count) {
        if(!_process) {
            if(failure)
                *failure = failureReason();
            return false;
        }
        const int remaining = timeoutMs - static_cast<int>(timer.elapsed());
        if(remaining <= 0 || !_process->waitForReadyRead(remaining)) {
            _buffer += _process->readAllStandardOutput();
            if(_buffer.size() < count) {
                if(failure)
                    *failure = QStringLiteral("%1; %2 of %3 payload bytes arrived")
                                   .arg(failureReason())
                                   .arg(_buffer.size())
                                   .arg(count);
                return false;
            }
            break;
        }
        _buffer += _process->readAllStandardOutput();
    }
    out = _buffer.left(count);
    _buffer.remove(0, count);
    return true;
}

quint64 PythonWorkerProcess::send(const PythonWorkerRequest& request)
{
    _sendError.clear();
    if(!isRunning()) {
        _sendError = QStringLiteral("the Python worker is not running");
        return 0;
    }
    if(_outstandingId != 0) {
        _sendError = QStringLiteral("the worker already has an unanswered request (%1); it runs one operation at a time")
                         .arg(_outstandingId);
        return 0;
    }
    PythonWorkerRequest outgoing = request;
    if(outgoing.id == 0)
        outgoing.id = nextRequestId();
    const QByteArray header = QJsonDocument(QJsonObject::fromVariantMap(outgoing.toJson())).toJson(QJsonDocument::Compact) + '\n';
    // The header and its payload are written as one buffer: a worker that reads the header must find the announced bytes
    // behind it without a race of ours making it wait.
    QByteArray message = header;
    message += outgoing.payload;
    if(_process->write(message) != message.size() || !_process->waitForBytesWritten(5000)) {
        _sendError = QStringLiteral("the request could not be written to the worker (%1)").arg(_process->errorString());
        return 0;
    }
    _outstandingId = outgoing.id;
    return outgoing.id;
}

bool PythonWorkerProcess::receive(PythonWorkerReply& reply, int timeoutMs)
{
    // A reply that cancel() read while it was waiting for its acknowledgement is the answer the caller is waiting for.
    if(_deferredReply.has_value()) {
        reply = *std::move(_deferredReply);
        _deferredReply.reset();
        return reply.transportOk;
    }

    reply = PythonWorkerReply{};
    QElapsedTimer timer;
    timer.start();

    QByteArray line;
    QString failure;
    if(!readLine(line, timeoutMs, &failure)) {
        _outstandingId = 0;
        reply.transportOk = false;
        reply.errorCode = QStringLiteral("worker_unavailable");
        reply.errorMessage = failure;
        reply.elapsedMs = timer.nsecsElapsed() / 1e6;
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(line);
    if(!document.isObject()) {
        // The channel is unusable from here on: whatever the worker wrote cannot be attributed to a request, so the
        // request stays unanswerable and the caller must restart the worker rather than read the next line as an answer.
        _outstandingId = 0;
        reply.transportOk = false;
        reply.errorCode = QStringLiteral("protocol_error");
        reply.errorMessage = QStringLiteral("the worker answered with something other than a JSON object: %1")
                                 .arg(QString::fromUtf8(line.left(200)));
        reply.elapsedMs = timer.nsecsElapsed() / 1e6;
        return false;
    }

    reply.json = document.object().toVariantMap();
    reply.id = reply.json.value(QStringLiteral("id")).toULongLong();
    reply.ok = reply.json.value(QStringLiteral("ok")).toBool();
    reply.elapsedMs = timer.nsecsElapsed() / 1e6;
    if(reply.id != 0 && _outstandingId != 0 && reply.id != _outstandingId) {
        reply.transportOk = false;
        reply.ok = false;
        reply.errorCode = QStringLiteral("protocol_error");
        reply.errorMessage = QStringLiteral("the worker answered request %1 while %2 was outstanding").arg(reply.id).arg(_outstandingId);
        return false;
    }
    _outstandingId = 0;

    const QVariantMap error = reply.json.value(QStringLiteral("error")).toMap();
    if(!error.isEmpty()) {
        reply.errorCode = error.value(QStringLiteral("code")).toString();
        reply.errorMessage = error.value(QStringLiteral("message")).toString();
        reply.errorDetails = error.value(QStringLiteral("details")).toMap();
    }
    else if(!reply.ok) {
        reply.errorCode = QStringLiteral("protocol_error");
        reply.errorMessage = QStringLiteral("the worker reported a failure without an error object");
    }

    // The framed transfer: exactly the announced number of bytes follow the header line, and a reply is not handed out
    // before all of them are here - a half-read payload would desynchronize the next request.
    const qint64 bytes = reply.json.value(QStringLiteral("bytes")).toLongLong();
    if(bytes > 0) {
        if(!readExact(bytes, reply.payload, timeoutMs, &failure)) {
            reply.transportOk = false;
            reply.ok = false;
            reply.errorCode = QStringLiteral("worker_unavailable");
            reply.errorMessage = failure;
            return false;
        }
    }
    return true;
}

PythonWorkerReply PythonWorkerProcess::exchange(const PythonWorkerRequest& request, int timeoutMs)
{
    PythonWorkerReply reply;
    QElapsedTimer timer;
    timer.start();
    const quint64 id = send(request);
    if(id == 0) {
        reply.transportOk = false;
        reply.errorCode = QStringLiteral("worker_unavailable");
        reply.errorMessage = _sendError;
        reply.elapsedMs = timer.nsecsElapsed() / 1e6;
        return reply;
    }
    receive(reply, timeoutMs);
    reply.elapsedMs = timer.nsecsElapsed() / 1e6;
    return reply;
}

bool PythonWorkerProcess::cancel(quint64 targetId, int timeoutMs)
{
    if(!isRunning() || targetId == 0)
        return false;
    // The cancellation message is not an operation of its own: the worker handles it in its reader thread and answers
    // immediately, which is what lets it overtake the operation it stops. It therefore does not take the outstanding
    // slot of this client - but it travels on the same channel, so the answer that comes back may be the one of the
    // operation being cancelled. Anything that is not the acknowledgement is kept for the next receive(), which is
    // exactly where the caller expects the answer of the cancelled request.
    PythonWorkerRequest request;
    request.id = nextRequestId();
    request.operation = QLatin1String(PythonWorkerProtocol::cancelOperation);
    request.arguments.insert(QStringLiteral("target"), QVariant::fromValue(targetId));

    const quint64 savedOutstanding = _outstandingId;
    _outstandingId = 0;
    const quint64 id = send(request);
    if(id == 0) {
        _outstandingId = savedOutstanding;
        return false;
    }

    QElapsedTimer timer;
    timer.start();
    bool acknowledged = false;
    for(;;) {
        PythonWorkerReply reply;
        const int remaining = timeoutMs - static_cast<int>(timer.elapsed());
        if(remaining <= 0 || !receive(reply, remaining))
            break;
        if(reply.id == id) {
            acknowledged = reply.ok;
            break;
        }
        // The answer of the operation that is being cancelled arrived first. It belongs to the caller, not here.
        if(!_deferredReply.has_value())
            _deferredReply = reply;
    }
    _outstandingId = savedOutstanding;
    return acknowledged;
}

/******************************************************************************
* The handshake.
******************************************************************************/
PythonWorkerHandshake PythonWorkerProcess::handshake(int timeoutMs, QString* error)
{
    PythonWorkerHandshake handshake;
    PythonWorkerRequest request;
    request.operation = QLatin1String(PythonWorkerProtocol::handshakeOperation);

    const PythonWorkerReply reply = exchange(request, timeoutMs);
    if(!reply.transportOk) {
        if(error)
            *error = reply.errorMessage.isEmpty() ? failureReason() : reply.errorMessage;
        return handshake;
    }
    if(!reply.ok) {
        if(error)
            *error = QStringLiteral("the worker refused the handshake: %1").arg(reply.errorMessage);
        return handshake;
    }

    handshake.raw = reply.json;
    handshake.ok = true;
    handshake.protocolName = reply.json.value(QStringLiteral("handshake")).toString();
    handshake.protocolVersionText = reply.json.value(QStringLiteral("protocolVersion")).toString();
    const QVersionNumber version = QVersionNumber::fromString(handshake.protocolVersionText);
    handshake.protocolVersionMajor = version.majorVersion();
    handshake.protocolVersionMinor = version.minorVersion();
    handshake.implementation = reply.json.value(QStringLiteral("implementation")).toString();
    handshake.versionText = reply.json.value(QStringLiteral("version")).toString();
    handshake.executable = reply.json.value(QStringLiteral("executable")).toString();
    handshake.processId = reply.json.value(QStringLiteral("pid")).toLongLong();
    handshake.numpyVersion = reply.json.value(QStringLiteral("numpy")).toString();
    for(const QVariant& feature : reply.json.value(QStringLiteral("features")).toList()) {
        // A feature this build does not know is kept, not dropped: that is how a newer worker stays usable and how a
        // caller can say what it did not understand (the rule PythonHandshake follows as well).
        const QString name = feature.toString();
        if(name.isEmpty())
            continue;
        handshake.features << name;
        if(!PythonContract::featureFromName(name).has_value())
            handshake.unknownFeatures << name;
    }
    for(const QVariant& mode : reply.json.value(QStringLiteral("transferModes")).toList())
        handshake.transferModes << mode.toString();

    // The two gates that make this handshake worth having. A worker that is not the runtime half of the package this
    // build expects, or one whose protocol major version differs, cannot be used - and a silent mismatch is how a
    // Python track becomes unexplainable instead of merely broken. A newer *minor* version is fine: the protocol grows
    // additively, as the package contract does.
    const QVersionNumber expected = QVersionNumber::fromString(PythonContract::protocolVersion());
    if(handshake.protocolName != PythonContract::handshakeName()) {
        if(error)
            *error = QStringLiteral("the interpreter answered the handshake of \"%1\", not \"%2\"")
                         .arg(handshake.protocolName, PythonContract::handshakeName());
        handshake.ok = false;
        return handshake;
    }
    if(handshake.protocolVersionMajor != expected.majorVersion()) {
        if(error)
            *error = QStringLiteral("the worker speaks protocol version %1; this build implements version %2")
                         .arg(handshake.protocolVersionText, PythonContract::protocolVersion());
        handshake.ok = false;
        return handshake;
    }
    if(error)
        error->clear();
    return handshake;
}

}   // namespace Ovito
