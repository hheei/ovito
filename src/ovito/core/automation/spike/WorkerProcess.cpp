// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include "WorkerProcess.h"

#include <QElapsedTimer>
#include <QJsonDocument>
#include <QProcess>

#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace Ovito {

/******************************************************************************
* Life cycle.
******************************************************************************/
WorkerProcess::WorkerProcess() = default;

WorkerProcess::~WorkerProcess()
{
    if(_process && _process->state() != QProcess::NotRunning) {
        _process->kill();
        _process->waitForFinished(2000);
    }
}

bool WorkerProcess::start(const QString& interpreter, const QString& workerScript, QString* error)
{
    _buffer.clear();
    _process = std::make_unique<QProcess>();
    // The worker must run in the environment the caller selected, with no extra interpreter of ours involved.
    _process->setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    _process->setProcessChannelMode(QProcess::SeparateChannels);
    _process->start(interpreter, { QStringLiteral("-I"), workerScript });
    if(!_process->waitForStarted(10000)) {
        if(error)
            *error = QStringLiteral("could not start \"%1\": %2").arg(interpreter, _process->errorString());
        _process = nullptr;
        return false;
    }
    return true;
}

bool WorkerProcess::isRunning() const
{
    return _process && _process->state() == QProcess::Running;
}

qint64 WorkerProcess::processId() const
{
    return _process ? _process->processId() : -1;
}

void WorkerProcess::kill()
{
    if(_process && _process->state() != QProcess::NotRunning)
        _process->kill();
}

bool WorkerProcess::waitForFinished(int timeoutMs)
{
    if(!_process)
        return false;
    return _process->waitForFinished(timeoutMs);
}

int WorkerProcess::exitCode() const
{
    return _process ? _process->exitCode() : -1;
}

/******************************************************************************
* Reading one answer.
******************************************************************************/
QString WorkerProcess::failureReason(const QByteArray& data) const
{
    if(!_process)
        return QStringLiteral("no worker was started");
    // An exit is the important distinction: waiting for a timeout after the interpreter died would hide the very
    // problem the crash-recovery measurement is about.
    if(_process->state() == QProcess::NotRunning && data.isEmpty())
        return QStringLiteral("the worker exited with code %1").arg(_process->exitCode());
    return QStringLiteral("no answer within the timeout");
}

bool WorkerProcess::readLine(QByteArray& line, int timeoutMs, QString* failure)
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
        if(!_process)
            break;
        const int remaining = timeoutMs - static_cast<int>(timer.elapsed());
        if(remaining <= 0 || !_process->waitForReadyRead(remaining)) {
            _buffer += _process->readAllStandardOutput();
            const qsizetype lateIndex = _buffer.indexOf('\n');
            if(lateIndex >= 0) {
                line = _buffer.left(lateIndex);
                _buffer.remove(0, lateIndex + 1);
                return true;
            }
            if(failure)
                *failure = failureReason(_buffer);
            return false;
        }
        _buffer += _process->readAllStandardOutput();
    }
    if(failure)
        *failure = failureReason(_buffer);
    return false;
}

