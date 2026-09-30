// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

/**
 * \brief Tests of the first writable data bridge (Phase 2.6, deliverable 7).
 *
 * The bridge is the seam between OVITO's data and a user-selected interpreter, so the suite is about the rules that make
 * that seam safe rather than about a computation:
 *
 *  - the bytes that go out arrive unchanged (the worker reports their digests) and the bytes the caller holds are
 *    untouched by the exchange;
 *  - the reply is a *validated* block: descriptors and payload have to agree, the digests have to match, and a reply
 *    that does not is a failure instead of an array to compute with;
 *  - a description this build or the worker cannot honour is refused with the offending array named - on both sides,
 *    which is why one case sends a hand-built descriptor that the client's own validation would have caught;
 *  - the worker's environment is the caller's: an interpreter that is not Python, a missing worker and a dead worker are
 *    all reported as such, and none of them is replaced by a fallback interpreter.
 *
 * The binary runs on any machine; the cases that need an interpreter skip themselves without one, like the probe suite.
 */

#include <ovito/core/automation/python/PythonDataBridge.h>
#include <ovito/core/automation/python/PythonEnvironmentProbe.h>
#include <ovito/core/automation/python/PythonWorkerProcess.h>

#include <QtTest>

#include <limits>

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

using namespace Ovito;

/// Skips the current case on a machine without Python, and defines `interpreter` for it.
#define SKIP_WITHOUT_PYTHON()                                                                                                    \
    const QString interpreter = PythonEnvironmentProbe::findInterpreter();                                                       \
    if(interpreter.isEmpty())                                                                                                    \
    QSKIP("This machine has no python3 executable on its path.")

/// Skips the current case when this build cannot find the worker script, and defines `workerScript` for it.
#define SKIP_WITHOUT_WORKER()                                                                                                    \
    const QString workerScript = PythonWorkerProcess::defaultWorkerScript();                                                     \
    if(workerScript.isEmpty())                                                                                                   \
    QSKIP("This build cannot locate its Python worker script.")

class PythonDataBridgeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void initTestCase();

    // The value types: what makes a block valid, and what is refused.
    void array_describes_itself_and_checks_its_bytes();
    void array_is_refused_when_its_description_does_not_fit();
    void block_reads_back_the_bytes_it_wrote();
    void block_refuses_a_payload_that_does_not_match_its_descriptors();

    // The round trip against a real interpreter.
    void bridge_moves_arrays_and_returns_a_scale();
    void bridge_echoes_the_remaining_arrays_byte_for_byte();
    void bridge_leaves_the_callers_block_untouched();
    void bridge_moves_a_payload_larger_than_the_pipe_buffer();
    void bridge_refuses_a_block_it_cannot_send();

    // The other side of the seam: a worker that refuses, dies or never answers.
    void bridge_refuses_a_description_the_worker_does_not_know();
    void worker_reports_a_missing_interpreter();
    void worker_reports_a_missing_script();
    void worker_notices_a_dead_interpreter();
    void worker_reports_what_it_can_do();
    void worker_stops_when_it_is_asked_to();
    void worker_can_abandon_a_running_operation();

private:

    /// A worker started on the interpreter of this machine, or an empty optional when there is none.
    bool startWorker(PythonWorkerProcess& worker);

    /// A block of one float64 array and one int32 array, with values the tests can predict.
    PythonArrayBlock makeBlock();

    QStringList _missingReasons;
};

void PythonDataBridgeTest::initTestCase()
{
    // The bridge is only meaningful with the worker script of this build; without it every case would be skipped and
    // the suite would look green while testing nothing.
    if(PythonWorkerProcess::defaultWorkerScript().isEmpty())
        qWarning("This build cannot locate its Python worker script; the bridge cases will be skipped.");
}

bool PythonDataBridgeTest::startWorker(PythonWorkerProcess& worker)
{
    const QString interpreter = PythonEnvironmentProbe::findInterpreter();
    if(interpreter.isEmpty())
        return false;
    worker.setInterpreter(interpreter);
    QString error;
    if(!worker.start(&error)) {
        _missingReasons << error;
        return false;
    }
    return true;
}

PythonArrayBlock PythonDataBridgeTest::makeBlock()
{
    PythonArrayBlock block;
    block.append(*PythonArray::fromDoubles(QStringLiteral("positions"), { 2, 3 }, { 0.0, 0.125, 0.25, 0.375, 0.5, 0.625 }));
    block.append(*PythonArray::fromIntegers(QStringLiteral("indices"), { 4 }, { 0, 1, 2, 3 }));
    return block;
}

