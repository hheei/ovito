// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/automation/python/PythonWorkerProcess.h>

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <optional>

namespace Ovito {

/**
 * \brief One numeric array as it travels between OVITO and the worker (Phase 2.6, deliverable 7).
 *
 * The bridge moves bytes and describes them; it never moves a pointer into OVITO's data and never shares memory with the
 * interpreter. The rules that make that safe are the reason this type exists rather than a bare `QByteArray`:
 *
 *  - **A copy is a copy.** The bytes in a \ref PythonArray belong to this object. Sending an array to the worker copies
 *    it into the request, and an array received from the worker arrives in its own buffer, so neither side can change
 *    the other's data and neither buffer can outlive the array it came from. Nothing in this header hands an address of
 *    OVITO's memory to another process.
 *  - **A description that can be checked.** The descriptor carries the name, the type, the shape, the byte count and a
 *    SHA-256 over the bytes. The sender's digest is checked on arrival (\ref verifyChecksum), so a transfer that
 *    reinterpreted the data is a reported failure and not a silent mis-computation.
 *  - **Contiguous by construction.** There is no stride in the descriptor. A caller whose source buffer is strided
 *    copies it into a contiguous array; a stride the wire cannot express is worse than the copy, and a mismatch between
 *    a shape and a byte count is rejected with the offending array named.
 *  - **One representation.** The bytes are the numeric values in the host's native representation. Every platform the
 *    design targets (x86_64, ARM64) is little-endian, and the worker runs on the same machine as OVITO, so no
 *    conversion happens; a big-endian host or a remote worker is not a supported combination of this phase and would
 *    have to fix a byte order here first.
 *
 * Turning an OVITO `PropertyObject` into a \ref PythonArray (and back) is the adapter of the phase that runs a
 * modifier, not of this type: the bridge deliberately knows nothing about OVITO's data objects, which is what makes it
 * testable without a data set (open item O18).
 */
struct OVITO_CORE_EXPORT PythonArray
{
    /// The array's name, which is how a worker refers to it and how the reply names what it derived.
    QString name;

    /// The numeric type in the wire vocabulary, see \ref PythonArrayTypes::names.
    QString dtype;

    /// The dimensions. The product has to match the byte count and the element size.
    QVector<qint64> shape;

    /// The elements, in order, in the host's native representation.
    QByteArray bytes;

    /// The SHA-256 the sender announced. Empty for an array that has not been on the wire and is not to be checked.
    QString announcedChecksum;

    /// The number of elements the shape describes.
    qint64 elementCount() const;

    /// The size of one element in bytes, or 0 when the type is not one this build knows.
    int elementSize() const;

    /// Whether the name, the type, the shape and the byte count agree, with a reason when they do not.
    bool isValid(QString* problem = nullptr) const;

    /// The SHA-256 of \ref bytes, as lowercase hexadecimal.
    QByteArray checksum() const;

    /// Whether the bytes match \ref announcedChecksum (and whether there is a digest to check at all).
    bool verifyChecksum(QString* problem = nullptr) const;

    /// The wire descriptor of this array, including a fresh digest.
    QVariantMap descriptor() const;

    /// The array described by a wire descriptor and its bytes, validated as far as the descriptor allows.
    static std::optional<PythonArray> fromWire(const QVariantMap& descriptor, const QByteArray& bytes, QString* problem = nullptr);

    /// An array of double-precision values, `shape` describing them.
    static std::optional<PythonArray> fromDoubles(const QString& name, const QVector<qint64>& shape, const QVector<double>& values, QString* problem = nullptr);

    /// An array of 32-bit signed integers, `shape` describing them.
    static std::optional<PythonArray> fromIntegers(const QString& name, const QVector<qint64>& shape, const QVector<qint32>& values, QString* problem = nullptr);

    /// Reads the bytes back as double-precision values. Fails for any other type.
    std::optional<QVector<double>> asDoubles(QString* problem = nullptr) const;

    /// Reads the bytes back as integers, widening every integer type this build knows.
    std::optional<QVector<qint64>> asIntegers(QString* problem = nullptr) const;
};

/**
 * \brief The numeric types the bridge can move, which is the vocabulary both sides check against.
 *
 * An unknown type is refused with the offending array named, on both sides: the worker refuses a descriptor whose type
 * it does not know, and this build refuses to describe one. A type that a *newer* worker supports therefore has to be
 * added here (a wire change), which is the point - silently truncating an array whose type a receiver does not
 * understand is how a pipeline produces wrong numbers.
 */
namespace PythonArrayTypes {

/// The type names of the wire vocabulary.
inline QStringList names()
{
    return {
        QStringLiteral("float64"),
        QStringLiteral("float32"),
        QStringLiteral("int64"),
        QStringLiteral("int32"),
        QStringLiteral("uint32"),
        QStringLiteral("int8"),
        QStringLiteral("uint8"),
    };
}

/// The size of one element, or 0 for a name this build does not know.
inline int elementSize(const QString& name)
{
    if(name == QLatin1String("float64") || name == QLatin1String("int64"))
        return 8;
    if(name == QLatin1String("float32") || name == QLatin1String("int32") || name == QLatin1String("uint32"))
        return 4;
    if(name == QLatin1String("int8") || name == QLatin1String("uint8"))
        return 1;
    return 0;
}

/// Whether the name is a type of this vocabulary.
inline bool isKnown(const QString& name) { return elementSize(name) != 0; }

}   // namespace PythonArrayTypes

/**
 * \brief A set of named arrays: what one bridge exchange sends, and what it returns.
 *
 * The block owns the bytes of every array in it, so it can be handed around and outlived its source. The wire form of a
 * block is its descriptors (JSON) plus one payload (the concatenated bytes of the arrays in order) - the framed
 * transfer of audit decision D49, which is why a block is never serialized as JSON.
 */
class OVITO_CORE_EXPORT PythonArrayBlock
{
public:

    PythonArrayBlock() = default;

    /// Whether the block holds no array at all.
    bool isEmpty() const { return _arrays.empty(); }

    /// The number of arrays.
    qsizetype arrayCount() const { return _arrays.size(); }

    /// The arrays, in the order they travel.
    const QVector<PythonArray>& arrays() const { return _arrays; }

    /// The total number of payload bytes.
    qint64 totalBytes() const;

    /// The total number of elements.
    qint64 elementCount() const;

    /// The arrays' names, in order.
    QStringList names() const;

    /// One array by name, or nullptr.
    const PythonArray* find(const QString& name) const;

    /// Adds an array. The array is copied; the block owns its bytes from here on.
    void append(const PythonArray& array);

    /// The payload of this block: the arrays' bytes, concatenated in order.
    QByteArray payload() const;

    /// The descriptors of this block's arrays, each with a fresh digest - the request argument of an exchange.
    QVariantList descriptors() const;

    /**
     * \brief Reads the descriptors of a reply and the payload that followed its header line.
     *
     * Fails, with the reason and the offending array named, when the payload's size does not match what the descriptors
     * need (a truncated or over-long transfer), when a type is unknown, or when two arrays share a name.
     */
    static std::optional<PythonArrayBlock> fromWire(const QVariantList& descriptors, const QByteArray& payload, QString* problem = nullptr);

    /// Whether every array's announced digest matches its bytes.
    bool verifyChecksums(QString* problem = nullptr) const;

private:

    QVector<PythonArray> _arrays;
};

/**
 * \brief The first writable bridge between OVITO and the interpreter: arrays in, arrays out.
 *
 * Phase 2.6 asks for one *writable* data bridge, and this is it, on purpose narrow: it makes the seam's ownership and
 * transfer rules executable and verifiable before a Python modifier depends on them. What it demonstrates, with data
 * the caller can predict:
 *
 *  - arrays go out as framed raw buffers and come back the same way, in both directions with a digest that proves the
 *    bytes were not reinterpreted (the topology spike only measured the worker-to-client direction);
 *  - the caller's block is untouched by an exchange, because it was copied - which is the ownership rule a modifier
 *    needs when the data it sends belongs to a pipeline that may be re-evaluated;
 *  - a result is refused rather than used when the reply does not describe what arrived.
 *
 * What it deliberately does *not* do: it does not run user code. The arrays operation of the worker is a fixed
 * computation (`scale` multiplies the first float64 array by a factor and echoes the remaining arrays byte for byte,
 * `identity` only echoes). Running a decorated user function on those arrays is the Python Function Modifier of
 * Phase 4, on this same client and this same framing; the operation is a spike-grade stand-in that a test can check
 * exactly, and the design's data contract (what a modifier receives and returns) is Phase 4's subject.
 */
class OVITO_CORE_EXPORT PythonArrayBridge
{
public:

    /// What the worker's arrays operation does with the arrays it receives.
    enum class Operation {
        /// Multiply the first float64 array by the factor and echo the rest (the default).
        Scale,
        /// Echo every array unchanged, which is the transfer's own test.
        Identity
    };

    /// The wire name of an operation.
    static QString operationName(Operation operation);

    /// The operation a wire name denotes, if it is one this build knows.
    static std::optional<Operation> operationFromName(const QString& name);

    /// What one exchange produced.
    struct Result
    {
        bool ok = false;

        /// The arrays the worker returned, in the order it returned them.
        PythonArrayBlock arrays;

        /// The error: a worker error code, or one of this client's own (\ref PythonWorkerProtocol::clientErrorCodes).
        QString errorCode;
        QString errorMessage;

        /// The whole reply, for the facts this type does not model (element count, the worker's own timings).
        QVariantMap response;

        /// What the caller experienced. The worker reports how long it computed; only the caller can time the seam.
        double elapsedMs = 0.0;

        /// The digests of what the worker received, by array name - the proof that the outbound transfer was exact.
        QVariantMap receivedInputDigests() const { return response.value(QStringLiteral("inputSha256")).toMap(); }

        QString toString() const;
    };

    /**
     * \brief Sends one block of arrays to the worker and returns the block it produced.
     *
     * The block is validated before anything is written (a name, a known type, a shape that matches the byte count, and
     * the bounds below); the reply is validated before it is returned. Both refusals name the array they are about.
     */
    static Result exchange(PythonWorkerProcess& worker, const PythonArrayBlock& inputs, Operation operation = Operation::Scale,
                           double factor = 1.0, int timeoutMs = 60000);

    /// The largest payload this bridge sends in one request. A larger transfer needs the streaming form of Phase 4.
    static constexpr qint64 maximumPayloadBytes = 512 * 1024 * 1024;

    /// The largest number of arrays in one request, which bounds the request header as well.
    static constexpr int maximumArrays = 64;

    /// The largest factor term, so that a typo cannot ask for a meaningless computation.
    static constexpr double maximumFactor = 1e12;
};

}   // namespace Ovito