bool WorkerProcess::readExact(qint64 count, QByteArray& out, int timeoutMs, QString* failure)
{
    QElapsedTimer timer;
    timer.start();
    while(_buffer.size() < count) {
        if(!_process) {
            if(failure)
                *failure = failureReason(_buffer);
            return false;
        }
        const int remaining = timeoutMs - static_cast<int>(timer.elapsed());
        if(remaining <= 0 || !_process->waitForReadyRead(remaining)) {
            _buffer += _process->readAllStandardOutput();
            if(_buffer.size() < count) {
                if(failure)
                    *failure = failureReason(_buffer);
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

bool WorkerProcess::send(const QJsonObject& request)
{
    if(!isRunning())
        return false;
    const QByteArray line = QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n';
    return _process->write(line) == line.size() && _process->waitForBytesWritten(5000);
}

bool WorkerProcess::receive(WorkerReply& reply, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();

    QByteArray line;
    QString failure;
    if(!readLine(line, timeoutMs, &failure)) {
        reply.ok = false;
        reply.errorCode = QStringLiteral("worker_unavailable");
        reply.errorMessage = failure;
        reply.elapsedMs = timer.nsecsElapsed() / 1e6;
        return false;
    }
    reply.elapsedMs = timer.nsecsElapsed() / 1e6;
    const QJsonDocument document = QJsonDocument::fromJson(line);
    if(!document.isObject()) {
        reply.ok = false;
        reply.errorCode = QStringLiteral("protocol_error");
        reply.errorMessage = QStringLiteral("the worker answered with something other than a JSON object: %1")
                                 .arg(QString::fromUtf8(line.left(200)));
        return false;
    }
    reply.json = document.object();
    reply.ok = reply.json.value(QStringLiteral("ok")).toBool();

    const QJsonObject error = reply.json.value(QStringLiteral("error")).toObject();
    if(!reply.ok && !error.isEmpty()) {
        reply.errorCode = error.value(QStringLiteral("code")).toString();
        reply.errorMessage = error.value(QStringLiteral("message")).toString();
    }

    // A response may announce raw bytes that follow its header line: the framed array transfer.
    const qint64 bytes = reply.json.value(QStringLiteral("bytes")).toInteger();
    if(bytes > 0) {
        if(!readExact(bytes, reply.payload, timeoutMs, &failure)) {
            reply.ok = false;
            reply.errorCode = QStringLiteral("worker_unavailable");
            reply.errorMessage = failure;
            return false;
        }
    }
    return true;
}

WorkerReply WorkerProcess::request(const QJsonObject& request, int timeoutMs)
{
    WorkerReply reply;
    if(!send(request)) {
        reply.errorCode = QStringLiteral("worker_unavailable");
        reply.errorMessage = QStringLiteral("the request could not be written to the worker");
        return reply;
    }
    receive(reply, timeoutMs);
    return reply;
}

/******************************************************************************
* The shared-memory read path.
******************************************************************************/
bool WorkerProcess::isSharedMemorySupported()
{
#ifdef Q_OS_UNIX
    return true;
#else
    return false;
#endif
}

QByteArray WorkerProcess::readSharedMemory(const QString& name, qint64 bytes, QString* error)
{
#ifdef Q_OS_UNIX
    // The worker creates the segment with the name it reports; POSIX names are absolute and short, which is why the
    // worker builds them from its process id and a counter.
    if(!name.startsWith(u'/'))
        return readSharedMemory(u'/' + name, bytes, error);
    const QByteArray nativeName = name.toLocal8Bit();
    const int fd = ::shm_open(nativeName.constData(), O_RDONLY, 0);
    if(fd < 0) {
        if(error)
            *error = QStringLiteral("cannot open the shared memory segment \"%1\"").arg(name);
        return {};
    }
    struct stat status {};
    if(::fstat(fd, &status) != 0 || status.st_size < bytes) {
        ::close(fd);
        if(error)
            *error = QStringLiteral("the shared memory segment \"%1\" is smaller than the announced %2 bytes").arg(name).arg(bytes);
        return {};
    }
    void* mapping = ::mmap(nullptr, bytes, PROT_READ, MAP_SHARED, fd, 0);
    ::close(fd);
    if(mapping == MAP_FAILED) {
        if(error)
            *error = QStringLiteral("cannot map the shared memory segment \"%1\"").arg(name);
        return {};
    }
    const QByteArray contents(static_cast<const char*>(mapping), bytes);
    ::munmap(mapping, bytes);
    return contents;
#else
    if(error)
        *error = QStringLiteral("shared memory transfer is not implemented for this platform in the spike");
    Q_UNUSED(name);
    Q_UNUSED(bytes);
    return {};
#endif
}

}   // namespace Ovito
