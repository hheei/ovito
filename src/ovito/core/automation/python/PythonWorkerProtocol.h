// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace Ovito {

/**
 * \brief The wire vocabulary of the Python worker protocol (Phase 2.6, deliverable 7).
 *
 * The worker speaks one JSON object per line over its standard input and output, and a request or a reply may announce
 * `bytes` in its header object, in which case exactly that many raw bytes follow the line - the framed transfer that
 * audit decision D49 selects for arrays. This header holds the names both sides use, so that a client, the worker
 * script and the documentation cannot drift apart, and so that Phase 4 has one place to extend the protocol when the
 * installed package's own entry point replaces the script (open item O16).
 *
 * The envelope has no version number of its own. The handshake names the *package protocol* (see
 * \ref PythonContract::handshakeName and \ref PythonContract::protocolVersion), and its major number is the
 * compatibility gate for this channel as well: a change to the framing that an older client could not tolerate has to
 * move that major number, so that an incompatible pair refuses to work instead of mis-reading each other's bytes.
 *
 * Two vocabularies are deliberately separate:
 *
 *  - **Worker errors** (`unknown_operation`, `invalid_argument`, ...) belong to this protocol: they mean the worker
 *    understood the envelope and refused the work.
 *  - **Client errors** (`worker_unavailable`, `protocol_error`) are produced by \ref PythonWorkerProcess when the
 *    exchange itself failed - the interpreter died, the answer was not JSON, or the announced payload never arrived.
 *    A caller that conflates the two cannot tell "your request was wrong" from "the worker is gone", which is exactly
 *    the distinction a frontend has to show.
 *
 * Note that this is the *runtime* handshake. It carries the same protocol identity as the *package* handshake of
 * \ref PythonHandshake - whose canonical shape PythonHandshake::fromJson defines and which \ref PythonEnvironmentProbe
 * validates before anything runs - but its subject is different: this one describes the running process (its pid, its
 * numpy availability, the transfer modes it offers), while that one describes an installation. Phase 4's entry point
 * answers both, and a client must not use one as a substitute for the other: the probe decides whether an environment
 * may be used at all, this handshake decides what this particular worker process can do.
 */
namespace PythonWorkerProtocol {

/// The operation that asks the worker what it is and what it can do.
inline constexpr auto handshakeOperation = "handshake";

/// The operation that stops the worker.
inline constexpr auto quitOperation = "quit";

/// The operation that asks the worker to abandon one in-flight request.
inline constexpr auto cancelOperation = "cancel";

/// The operation of the data bridge: named numeric arrays in, derived and echoed arrays out.
inline constexpr auto arraysOperation = "arrays";

/// The errors the worker itself answers with.
inline QStringList workerErrorCodes()
{
    return {
        QStringLiteral("unknown_operation"),
        QStringLiteral("invalid_argument"),
        QStringLiteral("unsupported_transfer"),
        QStringLiteral("cancelled"),
        QStringLiteral("internal_error"),
    };
}

/// The errors \ref PythonWorkerProcess produces for a failed exchange, which are disjoint from the worker's own.
inline QStringList clientErrorCodes()
{
    return {
        QStringLiteral("worker_unavailable"),
        QStringLiteral("protocol_error"),
    };
}

}   // namespace PythonWorkerProtocol

/**
 * \brief One request to the Python worker: an operation, its arguments and an optional raw payload.
 *
 * The payload is written verbatim after the header line, and the header announces its size - no array ever travels as
 * JSON or base64 (design section 3.4.1). The protocol fields (`id`, `op`, `bytes`) are written last, so an argument map
 * cannot accidentally redefine them.
 */
struct OVITO_CORE_EXPORT PythonWorkerRequest
{
    /// The worker operation, e.g. \ref PythonWorkerProtocol::arraysOperation.
    QString operation;

    /// The operation's own arguments. Scalars only: anything large belongs in \ref payload.
    QVariantMap arguments;

    /// The framed payload that follows the header line, if any.
    QByteArray payload;

    /// The request id the answer is correlated by. Set by \ref PythonWorkerProcess::exchange.
    quint64 id = 0;

    /// The header line of this request.
    QVariantMap toJson() const
    {
        QVariantMap json = arguments;
        json.insert(QStringLiteral("id"), QVariant::fromValue(id));
        json.insert(QStringLiteral("op"), operation);
        if(!payload.isEmpty())
            json.insert(QStringLiteral("bytes"), payload.size());
        return json;
    }
};

/**
 * \brief One answer of the Python worker.
 *
 * `transportOk` is false when the exchange itself failed and `errorCode` then names a client error; when it is true,
 * `ok` is the worker's own verdict and `errorCode` (if any) names a worker error. `payload` holds the framed bytes the
 * answer announced, read in full before the reply is returned.
 */
struct OVITO_CORE_EXPORT PythonWorkerReply
{
    bool transportOk = true;
    bool ok = false;
    quint64 id = 0;
    QVariantMap json;
    QByteArray payload;
    QString errorCode;
    QString errorMessage;
    QVariantMap errorDetails;

    /// What the caller experienced, which is what a latency number has to mean.
    double elapsedMs = 0.0;

    /// True when the worker answered without an error.
    bool succeeded() const { return transportOk && ok && errorCode.isEmpty(); }
};

/**
 * \brief What a worker process says about itself, as its runtime handshake reports it.
 *
 * The features and transfer modes are what the worker *offers*, not what the client may use: a caller asks for what it
 * needs (for instance `array.buffer` before moving arrays) and reports the mismatch rather than falling back silently,
 * exactly as the package handshake is treated.
 */
struct OVITO_CORE_EXPORT PythonWorkerHandshake
{
    bool ok = false;
    QString protocolName;
    QString protocolVersionText;
    int protocolVersionMajor = 0;
    int protocolVersionMinor = 0;
    QString implementation;
    QString versionText;
    QString executable;
    qint64 processId = -1;

    /// The numpy version the worker found, or empty. Its absence is a fact about the environment, not a failure.
    QString numpyVersion;

    /// The features of the contract vocabulary this worker implements.
    QStringList features;

    /// Features the worker advertises that this build does not know. Kept the way \ref PythonHandshake keeps them: a
    /// newer worker may offer more, and a client reports what it did not understand instead of ignoring it.
    QStringList unknownFeatures;

    /// The transfer modes this worker offers (`text`, `base64`, `framed`, `memoryview`, `shared-memory`, ...).
    QStringList transferModes;

    QVariantMap raw;

    /// Whether the worker advertises one feature from the contract vocabulary.
    bool hasFeature(const QString& feature) const { return features.contains(feature); }

    /// Whether the worker advertises one transfer mode.
    bool supportsTransferMode(const QString& mode) const { return transferModes.contains(mode); }
};

}   // namespace Ovito