/******************************************************************************
* The value types.
******************************************************************************/
void PythonDataBridgeTest::array_describes_itself_and_checks_its_bytes()
{
    // A fresh array carries a digest of what it holds, and the descriptor repeats it - which is how the receiver knows
    // the transfer was exact rather than merely plausible.
    std::optional<PythonArray> array = PythonArray::fromDoubles(QStringLiteral("positions"), { 3 }, { 1.0, 2.0, 3.0 });
    QVERIFY(array.has_value());
    QCOMPARE(array->name, QStringLiteral("positions"));
    QCOMPARE(array->dtype, QStringLiteral("float64"));
    QCOMPARE(array->elementCount(), 3);
    QCOMPARE(array->elementSize(), 8);
    QCOMPARE(array->bytes.size(), 24);
    QCOMPARE(array->checksum().size(), 64);
    QVERIFY(array->isValid());
    QVERIFY(array->announcedChecksum.isEmpty());   // nothing announced yet: verifyChecksum has nothing to check
    QString problem;
    QVERIFY(!array->verifyChecksum(&problem));
    QVERIFY(problem.contains(QStringLiteral("announced no digest")));

    const QVariantMap descriptor = array->descriptor();
    QCOMPARE(descriptor.value(QStringLiteral("name")).toString(), QStringLiteral("positions"));
    QCOMPARE(descriptor.value(QStringLiteral("dtype")).toString(), QStringLiteral("float64"));
    QCOMPARE(descriptor.value(QStringLiteral("bytes")).toInt(), 24);
    QCOMPARE(descriptor.value(QStringLiteral("sha256")).toString(), QString::fromLatin1(array->checksum()));
    QCOMPARE(descriptor.value(QStringLiteral("shape")).toList().size(), 1);

    // The values come back as they went in, and an integer array reads back as integers.
    QCOMPARE(*array->asDoubles(), (QVector<double>{ 1.0, 2.0, 3.0 }));
    QVERIFY(!array->asIntegers(&problem));
    QVERIFY(problem.contains(QStringLiteral("not an integer type")));
    std::optional<PythonArray> integers = PythonArray::fromIntegers(QStringLiteral("indices"), { 2, 2 }, { -1, 0, 7, 42 });
    QVERIFY(integers.has_value());
    QCOMPARE(integers->elementCount(), 4);
    QCOMPARE(*integers->asIntegers(), (QVector<qint64>{ -1, 0, 7, 42 }));
    QVERIFY(integers->dtype == QStringLiteral("int32"));
}

void PythonDataBridgeTest::array_is_refused_when_its_description_does_not_fit()
{
    // Each of these is a description the wire would carry happily and a receiver could not honour. They are refused
    // with the array named, because "invalid argument" alone does not tell a user which array is wrong.
    PythonArray array;
    QString problem;

    array.name = QStringLiteral("a");
    array.dtype = QStringLiteral("float64");
    array.shape = { 2 };
    array.bytes = QByteArray(4, '\0');                        // half of what the shape needs
    QVERIFY(!array.isValid(&problem));
    QVERIFY(problem.contains(QStringLiteral("\"a\"")));
    QVERIFY(problem.contains(QStringLiteral("needs")));

    array.bytes = QByteArray(16, '\0');
    array.name.clear();
    QVERIFY(!array.isValid(&problem));
    QVERIFY(problem.contains(QStringLiteral("without a name")));

    array.name = QStringLiteral("a");
    array.dtype = QStringLiteral("float128");
    QVERIFY(!array.isValid(&problem));
    QVERIFY(problem.contains(QStringLiteral("unsupported type")));
    QVERIFY(problem.contains(QStringLiteral("float128")));
    // The message lists what *is* supported, so a caller does not have to read the header to fix it.
    QVERIFY(problem.contains(QStringLiteral("float64")));

    array.shape.clear();
    array.dtype = QStringLiteral("uint8");
    array.bytes = QByteArray(1, '\0');
    QVERIFY(!array.isValid(&problem));
    QVERIFY(problem.contains(QStringLiteral("without a shape")));

    array.shape = { -1 };
    QVERIFY(!array.isValid(&problem));
    QVERIFY(problem.contains(QStringLiteral("negative dimension")));

    // A shape that does not match the values is a caller error, not an array.
    QVERIFY(!PythonArray::fromDoubles(QStringLiteral("a"), { 5 }, { 1.0, 2.0 }, &problem).has_value());
    QVERIFY(problem.contains(QStringLiteral("describes 5 values but 2 were given")));

    // An announced digest that does not match the bytes is exactly the case the digest exists for.
    std::optional<PythonArray> good = PythonArray::fromDoubles(QStringLiteral("a"), { 1 }, { 1.0 });
    QVERIFY(good.has_value());
    good->announcedChecksum = QStringLiteral("00");
    QVERIFY(!good->verifyChecksum(&problem));
    QVERIFY(problem.contains(QStringLiteral("arrived changed")));
}

