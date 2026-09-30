// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

/**
 * \file
 * \brief The interpreter and execution-topology spike of Phase 2.6 (deliverable 6).
 *
 * The design leaves one question open: should OVITO run user Python in an embedded runtime, or in a persistent worker
 * process that the user's own interpreter serves? The answer has to be measured rather than assumed, because it decides
 * the runtime weight, the crash isolation and the data bridge of the whole Python track.
 *
 * This program measures the *seam*: what one evaluation costs when the interpreter is a separate process, broken down
 * into the parts that a decision can act on - process startup, control round trips, and the transfer of a representative
 * particle array plus a mesh index array in every transport the design discusses. It compares those numbers with two
 * baselines: `none`, the arithmetic alone, and `in-process`, the same arrays computed and hashed without ever leaving the
 * interpreter. The second one is the cost model of an embedded runtime for the data path; what it cannot measure is the
 * GIL, ABI and crash-coupling cost of embedding, which is exactly why the numbers are reported with that caveat instead
 * of being turned into a claim about embedded runtimes.
 *
 * Every transfer is verified, not trusted: all array-bearing modes must report the same checksum for the same input, the
 * framed bytes and the shared-memory bytes must be identical, the client recomputes the hashes of what it received, and
 * the first values of the decoded JSON and base64 forms must equal the first values of the raw form. A measurement tool
 * that silently measures the wrong data would be worse than no measurement.
 *
 * Usage:
 * \code
 * ovito-automation-spike [--python PATH] [--worker PATH] [--json FILE] [--quick] [--verbose]
 *                        [--particles N] [--faces N] [--iterations N] [--pings N] [--startups N] [--frames N]
 *                        [--modes none,in-process,text,base64,framed,shared-memory]
 * \endcode
 *
 * Exit codes: 0 when every check passed, 1 when a check failed (the measurements themselves always print), 2 when there
 * was no interpreter to measure - so that a caller can tell "the spike failed" from "the spike was skipped".
 */

#include "WorkerProcess.h"

#include <ovito/core/automation/python/PythonContract.h>
#include <ovito/core/automation/python/PythonEnvironmentProbe.h>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QStringList>
#include <QThread>

#include <algorithm>
#include <cmath>
#include <limits>

using namespace Ovito;

namespace {

/// The particle positions the worker builds, spelled exactly as `ovito_worker.py` does, so a few values can be checked
/// without trusting the worker's own report. Only single-operation expressions are checked, because a compiler is
/// allowed to contract `a * b + c` differently from Python.
constexpr double PositionScaleX = 0.1234567890123456;
constexpr double PositionSeedScale = 0.987654321;

/// The number of values of a transfer that are compared across modes; enough to catch a broken encoding, cheap enough to
/// keep the round trip honest.
constexpr int SpotCheckValues = 8;

struct Options
{
    QString interpreter;
    QString worker;
    QString jsonPath;
    int startups = 5;
    int pings = 50;
    int iterations = 5;
    int particles = 500000;
    int faces = 200000;
    int frames = 10;
    int timeoutMs = 300000;
    QStringList modes = { QStringLiteral("none"),   QStringLiteral("in-process"), QStringLiteral("framed"),
                          QStringLiteral("shared-memory"), QStringLiteral("base64"), QStringLiteral("text") };
    bool verbose = false;
    bool quick = false;
};

struct Samples
{
    QString label;
    QVector<double> values;
    QString note;

    void add(double value) { values.push_back(value); }
    bool isEmpty() const { return values.isEmpty(); }
    double minimum() const { return values.isEmpty() ? 0.0 : *std::min_element(values.cbegin(), values.cend()); }
    double maximum() const { return values.isEmpty() ? 0.0 : *std::max_element(values.cbegin(), values.cend()); }
    double median() const
    {
        if(values.isEmpty())
            return 0.0;
        QVector<double> sorted = values;
        std::sort(sorted.begin(), sorted.end());
        return sorted.at(sorted.size() / 2);
    }
    double percentile(double fraction) const
    {
        if(values.isEmpty())
            return 0.0;
        QVector<double> sorted = values;
        std::sort(sorted.begin(), sorted.end());
        const int index = std::min<int>(sorted.size() - 1, static_cast<int>(std::ceil(fraction * sorted.size())) - 1);
        return sorted.at(std::max(0, index));
    }
};

/// Collects the pass/fail checks. The spike is a measurement tool, so a check failure is reported and the exit code is
/// non-zero, but the measurements of the remaining phases are still taken.
class CheckLog
{
public:

