// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/automation/python/PythonDataBridge.h>

#include <QCryptographicHash>

#include <cmath>
#include <cstring>

namespace Ovito {

namespace {

/// Reads the numbers of one array out of its bytes, for the types whose bytes this build can interpret.
template<typename T>
std::optional<QVector<T>> decode(const QByteArray& bytes, bool* ok)
{
    if(bytes.size() % qsizetype(sizeof(T)) != 0) {
        *ok = false;
        return {};
    }
    const qsizetype count = bytes.size() / sizeof(T);
    QVector<T> values(count);
    if(count > 0)
        std::memcpy(values.data(), bytes.constData(), static_cast<size_t>(bytes.size()));
    *ok = true;
    return values;
}

/// The bytes of a vector of numbers, copied into an owned buffer.
template<typename T>
QByteArray encode(const QVector<T>& values)
{
    QByteArray bytes(sizeof(T) * values.size(), Qt::Uninitialized);
    if(!values.isEmpty())
        std::memcpy(bytes.data(), values.constData(), sizeof(T) * static_cast<size_t>(values.size()));
    return bytes;
}

}   // namespace

/******************************************************************************
* PythonArray.
******************************************************************************/
qint64 PythonArray::elementCount() const
{
    qint64 count = 1;
    for(qint64 dimension : shape)
        count *= dimension;
    return shape.isEmpty() ? 0 : count;
}

int PythonArray::elementSize() const
{
    return PythonArrayTypes::elementSize(dtype);
}

bool PythonArray::isValid(QString* problem) const
{
    if(name.isEmpty()) {
        if(problem)
            *problem = QStringLiteral("an array was described without a name");
        return false;
    }
    if(!PythonArrayTypes::isKnown(dtype)) {
        if(problem)
            *problem = QStringLiteral("the array \"%1\" has the unsupported type \"%2\"; this build knows %3")
                           .arg(name, dtype, PythonArrayTypes::names().join(QStringLiteral(", ")));
        return false;
    }
    if(shape.isEmpty()) {
        if(problem)
            *problem = QStringLiteral("the array \"%1\" was described without a shape").arg(name);
        return false;
    }
    for(qint64 dimension : shape) {
        if(dimension < 0) {
            if(problem)
                *problem = QStringLiteral("the array \"%1\" has a negative dimension").arg(name);
            return false;
        }
    }
    const qint64 expected = elementCount() * elementSize();
    if(bytes.size() != expected) {
        if(problem)
            *problem = QStringLiteral("the array \"%1\" holds %2 bytes but its shape needs %3 bytes of %4")
                           .arg(name)
                           .arg(bytes.size())
                           .arg(expected)
                           .arg(dtype);
        return false;
    }
    return true;
}

QByteArray PythonArray::checksum() const
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
}

bool PythonArray::verifyChecksum(QString* problem) const
{
    if(announcedChecksum.isEmpty()) {
        if(problem)
            *problem = QStringLiteral("the array \"%1\" announced no digest, so its bytes could not be verified").arg(name);
        return false;
    }
    const QByteArray actual = checksum();
    if(actual != announcedChecksum.toLatin1()) {
        if(problem)
            *problem = QStringLiteral("the array \"%1\" arrived changed: %2 was announced, %3 arrived")
                           .arg(name, announcedChecksum, QString::fromLatin1(actual));
        return false;
    }
    return true;
}

QVariantMap PythonArray::descriptor() const
{
    QVariantMap json;
    json.insert(QStringLiteral("name"), name);
    json.insert(QStringLiteral("dtype"), dtype);
    QVariantList shapeList;
    for(qint64 dimension : shape)
        shapeList.push_back(dimension);
    json.insert(QStringLiteral("shape"), shapeList);
    json.insert(QStringLiteral("bytes"), bytes.size());
    // A fresh digest every time the descriptor is written: a descriptor that carried a stale one would make the worker
    // refuse an array that is perfectly fine, and a caller cannot be asked to remember to refresh it.
    json.insert(QStringLiteral("sha256"), QString::fromLatin1(checksum()));
    return json;
}