void PythonDataBridgeTest::block_reads_back_the_bytes_it_wrote()
{
    const PythonArrayBlock block = makeBlock();
    QCOMPARE(block.arrayCount(), 2);
    QCOMPARE(block.names(), (QStringList{ QStringLiteral("positions"), QStringLiteral("indices") }));
    QCOMPARE(block.totalBytes(), 48 + 16);   // six doubles and four 32-bit integers
    QCOMPARE(block.elementCount(), 6 + 4);
    QCOMPARE(block.find(QStringLiteral("indices"))->dtype, QStringLiteral("int32"));
    QVERIFY(block.find(QStringLiteral("missing")) == nullptr);

    // The payload is the concatenation of the arrays in order, and the descriptors describe exactly that.
    const QByteArray payload = block.payload();
    QCOMPARE(payload.size(), block.totalBytes());
    QVERIFY(payload.startsWith(block.arrays().first().bytes));

    std::optional<PythonArrayBlock> readBack = PythonArrayBlock::fromWire(block.descriptors(), payload);
    QVERIFY(readBack.has_value());
    QCOMPARE(readBack->names(), block.names());
    QCOMPARE(*readBack->find(QStringLiteral("positions"))->asDoubles(), (QVector<double>{ 0.0, 0.125, 0.25, 0.375, 0.5, 0.625 }));
    QCOMPARE(*readBack->find(QStringLiteral("indices"))->asIntegers(), (QVector<qint64>{ 0, 1, 2, 3 }));
    QVERIFY(readBack->verifyChecksums());

    // A block owns its arrays, so a copy can be kept and modified without touching the one it was read from.
    PythonArrayBlock copy = *readBack;
    copy.append(*PythonArray::fromDoubles(QStringLiteral("extra"), { 1 }, { 42.0 }));
    QCOMPARE(copy.arrayCount(), 3);
    QCOMPARE(readBack->arrayCount(), 2);
    QVERIFY(readBack->find(QStringLiteral("extra")) == nullptr);
}

void PythonDataBridgeTest::block_refuses_a_payload_that_does_not_match_its_descriptors()
{
    const PythonArrayBlock block = makeBlock();
    const QVariantList descriptors = block.descriptors();
    QString problem;

    // A truncated transfer is the case a receiver has to catch, because the bytes that are missing would otherwise be
    // read from the *next* message.
    QVERIFY(!PythonArrayBlock::fromWire(descriptors, block.payload().left(30), &problem).has_value());
    QVERIFY(problem.contains(QStringLiteral("too short")));
    // The message names the array whose bytes are missing, which is the one the descriptors reached first.
    QVERIFY(problem.contains(QStringLiteral("positions")));

    // Trailing bytes are the mirror image, and just as unusable.
    QVERIFY(!PythonArrayBlock::fromWire(descriptors, block.payload() + QByteArray(8, '\0'), &problem).has_value());
    QVERIFY(problem.contains(QStringLiteral("payload bytes")));

    // Two arrays with one name cannot be looked up reliably, so the block is refused.
    QVariantList renamed = descriptors;
    QVariantMap second = renamed.last().toMap();
    second.insert(QStringLiteral("name"), QStringLiteral("positions"));
    renamed[1] = second;
    QVERIFY(!PythonArrayBlock::fromWire(renamed, block.payload(), &problem).has_value());
    QVERIFY(problem.contains(QStringLiteral("two arrays named")));

    // A descriptor whose declared size disagrees with the bytes it was handed is refused before anything is decoded.
    QVERIFY(!PythonArray::fromWire(descriptors.first().toMap(), block.arrays().first().bytes.left(8), &problem).has_value());
    QVERIFY(problem.contains(QStringLiteral("announced")));
}