    bool check(bool condition, const QString& description)
    {
        ++_total;
        if(condition) {
            ++_passed;
            if(_verbose)
                qInfo().noquote() << "  ok  " << description;
        }
        else {
            _failures.push_back(description);
            qWarning().noquote() << "  FAIL" << description;
        }
        return condition;
    }

    void setVerbose(bool verbose) { _verbose = verbose; }
    int passed() const { return _passed; }
    int total() const { return _total; }
    const QStringList& failures() const { return _failures; }

private:

    bool _verbose = false;
    int _total = 0;
    int _passed = 0;
    QStringList _failures;
};

QString megabytes(qint64 bytes)
{
    return QStringLiteral("%1 MB").arg(QString::number(static_cast<double>(bytes) / (1024.0 * 1024.0), 'f', 1));
}

QString milliseconds(double ms)
{
    return QString::number(ms, 'f', ms < 10.0 ? 2 : 1);
}

void printSamples(const Samples& samples, const QString& unit = QStringLiteral("ms"))
{
    if(samples.isEmpty()) {
        qInfo().noquote() << QStringLiteral("  %1: no data").arg(samples.label);
        return;
    }
    qInfo().noquote() << QStringLiteral("  %1: min %2, median %3, max %4 %5 (%6 samples)%7")
                             .arg(samples.label, milliseconds(samples.minimum()), milliseconds(samples.median()),
                                  milliseconds(samples.maximum()), unit)
                             .arg(samples.values.size())
                             .arg(samples.note.isEmpty() ? QString() : QStringLiteral(" - ") + samples.note);
}

/// The first `count` doubles of a raw little-endian double buffer.
QVector<double> leadingDoubles(const QByteArray& buffer, int count)
{
    QVector<double> values;
    const int available = std::min<int>(count, static_cast<int>(buffer.size() / static_cast<int>(sizeof(double))));
    values.reserve(available);
    for(int i = 0; i < available; ++i)
        values.push_back(reinterpret_cast<const double*>(buffer.constData())[i]);
    return values;
}

QString sha256(const QByteArray& buffer)
{
    return QString::fromLatin1(QCryptographicHash::hash(buffer, QCryptographicHash::Sha256).toHex());
}

/// Parses the command line. Returns false for an unknown option.
bool parseOptions(const QStringList& arguments, Options& options)
{
    for(int i = 0; i < arguments.size(); ++i) {
        const QString argument = arguments.at(i);
        const auto next = [&](int& index) -> QString { return index + 1 < arguments.size() ? arguments.at(++index) : QString(); };
        if(argument == QStringLiteral("--python"))
            options.interpreter = next(i);
        else if(argument == QStringLiteral("--worker"))
            options.worker = next(i);
        else if(argument == QStringLiteral("--json"))
            options.jsonPath = next(i);
        else if(argument == QStringLiteral("--particles"))
            options.particles = next(i).toInt();
        else if(argument == QStringLiteral("--faces"))
            options.faces = next(i).toInt();
        else if(argument == QStringLiteral("--iterations"))
            options.iterations = next(i).toInt();
        else if(argument == QStringLiteral("--pings"))
            options.pings = next(i).toInt();
        else if(argument == QStringLiteral("--startups"))
            options.startups = next(i).toInt();
        else if(argument == QStringLiteral("--frames"))
            options.frames = next(i).toInt();
        else if(argument == QStringLiteral("--modes"))
            options.modes = next(i).split(u',', Qt::SkipEmptyParts);
        else if(argument == QStringLiteral("--quick")) {
            options.quick = true;
            options.particles = 20000;
            options.faces = 8000;
            options.iterations = 2;
            options.pings = 10;
            options.startups = 2;
            options.frames = 4;
        }
        else if(argument == QStringLiteral("--verbose"))
            options.verbose = true;
        else if(argument.startsWith(QStringLiteral("--"))) {
            qWarning().noquote() << "Unknown option:" << argument;
            return false;
        }
    }
    return true;
}

/// Where the worker script of this build lives, baked in at build time like the probe script.
QString defaultWorkerScript()
{
#ifdef OVITO_AUTOMATION_PYTHON_DIR
    const QFileInfo script(QStringLiteral(OVITO_AUTOMATION_PYTHON_DIR "/ovito_worker.py"));
    if(script.exists())
        return script.absoluteFilePath();
#endif
    return {};
}

}   // namespace

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ovito-automation-spike"));

    Options options;
    if(!parseOptions(QCoreApplication::arguments().mid(1), options)) {
        qWarning().noquote() << "Usage: ovito-automation-spike [--python PATH] [--worker PATH] [--json FILE] [--quick] "
                                "[--verbose] [--particles N] [--faces N] [--iterations N] [--pings N] [--startups N] "
                                "[--frames N] [--modes a,b,c]";
        return 2;
    }
    if(options.interpreter.isEmpty())
        options.interpreter = PythonEnvironmentProbe::findInterpreter();
    if(options.worker.isEmpty())
        options.worker = defaultWorkerScript();

    CheckLog checks;
    checks.setVerbose(options.verbose);

    qInfo().noquote() << "== Phase 2.6 Python execution topology spike ==";
    if(options.interpreter.isEmpty() || options.worker.isEmpty()) {
        qWarning().noquote() << "No Python interpreter to measure"
                             << (options.interpreter.isEmpty() ? QStringLiteral("(none found on the path; pass --python)")
                                                               : QStringLiteral("(the worker script is missing)"));
        qInfo().noquote() << "interpreter:" << options.interpreter;
        qInfo().noquote() << "worker     :" << options.worker;
        return 2;
    }

    qInfo().noquote() << "interpreter :" << options.interpreter;
    qInfo().noquote() << "worker      :" << options.worker;

    // The environment probe of deliverable 5 runs first: the spike measures the same interpreter, and reporting the
    // probe's verdict here documents which environment the numbers belong to. A missing package is not fatal, because
    // the spike runs its own worker script rather than the package's entry point.
    PythonProbeRequest probeRequest = PythonProbeRequest::forExecutable(options.interpreter);
    probeRequest.setRequiredFeatures({});
    const PythonProbeResult probe = PythonEnvironmentProbe::probeBlocking(probeRequest);
    qInfo().noquote() << "probe       :" << probe.statusName()
                      << (probe.handshake() ? QStringLiteral("(%1)").arg(probe.handshake()->environment().displayName())
                                            : QStringLiteral("(%1)").arg(probe.message()));
    checks.check(probe.handshake().has_value() || probe.status() == PythonContract::ProbeStatus::PackageMissing,
                 QStringLiteral("the environment probe describes the interpreter"));

    QJsonObject report;
    report.insert(QStringLiteral("interpreter"), options.interpreter);
    report.insert(QStringLiteral("worker"), options.worker);
    report.insert(QStringLiteral("probeStatus"), probe.statusName());
    QJsonObject environment;
    if(probe.handshake()) {
        environment = QJsonObject::fromVariantMap(probe.handshake()->environment().toJson());
        environment.insert(QStringLiteral("features"), QJsonArray::fromStringList(probe.handshake()->featureNames()));
    }
    report.insert(QStringLiteral("environment"), environment);
    QJsonObject payload;
    payload.insert(QStringLiteral("particles"), options.particles);
    payload.insert(QStringLiteral("faces"), options.faces);
    payload.insert(QStringLiteral("transferBytes"),
                   static_cast<double>(options.particles) * 3 * sizeof(double) + static_cast<double>(options.faces) * 3 * sizeof(int));
    report.insert(QStringLiteral("payload"), payload);

    // -----------------------------------------------------------------------------------------------------------------
    // 1. Startup: spawn the interpreter and complete the handshake. This is what a worker avoids paying per evaluation.
    // -----------------------------------------------------------------------------------------------------------------
    Samples startup;
    startup.label = QStringLiteral("startup (spawn + handshake)");
    QJsonObject handshake;
    for(int i = 0; i < options.startups; ++i) {
        WorkerProcess worker;
        QElapsedTimer timer;
        timer.start();
        QString error;
        if(!worker.start(options.interpreter, options.worker, &error)) {
            checks.check(false, QStringLiteral("the interpreter starts: %1").arg(error));
            break;
        }
        const int id = worker.allocateRequestId();
        const WorkerReply reply = worker.request(QJsonObject({ { QStringLiteral("id"), id }, { QStringLiteral("op"), QStringLiteral("handshake") } }));
        startup.add(timer.nsecsElapsed() / 1e6);
        if(!reply.ok) {
            checks.check(false, QStringLiteral("the worker answers the handshake: %1").arg(reply.errorMessage));
            break;
        }
        if(i == 0) {
            handshake = reply.json;
            const QString reported = reply.json.value(QStringLiteral("executable")).toString();
            checks.check(QFileInfo(reported).canonicalFilePath() == QFileInfo(options.interpreter).canonicalFilePath(),
                         QStringLiteral("the worker reports the interpreter that was started (%1)").arg(reported));
            checks.check(!reply.json.value(QStringLiteral("features")).toArray().isEmpty(),
                         QStringLiteral("the worker describes the features of the interpreter"));
            if(reply.json.value(QStringLiteral("numpy")).isNull())
                qInfo().noquote() << "numpy       : absent (the worker runs its pure-Python path)";
            else
                qInfo().noquote() << "numpy       :" << reply.json.value(QStringLiteral("numpy")).toString();
        }
        if(i + 1 < options.startups)
            worker.kill();
    }
    const QStringList advertisedModes = [&] {
        QStringList modes;
        for(const QJsonValue& value : handshake.value(QStringLiteral("transferModes")).toArray())
            modes.push_back(value.toString());
        return modes;
    }();
    report.insert(QStringLiteral("workerFeatures"), handshake.value(QStringLiteral("features")));
    report.insert(QStringLiteral("workerTransferModes"), handshake.value(QStringLiteral("transferModes")));
    qInfo().noquote() << "payload     :" << QStringLiteral("%1 particles (%2) + %3 triangles (%4)")
                                             .arg(options.particles)
                                             .arg(megabytes(static_cast<qint64>(options.particles) * 3 * sizeof(double)))
                                             .arg(options.faces)
                                             .arg(megabytes(static_cast<qint64>(options.faces) * 3 * sizeof(int)));
    qInfo().noquote();

    // -----------------------------------------------------------------------------------------------------------------
    // 2. One worker for everything else: control latency, the transfer comparison, cancellation, crash recovery.
    // -----------------------------------------------------------------------------------------------------------------
    WorkerProcess worker;
    QString startError;
    if(!worker.start(options.interpreter, options.worker, &startError)) {
        checks.check(false, QStringLiteral("the interpreter starts for the measurements: %1").arg(startError));
        return 1;
    }
    const int handshakeId = worker.allocateRequestId();
    const WorkerReply handshakeReply =
        worker.request(QJsonObject({ { QStringLiteral("id"), handshakeId }, { QStringLiteral("op"), QStringLiteral("handshake") } }));
    checks.check(handshakeReply.ok, QStringLiteral("the worker is ready for the measurements"));

    Samples ping;
    ping.label = QStringLiteral("control round trip (ping)");
    for(int i = 0; i < options.pings; ++i) {
        const int id = worker.allocateRequestId();
        const WorkerReply reply =
            worker.request(QJsonObject({ { QStringLiteral("id"), id }, { QStringLiteral("op"), QStringLiteral("ping") } }), 30000);
        if(!checks.check(reply.ok, QStringLiteral("ping %1 answers").arg(i)))
            break;
        ping.add(reply.elapsedMs);
    }

    // -----------------------------------------------------------------------------------------------------------------
    // 3. The transfer comparison. One entry per mode, with the checks that make the modes comparable.
    // -----------------------------------------------------------------------------------------------------------------
    QVector<Samples> transfers;
    QJsonObject transferJson;
    QByteArray framedPayload;
    QByteArray sharedMemoryPayload;
    double referenceChecksum = std::numeric_limits<double>::quiet_NaN();
    qint64 referenceIndicesChecksum = 0;
    for(const QString& mode : options.modes) {
        if(!advertisedModes.isEmpty() && !advertisedModes.contains(mode)) {
            qInfo().noquote() << "  skipping transfer mode" << mode << "(the worker does not offer it)";
            continue;
        }
        Samples samples;
        samples.label = QStringLiteral("evaluation, transfer %1").arg(mode);
        QJsonObject lastJson;
        QByteArray lastPayload;
        qint64 payloadBytes = 0;
        for(int i = 0; i < options.iterations; ++i) {
            const int id = worker.allocateRequestId();
            const WorkerReply reply = worker.request(QJsonObject({ { QStringLiteral("id"), id },
                                                                   { QStringLiteral("op"), QStringLiteral("process") },
                                                                   { QStringLiteral("particles"), options.particles },
                                                                   { QStringLiteral("faces"), options.faces },
                                                                   { QStringLiteral("seed"), 2.0 },
                                                                   { QStringLiteral("transfer"), mode } }),
                                                     options.timeoutMs);
            if(!reply.ok) {
                checks.check(false, QStringLiteral("%1 answers: %2").arg(mode, reply.errorMessage));
                samples.values.clear();
                break;
            }
            samples.add(reply.elapsedMs);
            lastJson = reply.json;
            lastPayload = reply.payload;
            payloadBytes = std::max<qint64>(payloadBytes, reply.payload.size());
        }
        if(samples.isEmpty())
            continue;

        const double checksum = lastJson.value(QStringLiteral("checksum")).toDouble();
        const qint64 indicesChecksum = lastJson.value(QStringLiteral("indicesChecksum")).toInteger();
        const bool buildsArrays = mode != QStringLiteral("none");
        if(buildsArrays && std::isnan(referenceChecksum)) {
            referenceChecksum = checksum;
            referenceIndicesChecksum = indicesChecksum;
        }
        if(buildsArrays)
            checks.check(checksum == referenceChecksum && indicesChecksum == referenceIndicesChecksum,
                         QStringLiteral("%1 transfers the same data as the other modes (checksums %2 / %3)")
                             .arg(mode)
                             .arg(checksum, 0, 'g', 17)
                             .arg(indicesChecksum));

        // The framed transfer: the client verifies the hashes of what it received and keeps the bytes for the
        // comparison with the shared-memory path.
        if(mode == QStringLiteral("framed")) {
            const qint64 positionsBytes = lastJson.value(QStringLiteral("positionsBytes")).toInteger();
            const qint64 indicesBytes = lastJson.value(QStringLiteral("indicesBytes")).toInteger();
            framedPayload = lastPayload;
            checks.check(framedPayload.size() == positionsBytes + indicesBytes,
                         QStringLiteral("the framed transfer carries exactly the announced bytes (%1)").arg(framedPayload.size()));
            checks.check(sha256(framedPayload.left(positionsBytes)) == lastJson.value(QStringLiteral("sha256positions")).toString(),
                         QStringLiteral("the received positions hash to the hash the worker reported"));
            checks.check(sha256(framedPayload.mid(positionsBytes, indicesBytes)) == lastJson.value(QStringLiteral("sha256indices")).toString(),
                         QStringLiteral("the received indices hash to the hash the worker reported"));
            // The data itself: the first particle's coordinates follow the formula both sides share. Only single
            // operations are compared, because a compiler may contract a multiply-add differently from Python.
            const QVector<double> values = leadingDoubles(framedPayload, 3);
            checks.check(values.size() == 3, QStringLiteral("the framed transfer starts with a full particle"));
            if(values.size() == 3) {
                checks.check(values.at(0) == 2.0 * PositionSeedScale, QStringLiteral("the first x coordinate matches the formula"));
                checks.check(values.at(1) == 2.0, QStringLiteral("the first y coordinate matches the formula"));
                checks.check(values.at(2) == 0.0, QStringLiteral("the first z coordinate matches the formula"));
                checks.check(leadingDoubles(framedPayload.mid(3 * sizeof(double)), 1).value(0) ==
                                 PositionScaleX + 2.0 * PositionSeedScale,
                             QStringLiteral("the second x coordinate matches the formula"));
            }
        }
        else if(mode == QStringLiteral("shared-memory")) {
            const QJsonObject shared = lastJson.value(QStringLiteral("sharedMemory")).toObject();
            QString error;
            sharedMemoryPayload = WorkerProcess::readSharedMemory(shared.value(QStringLiteral("name")).toString(),
                                                                  shared.value(QStringLiteral("positionsBytes")).toInteger() +
                                                                      shared.value(QStringLiteral("indicesBytes")).toInteger(),
                                                                  &error);
            checks.check(!sharedMemoryPayload.isEmpty(), QStringLiteral("the shared memory segment can be mapped: %1").arg(error));
            const qint64 positionsBytes = shared.value(QStringLiteral("positionsBytes")).toInteger();
            if(!sharedMemoryPayload.isEmpty()) {
                checks.check(sha256(sharedMemoryPayload.left(positionsBytes)) ==
                                 shared.value(QStringLiteral("sha256positions")).toString(),
                             QStringLiteral("the mapped positions hash to the hash the worker reported"));
                checks.check(sharedMemoryPayload == framedPayload,
                             QStringLiteral("the shared-memory bytes are identical to the framed bytes"));
                payloadBytes = sharedMemoryPayload.size();
            }
        }
        else if(mode == QStringLiteral("text") || mode == QStringLiteral("base64")) {
            // These two carry the array inside the JSON message; the client decodes what it received and compares it
            // with the raw form, which is what makes the size and time comparison meaningful. The raw form comes from
            // the framed mode, so a run that measures text without it cannot check the values.
            if(framedPayload.isEmpty())
                qWarning().noquote() << "  the framed mode was not measured before " << mode
                                     << "- the payload comparison is skipped";
            const QString payloadText = lastJson.value(QStringLiteral("payload")).toString();
            checks.check(!payloadText.isEmpty(), QStringLiteral("%1 carries its payload in the response").arg(mode));
            QByteArray decoded;
            if(mode == QStringLiteral("base64")) {
                // The base64 form is a JSON object of two encoded buffers; decoding them yields exactly the raw layout.
                const QJsonObject encoded = QJsonDocument::fromJson(payloadText.toUtf8()).object();
                decoded = QByteArray::fromBase64(encoded.value(QStringLiteral("positions")).toString().toLatin1());
                decoded.append(QByteArray::fromBase64(encoded.value(QStringLiteral("indices")).toString().toLatin1()));
            }
            else {
                // The text form is `[positions, indices]` with a nested list per particle.
                const QJsonArray positions = QJsonDocument::fromJson(payloadText.toUtf8()).array().at(0).toArray();
                for(const QJsonValue& particle : positions) {
                    for(const QJsonValue& coordinate : particle.toArray()) {
                        const double value = coordinate.toDouble();
                        decoded.append(reinterpret_cast<const char*>(&value), sizeof(double));
                    }
                }
            }
            payloadBytes = payloadText.size();
            if(!framedPayload.isEmpty()) {
                if(mode == QStringLiteral("base64")) {
                    checks.check(decoded.left(SpotCheckValues * sizeof(double)) ==
                                     framedPayload.left(SpotCheckValues * sizeof(double)),
                                 QStringLiteral("the decoded base64 payload equals the raw payload"));
                }
                else {
                    const QVector<double> values = leadingDoubles(decoded, SpotCheckValues);
                    const QVector<double> raw = leadingDoubles(framedPayload, SpotCheckValues);
                    checks.check(values == raw, QStringLiteral("the decoded JSON numbers equal the raw values"));
                }
            }
        }

        const QString note = payloadBytes > 0 ? megabytes(payloadBytes) : QStringLiteral("no payload");
        samples.note = note;
        transfers.push_back(samples);
        QJsonObject entry;
        entry.insert(QStringLiteral("medianMs"), samples.median());
        entry.insert(QStringLiteral("minMs"), samples.minimum());
        entry.insert(QStringLiteral("maxMs"), samples.maximum());
        entry.insert(QStringLiteral("samples"), samples.values.size());
        entry.insert(QStringLiteral("payloadBytes"), static_cast<double>(payloadBytes));
        entry.insert(QStringLiteral("workerComputeMs"), lastJson.value(QStringLiteral("computeMs")).toDouble());
        entry.insert(QStringLiteral("workerSerializeMs"), lastJson.value(QStringLiteral("serializeMs")).toDouble());
        transferJson.insert(mode, entry);
    }
    report.insert(QStringLiteral("transfers"), transferJson);

    // -----------------------------------------------------------------------------------------------------------------
    // 4. Cancellation: a long operation and the cancel message that stops it travel over the same channel.
    // -----------------------------------------------------------------------------------------------------------------
    double cancellationLatency = -1.0;
    {
        const int sleepId = worker.allocateRequestId();
        if(worker.send(QJsonObject({ { QStringLiteral("id"), sleepId },
                                     { QStringLiteral("op"), QStringLiteral("sleep") },
                                     { QStringLiteral("seconds"), 5.0 } }))) {
            QThread::msleep(150);   // let the operation start, so the cancel really interrupts work
            const int cancelId = worker.allocateRequestId();
            QElapsedTimer timer;
            timer.start();
            worker.send(QJsonObject({ { QStringLiteral("id"), cancelId },
                                      { QStringLiteral("op"), QStringLiteral("cancel") },
                                      { QStringLiteral("target"), sleepId } }));
            WorkerReply ack;
            checks.check(worker.receive(ack, 30000) && ack.ok, QStringLiteral("the cancel message is acknowledged"));
            WorkerReply cancelled;
            if(worker.receive(cancelled, 30000)) {
                cancellationLatency = timer.nsecsElapsed() / 1e6;
                checks.check(!cancelled.ok && cancelled.errorCode == QStringLiteral("cancelled"),
                             QStringLiteral("the running operation reports cancellation (%1)").arg(cancelled.errorCode));
                checks.check(cancellationLatency < 2000.0,
                             QStringLiteral("cancellation reaches the operation (%1 ms)").arg(milliseconds(cancellationLatency)));
            }
            else {
                checks.check(false, QStringLiteral("the cancelled operation answers"));
            }
            // The interpreter must survive a cancellation: a worker that has to be restarted after every cancel would
            // be no better than one process per evaluation.
            const int pingId = worker.allocateRequestId();
            const WorkerReply afterCancel =
                worker.request(QJsonObject({ { QStringLiteral("id"), pingId }, { QStringLiteral("op"), QStringLiteral("ping") } }), 30000);
            checks.check(afterCancel.ok, QStringLiteral("the worker stays usable after a cancellation"));
        }
        else {
            checks.check(false, QStringLiteral("the long operation starts"));
        }
        report.insert(QStringLiteral("cancellationLatencyMs"), cancellationLatency);
    }

    // -----------------------------------------------------------------------------------------------------------------
    // 5. Crash recovery: the worker dies on purpose, and the client must notice instead of waiting for a timeout.
    // -----------------------------------------------------------------------------------------------------------------
    double crashNoticedMs = -1.0;
    double recoveryMs = -1.0;
    {
        const int crashId = worker.allocateRequestId();
        QElapsedTimer timer;
        timer.start();
        const WorkerReply crashReply =
            worker.request(QJsonObject({ { QStringLiteral("id"), crashId }, { QStringLiteral("op"), QStringLiteral("crash") } }), 30000);
        crashNoticedMs = timer.nsecsElapsed() / 1e6;
        checks.check(!crashReply.ok && crashReply.errorCode == QStringLiteral("worker_unavailable"),
                     QStringLiteral("a dead worker is reported instead of a timeout"));
        checks.check(crashReply.errorMessage.contains(QStringLiteral("exited with code 3")),
                     QStringLiteral("the death carries the exit code (%1)").arg(crashReply.errorMessage));
        checks.check(!worker.isRunning(), QStringLiteral("the client sees the worker is gone"));

        QElapsedTimer recoveryTimer;
        recoveryTimer.start();
        QString error;
        if(worker.start(options.interpreter, options.worker, &error)) {
            const int id = worker.allocateRequestId();
            const WorkerReply reply =
                worker.request(QJsonObject({ { QStringLiteral("id"), id }, { QStringLiteral("op"), QStringLiteral("handshake") } }));
            recoveryMs = recoveryTimer.nsecsElapsed() / 1e6;
            checks.check(reply.ok, QStringLiteral("a new worker is usable after the crash"));
        }
        else {
            checks.check(false, QStringLiteral("a new worker can be started after the crash: %1").arg(error));
        }
        report.insert(QStringLiteral("crashNoticedMs"), crashNoticedMs);
        report.insert(QStringLiteral("recoveryMs"), recoveryMs);
    }

    // -----------------------------------------------------------------------------------------------------------------
    // 6. Frame change: an evaluation per animation frame, which is what the pipeline does when the time changes.
    // -----------------------------------------------------------------------------------------------------------------
    Samples frames;
    frames.label = QStringLiteral("evaluation per frame");
    {
        QSet<QString> checksums;
        for(int i = 0; i < options.frames; ++i) {
            const int id = worker.allocateRequestId();
            const WorkerReply reply = worker.request(QJsonObject({ { QStringLiteral("id"), id },
                                                                   { QStringLiteral("op"), QStringLiteral("process") },
                                                                   { QStringLiteral("particles"), std::min(options.particles, 200000) },
                                                                   { QStringLiteral("faces"), std::min(options.faces, 80000) },
                                                                   { QStringLiteral("seed"), 1000.0 + i },
                                                                   { QStringLiteral("transfer"), QStringLiteral("framed") } }),
                                                     options.timeoutMs);
            if(!checks.check(reply.ok, QStringLiteral("frame %1 evaluates").arg(i)))
                break;
            frames.add(reply.elapsedMs);
            checksums.insert(QString::number(reply.json.value(QStringLiteral("checksum")).toDouble(), 'g', 17));
        }
        checks.check(checksums.size() == frames.values.size(),
                     QStringLiteral("every frame produces its own result, so nothing is served from a stale cache (%1 distinct of %2)")
                         .arg(checksums.size())
                         .arg(frames.values.size()));
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Report.
    // -----------------------------------------------------------------------------------------------------------------
    qInfo().noquote();
    printSamples(startup);
    printSamples(ping);
    for(const Samples& samples : transfers)
        printSamples(samples);
    if(!frames.isEmpty())
        printSamples(frames);
    if(cancellationLatency >= 0.0)
        qInfo().noquote() << QStringLiteral("  cancellation: %1 ms from sending cancel to the cancelled answer")
                                 .arg(milliseconds(cancellationLatency));
    if(crashNoticedMs >= 0.0)
        qInfo().noquote() << QStringLiteral("  crash recovery: noticed after %1 ms, restart + handshake %2 ms")
                                 .arg(milliseconds(crashNoticedMs), milliseconds(recoveryMs));
    qInfo().noquote();
    qInfo().noquote() << QStringLiteral("  checks: %1 passed, %2 failed").arg(checks.passed()).arg(checks.failures().size());
    for(const QString& failure : checks.failures())
        qInfo().noquote() << QStringLiteral("    - %1").arg(failure);

    QJsonObject measurements;
    measurements.insert(QStringLiteral("startup"), QJsonObject({ { QStringLiteral("medianMs"), startup.median() },
                                                                 { QStringLiteral("minMs"), startup.minimum() },
                                                                 { QStringLiteral("maxMs"), startup.maximum() },
                                                                 { QStringLiteral("samples"), startup.values.size() } }));
    measurements.insert(QStringLiteral("ping"), QJsonObject({ { QStringLiteral("medianMs"), ping.median() },
                                                              { QStringLiteral("p95Ms"), ping.percentile(0.95) },
                                                              { QStringLiteral("samples"), ping.values.size() } }));
    measurements.insert(QStringLiteral("frames"), QJsonObject({ { QStringLiteral("medianMs"), frames.median() },
                                                                { QStringLiteral("samples"), frames.values.size() } }));
    report.insert(QStringLiteral("measurements"), measurements);
    report.insert(QStringLiteral("checks"),
                  QJsonObject({ { QStringLiteral("passed"), checks.passed() },
                                { QStringLiteral("total"), checks.total() },
                                { QStringLiteral("failures"), QJsonArray::fromStringList(checks.failures()) } }));
    report.insert(QStringLiteral("quick"), options.quick);

    const QByteArray json = QJsonDocument(report).toJson(QJsonDocument::Indented);
    if(options.jsonPath.isEmpty()) {
        qInfo().noquote() << json;
    }
    else {
        QFile file(options.jsonPath);
        if(file.open(QIODevice::WriteOnly)) {
            file.write(json);
            qInfo().noquote() << "wrote" << options.jsonPath;
        }
        else {
            qWarning().noquote() << "cannot write" << options.jsonPath;
        }
    }

    worker.kill();
    worker.waitForFinished(5000);
    return checks.failures().isEmpty() ? 0 : 1;
}