std::optional<PythonArray> PythonArray::fromWire(const QVariantMap& descriptor, const QByteArray& bytes, QString* problem)
{
    PythonArray array;
    array.name = descriptor.value(QStringLiteral("name")).toString();
    array.dtype = descriptor.value(QStringLiteral("dtype")).toString();
    for(const QVariant& dimension : descriptor.value(QStringLiteral("shape")).toList())
        array.shape.push_back(dimension.toLongLong());
    array.bytes = bytes;
    array.announcedChecksum = descriptor.value(QStringLiteral("sha256")).toString();

    const qint64 declared = descriptor.value(QStringLiteral("bytes")).toLongLong();
    if(declared != bytes.size()) {
        if(problem)
            *problem = QStringLiteral("the array \"%1\" announced %2 bytes and %3 arrived").arg(array.name).arg(declared).arg(bytes.size());
        return {};
    }
    if(!array.isValid(problem))
        return {};
    return array;
}

std::optional<PythonArray> PythonArray::fromDoubles(const QString& name, const QVector<qint64>& shape, const QVector<double>& values, QString* problem)
{
    PythonArray array;
    array.name = name;
    array.dtype = QStringLiteral("float64");
    array.shape = shape;
    const qint64 count = array.elementCount();
    if(count != values.size()) {
        if(problem)
            *problem = QStringLiteral("the shape of \"%1\" describes %2 values but %3 were given").arg(name).arg(count).arg(values.size());
        return {};
    }
    array.bytes = encode(values);
    return array;
}

std::optional<PythonArray> PythonArray::fromIntegers(const QString& name, const QVector<qint64>& shape, const QVector<qint32>& values, QString* problem)
{
    PythonArray array;
    array.name = name;
    array.dtype = QStringLiteral("int32");
    array.shape = shape;
    const qint64 count = array.elementCount();
    if(count != values.size()) {
        if(problem)
            *problem = QStringLiteral("the shape of \"%1\" describes %2 values but %3 were given").arg(name).arg(count).arg(values.size());
        return {};
    }
    array.bytes = encode(values);
    return array;
}

std::optional<QVector<double>> PythonArray::asDoubles(QString* problem) const
{
    if(dtype != QLatin1String("float64")) {
        if(problem)
            *problem = QStringLiteral("the array \"%1\" is of type %2, not float64").arg(name, dtype);
        return {};
    }
    bool ok = false;
    auto values = decode<double>(bytes, &ok);
    if(!ok && problem)
        *problem = QStringLiteral("the bytes of \"%1\" are not double-precision values").arg(name);
    return values;
}

std::optional<QVector<qint64>> PythonArray::asIntegers(QString* problem) const
{
    bool ok = false;
    QVector<qint64> values;
    if(dtype == QLatin1String("int32")) {
        if(auto decoded = decode<qint32>(bytes, &ok))
            for(qint32 value : *decoded)
                values.push_back(value);
    }
    else if(dtype == QLatin1String("uint32")) {
        if(auto decoded = decode<quint32>(bytes, &ok))
            for(quint32 value : *decoded)
                values.push_back(static_cast<qint64>(value));
    }
    else if(dtype == QLatin1String("int64")) {
        if(auto decoded = decode<qint64>(bytes, &ok))
            values = *decoded;
    }
    else if(dtype == QLatin1String("int8")) {
        if(auto decoded = decode<qint8>(bytes, &ok))
            for(qint8 value : *decoded)
                values.push_back(value);
    }
    else if(dtype == QLatin1String("uint8")) {
        if(auto decoded = decode<quint8>(bytes, &ok))
            for(quint8 value : *decoded)
                values.push_back(value);
    }
    else {
        if(problem)
            *problem = QStringLiteral("the array \"%1\" is of type %2, which is not an integer type").arg(name, dtype);
        return {};
    }
    if(!ok) {
        if(problem)
            *problem = QStringLiteral("the bytes of \"%1\" are not whole %2 values").arg(name, dtype);
        return {};
    }
    return values;
}