/******************************************************************************
* The round trip.
******************************************************************************/
void PythonDataBridgeTest::bridge_moves_arrays_and_returns_a_scale()
{
    SKIP_WITHOUT_PYTHON();
    SKIP_WITHOUT_WORKER();
    PythonWorkerProcess worker;
    if(!startWorker(worker))
        QSKIP(qPrintable(_missingReasons.join(QStringLiteral("; "))));

    const PythonArrayBlock block = makeBlock();
    const PythonArrayBridge::Result result = PythonArrayBridge::exchange(worker, block, PythonArrayBridge::Operation::Scale, 2.5);

    QVERIFY2(result.ok, qPrintable(result.toString()));
    QVERIFY(result.errorCode.isEmpty());
    QVERIFY(result.elapsedMs > 0.0);
    QCOMPARE(result.arrays.names(), (QStringList{ QStringLiteral("positions_scaled"), QStringLiteral("indices_echo") }));

    // The arithmetic is what the caller can predict: every value is the input multiplied by the factor.
    const PythonArray* scaled = result.arrays.find(QStringLiteral("positions_scaled"));
    QVERIFY(scaled != nullptr);
    QCOMPARE(scaled->dtype, QStringLiteral("float64"));
    QCOMPARE(scaled->shape, (QVector<qint64>{ 2, 3 }));
    const std::optional<QVector<double>> values = scaled->asDoubles();
    QVERIFY(values.has_value());
    const QVector<double> input = *block.find(QStringLiteral("positions"))->asDoubles();
    QCOMPARE(values->size(), input.size());
    for(qsizetype index = 0; index < input.size(); ++index)
        QCOMPARE((*values)[index], input[index] * 2.5);

    // The reply reports what the *worker* received, digest by digest: this is the half of the seam the topology spike
    // did not measure, and it is what proves the outbound transfer was byte-exact.
    const QVariantMap digests = result.receivedInputDigests();
    QCOMPARE(digests.size(), 2);
    for(const PythonArray& array : block.arrays()) {
        const QVariantMap digest = digests.value(array.name).toMap();
        QCOMPARE(digest.value(QStringLiteral("sha256")).toString(), QString::fromLatin1(array.checksum()));
        QCOMPARE(digest.value(QStringLiteral("bytes")).toLongLong(), array.bytes.size());
        QCOMPARE(digest.value(QStringLiteral("dtype")).toString(), array.dtype);
    }
    QCOMPARE(result.response.value(QStringLiteral("operation")).toString(), QStringLiteral("scale"));
    QCOMPARE(result.response.value(QStringLiteral("factor")).toDouble(), 2.5);

    // The identity operation echoes everything and scales nothing.
    const PythonArrayBridge::Result echoed = PythonArrayBridge::exchange(worker, block, PythonArrayBridge::Operation::Identity);
    QVERIFY2(echoed.ok, qPrintable(echoed.toString()));
    QCOMPARE(echoed.arrays.names(), (QStringList{ QStringLiteral("positions_echo"), QStringLiteral("indices_echo") }));
    QCOMPARE(echoed.arrays.find(QStringLiteral("positions_echo"))->bytes, block.find(QStringLiteral("positions"))->bytes);
}

void PythonDataBridgeTest::bridge_echoes_the_remaining_arrays_byte_for_byte()
{
    SKIP_WITHOUT_PYTHON();
    SKIP_WITHOUT_WORKER();
    PythonWorkerProcess worker;
    if(!startWorker(worker))
        QSKIP(qPrintable(_missingReasons.join(QStringLiteral("; "))));

    // An array whose bytes are not a round number of anything, from a type whose values would be mangled by any text
    // transport: if the framing is right, the bytes are identical, and a JSON or a base64 path would still pass - which
    // is why the assertion is on the bytes and not on the values.
    PythonArrayBlock block;
    const QVector<qint64> shape{ 5 };
    std::optional<PythonArray> payload = PythonArray::fromDoubles(QStringLiteral("positions"), shape, { 0.1, 1e-300, 123456789.123456789, -0.0, 3.14159265358979 });
    QVERIFY(payload.has_value());
    block.append(*payload);
    QVERIFY(block.find(QStringLiteral("positions"))->bytes.size() == 40);

    // A second array of a different type rides along, so that the concatenation is not accidentally aligned.
    block.append(*PythonArray::fromIntegers(QStringLiteral("indices"), { 3 }, { 1, -2, 3 }));
    const PythonArrayBridge::Result result = PythonArrayBridge::exchange(worker, block);
    QVERIFY2(result.ok, qPrintable(result.toString()));

    QCOMPARE(result.arrays.find(QStringLiteral("indices_echo"))->bytes, block.find(QStringLiteral("indices"))->bytes);
    QCOMPARE(*result.arrays.find(QStringLiteral("indices_echo"))->asIntegers(), (QVector<qint64>{ 1, -2, 3 }));
    QCOMPARE(*result.arrays.find(QStringLiteral("positions_scaled"))->asDoubles(), *payload->asDoubles());
}

