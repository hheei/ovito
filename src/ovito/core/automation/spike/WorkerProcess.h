// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <memory>

QT_BEGIN_NAMESPACE
class QProcess;
QT_END_NAMESPACE

namespace Ovito {

/**
 * \brief One answer of the spike worker.
 *
 * `ok` is the protocol verdict, `json` the whole response object (which carries the worker's own timings and the
 * checksums of what it sent), and `payload` the raw bytes of a framed transfer. `elapsedMs` is what the caller
 * experienced, which is the number the topology decision is about: the worker can report how long it computed, but only
 * the client can say how long the round trip took.
 */
struct WorkerReply
{
    bool ok = false;
    QJsonObject json;
    QByteArray payload;
    QString errorCode;
    QString errorMessage;
    double elapsedMs = 0.0;
};

/**
 * \brief The client side of the spike protocol: a persistent Python worker and the two pipes it speaks over.
 *
 * This is deliberately *not* production code. It is the smallest thing that can answer the question the design leaves
 * open - what does the seam between OVITO and a user-selected interpreter cost - and it exists so that the answer is a
 * measurement rather than an assumption. It is written blocking and single-threaded on purpose: the spike measures one
 * operation at a time, and a blocking client makes the measured wall time unambiguous.
 *
 * What it implements of the design's open questions:
 *
 *  - one JSON-lines control channel, one request per line, responses correlated by `id`;
 *  - a binary framing for arrays: a response may announce `bytes`, and exactly that many raw bytes follow its header
 *    line, so no array is ever JSON or base64;
 *  - a shared-memory read path for the same arrays (POSIX `shm_open`/`mmap`; reported as unsupported elsewhere), which
 *    is what the comparison against the framed path is for;
 *  - death detection: a read that cannot complete because the interpreter exited reports the exit code instead of
 *    waiting for the timeout, which is what makes crash recovery measurable.
 */
class WorkerProcess
{
public:

    WorkerProcess();
    ~WorkerProcess();

    WorkerProcess(const WorkerProcess&) = delete;
    WorkerProcess& operator=(const WorkerProcess&) = delete;

    /// Starts the interpreter with the worker script. Returns false and a reason when it cannot be started.
    bool start(const QString& interpreter, const QString& workerScript, QString* error = nullptr);

    bool isRunning() const;

    /// The interpreter's process id, or -1.
    qint64 processId() const;

    /// A worker that is killed or has exited is not restarted implicitly: the caller decides what a dead worker means.
    void kill();
    bool waitForFinished(int timeoutMs);
    int exitCode() const;

    /// Sends a request and waits for its answer. `timeoutMs` bounds each read, not the whole operation.
    WorkerReply request(const QJsonObject& request, int timeoutMs = 120000);

    /**
     * Sends a request without waiting for its answer.
     *
     * This exists for the cancellation measurement: a long operation and the `cancel` message that stops it are sent
     * over the same channel, which is only possible if sending does not block on the answer.
     */
    bool send(const QJsonObject& request);

    /// The next request id; the caller uses it to correlate a request it sent with the answer it reads later.
    int allocateRequestId() { return ++_nextRequestId; }

    /// Reads the answer of a request that was sent earlier. Returns false when the worker died instead of answering.
    bool receive(WorkerReply& reply, int timeoutMs = 120000);

    /// Maps a shared-memory segment the worker created and returns its contents. POSIX only.
    static QByteArray readSharedMemory(const QString& name, qint64 bytes, QString* error = nullptr);

    /// Whether this platform has a shared-memory read path at all.
    static bool isSharedMemorySupported();

private:

    /// Reads one '\n'-terminated line into `line`, or fails with a reason.
    bool readLine(QByteArray& line, int timeoutMs, QString* failure = nullptr);

    /// Reads exactly `count` bytes into `out`.
    bool readExact(qint64 count, QByteArray& out, int timeoutMs, QString* failure = nullptr);

    /// Distinguishes "the worker exited" from "the read timed out", which are different measurements.
    QString failureReason(const QByteArray& data) const;

    std::unique_ptr<QProcess> _process;
    QByteArray _buffer;
    int _nextRequestId = 0;
};

}   // namespace Ovito