/******************************************************************************
* PythonArrayBlock.
******************************************************************************/
qint64 PythonArrayBlock::totalBytes() const
{
    qint64 total = 0;
    for(const PythonArray& array : _arrays)
        total += array.bytes.size();
    return total;
}

qint64 PythonArrayBlock::elementCount() const
{
    qint64 total = 0;
    for(const PythonArray& array : _arrays)
        total += array.elementCount();
    return total;
}

QStringList PythonArrayBlock::names() const
{
    QStringList names;
    for(const PythonArray& array : _arrays)
        names << array.name;
    return names;
}

const PythonArray* PythonArrayBlock::find(const QString& name) const
{
    for(const PythonArray& array : _arrays)
        if(array.name == name)
            return &array;
    return nullptr;
}

void PythonArrayBlock::append(const PythonArray& array)
{
    _arrays.push_back(array);
}

QByteArray PythonArrayBlock::payload() const
{
    QByteArray payload;
    payload.reserve(totalBytes());
    for(const PythonArray& array : _arrays)
        payload += array.bytes;
    return payload;
}

QVariantList PythonArrayBlock::descriptors() const
{
    QVariantList list;
    for(const PythonArray& array : _arrays)
        list.push_back(array.descriptor());
    return list;
}

std::optional<PythonArrayBlock> PythonArrayBlock::fromWire(const QVariantList& descriptors, const QByteArray& payload, QString* problem)
{
    PythonArrayBlock block;
    qint64 offset = 0;
    for(const QVariant& entry : descriptors) {
        const QVariantMap descriptor = entry.toMap();
        const qint64 declared = descriptor.value(QStringLiteral("bytes")).toLongLong();
        if(declared < 0 || offset + declared > payload.size()) {
            if(problem)
                *problem = QStringLiteral("the payload is too short for the array \"%1\": %2 bytes were announced after offset %3, but only %4 arrived")
                               .arg(descriptor.value(QStringLiteral("name")).toString())
                               .arg(declared)
                               .arg(offset)
                               .arg(payload.size() - offset);
            return {};
        }
        std::optional<PythonArray> array = PythonArray::fromWire(descriptor, payload.mid(offset, declared), problem);
        if(!array.has_value())
            return {};
        if(block.find(array->name) != nullptr) {
            if(problem)
                *problem = QStringLiteral("the reply describes two arrays named \"%1\"").arg(array->name);
            return {};
        }
        block.append(*array);
        offset += declared;
    }
    if(offset != payload.size()) {
        // Trailing bytes mean the descriptors and the payload disagree, which is a protocol failure and not something
        // to tolerate: the next request would read them as a header line.
        if(problem)
            *problem = QStringLiteral("the reply announced %1 payload bytes but the descriptors describe %2")
                           .arg(payload.size())
                           .arg(offset);
        return {};
    }
    return block;
}

bool PythonArrayBlock::verifyChecksums(QString* problem) const
{
    for(const PythonArray& array : _arrays) {
        if(!array.verifyChecksum(problem))
            return false;
    }
    return true;
}

/******************************************************************************
* PythonArrayBridge.
******************************************************************************/
QString PythonArrayBridge::operationName(Operation operation)
{
    switch(operation) {
    case Operation::Scale: return QStringLiteral("scale");
    case Operation::Identity: return QStringLiteral("identity");
    }
    return {};
}

std::optional<PythonArrayBridge::Operation> PythonArrayBridge::operationFromName(const QString& name)
{
    if(name == QLatin1String("scale"))
        return Operation::Scale;
    if(name == QLatin1String("identity"))
        return Operation::Identity;
    return {};
}