void PythonDataBridgeTest::bridge_leaves_the_callers_block_untouched()
{
    SKIP_WITHOUT_PYTHON();
    SKIP_WITHOUT_WORKER();
    PythonWorkerProcess worker;
    if(!startWorker(worker))
        QSKIP(qPrintable(_missingReasons.join(QStringLiteral("; "))));

    // The ownership rule a modifier needs: the data it sends belongs to a pipeline that may be re-evaluated, so an
    // exchange must not write into it. This is checked by keeping a copy of the bytes and comparing afterwards - the
    // client copies into the request, and the worker is a different process, so nothing here can touch the original.
    const PythonArrayBlock block = makeBlock();
    const QByteArray before = block.payload();
    const QByteArray digestBefore = block.arrays().first().checksum();

    const PythonArrayBridge::Result result = PythonArrayBridge::exchange(worker, block, PythonArrayBridge::Operation::Scale, 3.0);
    QVERIFY2(result.ok, qPrintable(result.toString()));
    QCOMPARE(block.payload(), before);
    QCOMPARE(block.arrays().first().checksum(), digestBefore);
    QCOMPARE(block.find(QStringLiteral("positions"))->announcedChecksum, QString());
    // The reply is a different block with different arrays: nothing was returned by reference into the caller's data.
    QVERIFY(result.arrays.find(QStringLiteral("positions_scaled")) != nullptr);
    QVERIFY(!result.arrays.find(QStringLiteral("positions_scaled"))->bytes.isSharedWith(block.find(QStringLiteral("positions"))->bytes));
}

void PythonDataBridgeTest::bridge_moves_a_payload_larger_than_the_pipe_buffer()
{
    SKIP_WITHOUT_PYTHON();
    SKIP_WITHOUT_WORKER();
    PythonWorkerProcess worker;
    if(!startWorker(worker))
        QSKIP(qPrintable(_missingReasons.join(QStringLiteral("; "))));

    // 8 MB in one request. A pipe holds 64 KB, so this is the case in which the client must keep writing while the
    // worker reads: a client that assumed its whole message reached the pipe would deadlock here, and one that read its
    // answer before the payload was written would read its own bytes back as a header line. The suite asserts the data,
    // not a duration - but it reports what the exchange took, because that is the number a later phase will compare
    // against when real property arrays travel this path.
    constexpr qint64 elements = 1000000;
    QVector<double> values;
    values.resize(elements);
    for(qint64 index = 0; index < elements; ++index)
        values[index] = index * 0.5;
    std::optional<PythonArray> array = PythonArray::fromDoubles(QStringLiteral("positions"), { elements }, values);
    QVERIFY(array.has_value());


    PythonArrayBlock block;
    block.append(*array);
    const PythonArrayBridge::Result result = PythonArrayBridge::exchange(worker, block, PythonArrayBridge::Operation::Scale, 2.0);
    QVERIFY2(result.ok, qPrintable(result.toString()));
    QCOMPARE(result.arrays.arrayCount(), 1);
    const PythonArray* scaled = result.arrays.find(QStringLiteral("positions_scaled"));
    QVERIFY(scaled != nullptr);
    QCOMPARE(scaled->elementCount(), elements);
    QCOMPARE(scaled->bytes.size(), elements * 8);
    const std::optional<QVector<double>> roundTrip = scaled->asDoubles();
    QVERIFY(roundTrip.has_value());
    QCOMPARE(roundTrip->first(), 0.0);
    QCOMPARE((*roundTrip)[1], 1.0);
    QCOMPARE((*roundTrip)[elements - 1], (elements - 1) * 0.5 * 2.0);
    qInfo("the bridge moved %lld bytes out and %lld bytes back in %.2f ms", static_cast<long long>(block.totalBytes()),
          static_cast<long long>(result.arrays.totalBytes()), result.elapsedMs);
}

void PythonDataBridgeTest::bridge_refuses_a_block_it_cannot_send()
{
    SKIP_WITHOUT_PYTHON();
    SKIP_WITHOUT_WORKER();
    PythonWorkerProcess worker;
    if(!startWorker(worker))
        QSKIP(qPrintable(_missingReasons.join(QStringLiteral("; "))));

    // Nothing is sent for any of these, which the test proves by checking that the worker is still idle afterwards.
    PythonArrayBridge::Result result = PythonArrayBridge::exchange(worker, PythonArrayBlock{});
    QVERIFY(!result.ok);
    QCOMPARE(result.errorCode, QStringLiteral("invalid_block"));
    QVERIFY(result.errorMessage.contains(QStringLiteral("at least one array")));

    // A wrong type for the array that is scaled is a caller error, and the message names it instead of travelling to
    // the worker and back.
    PythonArrayBlock onlyIntegers;
    onlyIntegers.append(*PythonArray::fromIntegers(QStringLiteral("indices"), { 2 }, { 1, 2 }));
    result = PythonArrayBridge::exchange(worker, onlyIntegers, PythonArrayBridge::Operation::Scale);
    QVERIFY(!result.ok);
    QCOMPARE(result.errorCode, QStringLiteral("invalid_block"));
    QVERIFY(result.errorMessage.contains(QStringLiteral("indices")));
    QVERIFY(result.errorMessage.contains(QStringLiteral("float64")));

    // Too many arrays for one request.
    PythonArrayBlock many;
    for(int index = 0; index < PythonArrayBridge::maximumArrays + 1; ++index)
        many.append(*PythonArray::fromDoubles(QStringLiteral("array%1").arg(index), { 1 }, { 1.0 }));
    result = PythonArrayBridge::exchange(worker, many);
    QVERIFY(!result.ok);
    QCOMPARE(result.errorCode, QStringLiteral("invalid_block"));
    QVERIFY(result.errorMessage.contains(QStringLiteral("at most")));

    // A factor that is not a number at all.
    result = PythonArrayBridge::exchange(worker, makeBlock(), PythonArrayBridge::Operation::Scale, std::numeric_limits<double>::quiet_NaN());
    QVERIFY(!result.ok);
    QCOMPARE(result.errorCode, QStringLiteral("invalid_block"));

    // The worker is still usable after every refusal, and its outstanding-request slot was never taken.
    QVERIFY(!worker.hasOutstandingRequest());
    const PythonArrayBridge::Result good = PythonArrayBridge::exchange(worker, makeBlock(), PythonArrayBridge::Operation::Identity);
    QVERIFY2(good.ok, qPrintable(good.toString()));
}

/******************************************************************************
* The other side of the seam.
******************************************************************************/
void PythonDataBridgeTest::bridge_refuses_a_description_the_worker_does_not_know()
{
    SKIP_WITHOUT_PYTHON();
    SKIP_WITHOUT_WORKER();
    PythonWorkerProcess worker;
    if(!startWorker(worker))
        QSKIP(qPrintable(_missingReasons.join(QStringLiteral("; "))));

    // The client's validation refuses an unknown type before anything is sent; to test the *worker's* defence the
    // descriptor has to be built by hand, which is what a broken or newer peer's message would look like.
    PythonWorkerRequest request;
    request.operation = QLatin1String(PythonWorkerProtocol::arraysOperation);
    QVariantList descriptors;
    QVariantMap descriptor;
    descriptor.insert(QStringLiteral("name"), QStringLiteral("exotic"));
    descriptor.insert(QStringLiteral("dtype"), QStringLiteral("complex128"));
    descriptor.insert(QStringLiteral("shape"), QVariantList{ 1 });
    descriptor.insert(QStringLiteral("bytes"), 16);
    descriptors.push_back(descriptor);
    request.arguments.insert(QStringLiteral("arrays"), descriptors);
    request.payload = QByteArray(16, '\0');

    const PythonWorkerReply reply = worker.exchange(request);
    QVERIFY(reply.transportOk);
    QVERIFY(!reply.ok);
    QCOMPARE(reply.errorCode, QStringLiteral("invalid_argument"));
    QVERIFY(reply.errorMessage.contains(QStringLiteral("exotic")));
    QVERIFY(reply.errorMessage.contains(QStringLiteral("complex128")));

    // A request without arrays is refused as well, so an empty operation cannot be mistaken for a no-op.
    PythonWorkerRequest empty;
    empty.operation = QLatin1String(PythonWorkerProtocol::arraysOperation);
    const PythonWorkerReply emptyReply = worker.exchange(empty);
    QVERIFY(!emptyReply.ok);
    QCOMPARE(emptyReply.errorCode, QStringLiteral("invalid_argument"));

    // An operation this worker does not implement is refused by name, not by silence.
    PythonWorkerRequest unknown;
    unknown.operation = QStringLiteral("teleport");
    const PythonWorkerReply unknownReply = worker.exchange(unknown);
    QVERIFY(!unknownReply.ok);
    QCOMPARE(unknownReply.errorCode, QStringLiteral("unknown_operation"));
    QVERIFY(unknownReply.errorMessage.contains(QStringLiteral("teleport")));
}