PythonArrayBridge::Result PythonArrayBridge::exchange(PythonWorkerProcess& worker, const PythonArrayBlock& inputs, Operation operation,
                                                     double factor, int timeoutMs)
{
    Result result;

    // Everything this side can check before a single byte is written. A refusal here is a caller defect, and it names
    // the array it is about, because "invalid argument" alone tells a user nothing about which of their arrays is wrong.
    if(inputs.isEmpty()) {
        result.errorCode = QStringLiteral("invalid_block");
        result.errorMessage = QStringLiteral("there is nothing to send: a bridge request needs at least one array");
        return result;
    }
    if(inputs.arrayCount() > maximumArrays) {
        result.errorCode = QStringLiteral("invalid_block");
        result.errorMessage = QStringLiteral("%1 arrays were given; one request moves at most %2").arg(inputs.arrayCount()).arg(maximumArrays);
        return result;
    }
    if(inputs.totalBytes() > maximumPayloadBytes) {
        result.errorCode = QStringLiteral("invalid_block");
        result.errorMessage = QStringLiteral("%1 payload bytes were given; one request moves at most %2")
                                  .arg(inputs.totalBytes())
                                  .arg(maximumPayloadBytes);
        return result;
    }
    for(const PythonArray& array : inputs.arrays()) {
        QString problem;
        if(!array.isValid(&problem)) {
            result.errorCode = QStringLiteral("invalid_block");
            result.errorMessage = problem;
            return result;
        }
    }
    if(std::abs(factor) > maximumFactor || std::isnan(factor)) {
        result.errorCode = QStringLiteral("invalid_block");
        result.errorMessage = QStringLiteral("the factor %1 is out of range").arg(factor);
        return result;
    }

    PythonWorkerRequest request;
    request.operation = QLatin1String(PythonWorkerProtocol::arraysOperation);
    request.arguments.insert(QStringLiteral("operation"), operationName(operation));
    if(operation == Operation::Scale) {
        request.arguments.insert(QStringLiteral("factor"), factor);
        // The worker scales the first array and needs it to be float64; saying so here gives the caller a message about
        // its own array instead of one that travelled there and back.
        if(inputs.arrays().first().dtype != QLatin1String("float64")) {
            result.errorCode = QStringLiteral("invalid_block");
            result.errorMessage = QStringLiteral("the array \"%1\" is scaled, so it has to be float64, not %2")
                                      .arg(inputs.arrays().first().name, inputs.arrays().first().dtype);
            return result;
        }
    }
    request.arguments.insert(QStringLiteral("arrays"), inputs.descriptors());
    request.payload = inputs.payload();

    const PythonWorkerReply reply = worker.exchange(request, timeoutMs);
    result.elapsedMs = reply.elapsedMs;
    result.response = reply.json;

    if(!reply.transportOk) {
        result.errorCode = reply.errorCode;
        result.errorMessage = reply.errorMessage;
        return result;
    }
    if(!reply.ok) {
        // The worker refused the request. Its own vocabulary is kept, so a caller can tell a type it does not support
        // from a worker that died.
        result.errorCode = reply.errorCode.isEmpty() ? QStringLiteral("invalid_argument") : reply.errorCode;
        result.errorMessage = reply.errorMessage;
        if(result.errorMessage.isEmpty())
            result.errorMessage = QStringLiteral("the worker refused the request without saying why");
        return result;
    }

    // The reply is validated before it is used: descriptors and payload have to agree exactly, and the announced digests
    // have to match the bytes that arrived. A mismatch is a failure, not an array to be computed with.
    QString problem;
    std::optional<PythonArrayBlock> returned = PythonArrayBlock::fromWire(reply.json.value(QStringLiteral("arrays")).toList(), reply.payload, &problem);
    if(!returned.has_value()) {
        result.errorCode = QStringLiteral("invalid_reply");
        result.errorMessage = problem;
        return result;
    }
    if(!returned->verifyChecksums(&problem)) {
        result.errorCode = QStringLiteral("invalid_reply");
        result.errorMessage = problem;
        return result;
    }
    // The reply announces how many bytes follow its header; the client read exactly those, so a disagreement here means
    // the worker's descriptors and its payload disagree.
    const qint64 announced = reply.json.value(QStringLiteral("bytes")).toLongLong();
    if(announced != returned->totalBytes()) {
        result.errorCode = QStringLiteral("invalid_reply");
        result.errorMessage = QStringLiteral("the reply announced %1 payload bytes but described %2").arg(announced).arg(returned->totalBytes());
        return result;
    }

    result.arrays = *returned;
    result.ok = true;
    return result;
}

}   // namespace Ovito