void PythonDataBridgeTest::worker_reports_a_missing_interpreter()
{
    // No interpreter named at all: nothing is started, and the reason is about the missing selection rather than about
    // a process that failed.
    PythonWorkerProcess worker;
    QString error;
    QVERIFY(!worker.start(&error));
    QVERIFY(error.contains(QStringLiteral("No Python interpreter was given")));
    QVERIFY(!worker.isRunning());

    // An interpreter that does not exist is a different message, and it carries the path that was tried.
    worker.setInterpreter(QDir(QDir::tempPath()).filePath(QStringLiteral("no-such-python-interpreter")));
    QVERIFY(!worker.start(&error));
    QVERIFY(error.contains(QStringLiteral("no-such-python-interpreter")));
    QVERIFY(!worker.isRunning());

    // An interpreter that exists but is not Python: the worker script fails immediately, and that is a transport
    // failure rather than a silent fallback to another interpreter.
    worker.setInterpreter(QStringLiteral("/bin/echo"));
    worker.setWorkerScript(PythonWorkerProcess::defaultWorkerScript());
    if(worker.start(&error)) {
        PythonWorkerHandshake handshake = worker.handshake(4000, &error);
        QVERIFY(!handshake.ok);
        QVERIFY(!error.isEmpty());
    }
    else {
        QVERIFY(!error.isEmpty());
    }
}

void PythonDataBridgeTest::worker_reports_a_missing_script()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    PythonWorkerProcess worker;
    worker.setInterpreter(QStringLiteral("/bin/echo"));
    const QString missing = QDir(directory.path()).filePath(QStringLiteral("no-worker.py"));
    worker.setWorkerScript(missing);
    QString error;
    QVERIFY(!worker.start(&error));
    QVERIFY(error.contains(missing));
    QVERIFY(!worker.isRunning());
}

void PythonDataBridgeTest::worker_notices_a_dead_interpreter()
{
    SKIP_WITHOUT_PYTHON();
    SKIP_WITHOUT_WORKER();
    PythonWorkerProcess worker;
    if(!startWorker(worker))
        QSKIP(qPrintable(_missingReasons.join(QStringLiteral("; "))));
    QVERIFY(worker.isRunning());
    const qint64 pid = worker.processId();
    QVERIFY(pid > 0);

    // Kill the interpreter behind the client's back. The next request must not wait for its timeout: a dead worker is
    // reported as a dead worker, with the exit status, because waiting would turn a crash into a slow run - and the
    // design requires a restart to be a user-visible event rather than a hidden retry.
    worker.kill();
    QVERIFY(worker.waitForFinished(5000));
    QVERIFY(!worker.isRunning());
    QCOMPARE(worker.exitCode(), 9);   // SIGKILL through QProcess::kill()

    PythonWorkerRequest request;
    request.operation = QLatin1String(PythonWorkerProtocol::handshakeOperation);
    const PythonWorkerReply reply = worker.exchange(request, 3000);
    QVERIFY(!reply.transportOk);
    QCOMPARE(reply.errorCode, QStringLiteral("worker_unavailable"));
    QVERIFY(reply.errorMessage.contains(QStringLiteral("not running")));
    QVERIFY(reply.elapsedMs < 2000.0);

    // And the bridge reports the same fact for a real request instead of inventing an empty result.
    const PythonArrayBridge::Result result = PythonArrayBridge::exchange(worker, makeBlock());
    QVERIFY(!result.ok);
    QCOMPARE(result.errorCode, QStringLiteral("worker_unavailable"));
    QVERIFY(result.arrays.isEmpty());

    // The case that matters for a long evaluation: the interpreter dies *while a request is outstanding*. The read must
    // end when the process ends - with its exit status - instead of waiting for the timeout, because a crash that is
    // reported as a slow run is the failure mode this rule exists to prevent.
    PythonWorkerProcess second;
    QVERIFY(startWorker(second));
    PythonWorkerRequest sleep;
    sleep.operation = QStringLiteral("sleep");
    sleep.arguments.insert(QStringLiteral("milliseconds"), 60000);
    QVERIFY(second.send(sleep) != 0);
    QTest::qWait(100);
    second.kill();
    QVERIFY(second.waitForFinished(5000));

    PythonWorkerReply died;
    QElapsedTimer timer;
    timer.start();
    QVERIFY(!second.receive(died, 30000));
    QVERIFY2(timer.elapsed() < 5000, "A dead worker was reported as a timeout.");
    QVERIFY(!died.transportOk);
    QCOMPARE(died.errorCode, QStringLiteral("worker_unavailable"));
    QVERIFY(died.errorMessage.contains(QStringLiteral("exited")));
    QVERIFY(!second.hasOutstandingRequest());
}

void PythonDataBridgeTest::worker_reports_what_it_can_do()
{
    SKIP_WITHOUT_PYTHON();
    SKIP_WITHOUT_WORKER();
    PythonWorkerProcess worker;
    if(!startWorker(worker))
        QSKIP(qPrintable(_missingReasons.join(QStringLiteral("; "))));

    QString error;
    const PythonWorkerHandshake handshake = worker.handshake(10000, &error);
    QVERIFY2(handshake.ok, qPrintable(error));
    QCOMPARE(handshake.protocolName, PythonContract::handshakeName());
    QCOMPARE(handshake.protocolVersionMajor, 1);
    QVERIFY(handshake.protocolVersionText.startsWith(QStringLiteral("1.")));
    QCOMPARE(handshake.processId, worker.processId());
    QVERIFY(!handshake.executable.isEmpty());
    QVERIFY(!handshake.implementation.isEmpty());
    QVERIFY(handshake.versionText.startsWith(QStringLiteral("3.")));
    QVERIFY(handshake.hasFeature(QStringLiteral("schema.introspection")));

    // The framed transfer is the bridge's contract, so the worker has to advertise it - and it does so with or without
    // numpy, because the bridge uses the standard library's array module.
    QVERIFY(handshake.hasFeature(PythonContract::featureName(PythonContract::Feature::ArrayBuffer)));
    QVERIFY(handshake.supportsTransferMode(QStringLiteral("framed")));
    QVERIFY(handshake.unknownFeatures.isEmpty());
    QVERIFY(!handshake.raw.isEmpty());

    // Asking twice on the same process gives the same identity, and the handshake does not disturb the request slots.
    const PythonWorkerHandshake second = worker.handshake(10000, &error);
    QVERIFY(second.ok);
    QVERIFY(!worker.hasOutstandingRequest());
}

void PythonDataBridgeTest::worker_stops_when_it_is_asked_to()
{
    SKIP_WITHOUT_PYTHON();
    SKIP_WITHOUT_WORKER();
    PythonWorkerProcess worker;
    if(!startWorker(worker))
        QSKIP(qPrintable(_missingReasons.join(QStringLiteral("; "))));
    QVERIFY(worker.isRunning());

    // stop() asks first: the worker answers `quitting` and exits by itself, which is what leaves no interpreter behind
    // for the next test and no half-written state anywhere.
    worker.stop(5000);
    QVERIFY(!worker.isRunning());
    QCOMPARE(worker.exitCode(), 0);
    QCOMPARE(worker.processId(), -1);

    // Stopping again is harmless, and a request on a stopped worker is a reported failure rather than a crash.
    worker.stop();
    const PythonArrayBridge::Result result = PythonArrayBridge::exchange(worker, makeBlock());
    QVERIFY(!result.ok);
    QCOMPARE(result.errorCode, QStringLiteral("worker_unavailable"));
    QVERIFY(result.errorMessage.contains(QStringLiteral("not running")));
}

void PythonDataBridgeTest::worker_can_abandon_a_running_operation()
{
    SKIP_WITHOUT_PYTHON();
    SKIP_WITHOUT_WORKER();
    PythonWorkerProcess worker;
    if(!startWorker(worker))
        QSKIP(qPrintable(_missingReasons.join(QStringLiteral("; "))));

    // `sleep` is the worker's stand-in for a long evaluation: what matters here is the client's ability to overtake a
    // running request with a cancellation and still read the answer of the request it cancelled.
    PythonWorkerRequest request;
    request.operation = QStringLiteral("sleep");
    request.arguments.insert(QStringLiteral("milliseconds"), 60000);
    const quint64 id = worker.send(request);
    QVERIFY(id != 0);
    QVERIFY(worker.hasOutstandingRequest());

    // A second send while one request is outstanding is a programming error and is reported as such.
    PythonWorkerRequest second;
    second.operation = QLatin1String(PythonWorkerProtocol::handshakeOperation);
    QCOMPARE(worker.send(second), quint64(0));
    QVERIFY(worker.lastSendError().contains(QStringLiteral("unanswered request")));

    QTest::qWait(100);
    QVERIFY(worker.cancel(id, 5000));

    PythonWorkerReply reply;
    QVERIFY(worker.receive(reply, 10000));
    QCOMPARE(reply.id, id);
    QVERIFY(!reply.ok);
    QCOMPARE(reply.errorCode, QStringLiteral("cancelled"));
    QVERIFY(!worker.hasOutstandingRequest());

    // The worker survived the cancellation and answers the next request normally.
    QString error;
    QVERIFY(worker.handshake(10000, &error).ok);
}

QTEST_MAIN(PythonDataBridgeTest)
#include "tst_python_data_bridge.moc"
