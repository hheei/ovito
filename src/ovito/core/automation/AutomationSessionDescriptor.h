// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <QDateTime>
#include <QString>
#include <QVariantMap>

namespace Ovito {

/**
 * \brief The on-disk description of one running workbench that an external client can connect to.
 *
 * A client that wants to talk to a live session has to find it first, and the only thing it can be sure of is that both
 * processes share a filesystem: the running workbench creates this descriptor in a per-user directory, and a client
 * scans that directory. The descriptor holds everything needed to *address* a session - its ID, the process that owns it,
 * the name of its local endpoint and when it started - and deliberately nothing that describes the user's data.
 *
 * Discovery is a filesystem convention rather than a service. It comes with the two facts that follow from it:
 *
 * - **A descriptor can outlive its session.** A workbench that is killed cannot clean up, so a client decides staleness
 *   by asking whether the owning process still exists (isStale()) *and* by what happens when it tries to connect. The
 *   filesystem convention is not a liveness guarantee, and this class does not pretend to be one.
 * - **The directory is shared, the writes are not.** Descriptors are written atomically enough for a reader to see either
 *   the complete file or none of it, are readable only by their owner, and carry the protocol version they were written
 *   by, so that a client of a newer protocol can say so instead of misreading the file.
 *
 * This is deliberately the *value* half of the local protocol: it contains no socket, no Qt event loop and no session
 * object, which is why it can be tested - and later reused by a production transport - without a running workbench. The
 * endpoint itself is the Phase 2.6 spike's business (see AUTOMATION_IPC_SPIKE.md), and the production endpoint is
 * Phase 3 work.
 */
class OVITO_CORE_EXPORT AutomationSessionDescriptor
{
public:

    /// The version of the discovery convention below, which is independent of the automation contract version.
    static constexpr int discoveryVersion = 1;

    /// The file suffix of a descriptor; only files with it are considered.
    static constexpr const char* fileSuffix = ".session.json";

    /// The number of descriptors discover() returns when nothing else is asked for.
    static constexpr int defaultDiscoveryLimit = 32;

    AutomationSessionDescriptor() = default;
    AutomationSessionDescriptor(QString sessionId, qint64 processId, QString endpoint, QDateTime startedAt = {});

    /// Allocates an ID and an endpoint name for a session of the calling process.
    ///
    /// The ID is unique per process and per start. The endpoint is the complete socket path inside directory(), which
    /// is short enough for the name-length limits of the local socket implementations (macOS allows 104 characters for
    /// a UNIX socket path) and lives in a directory only its owner can enter.
    static AutomationSessionDescriptor allocateNewSession(QString endpointPrefix = QStringLiteral("ovito"));

    /// A descriptor is usable when it names a session, its process, its endpoint and its start time, and when the
    /// automation contract it was written for is one this build understands.
    bool isValid() const;
    QString validityProblem() const;

    const QString& sessionId() const { return _sessionId; }
    qint64 processId() const { return _processId; }
    const QString& endpoint() const { return _endpoint; }
    const QDateTime& startedAt() const { return _startedAt; }

    /// The automation contract version this build writes, and read back from the descriptor.
    const QString& contractVersion() const { return _contractVersion; }

    /// A short human-readable summary for a session list, with no path and no user data in it.
    QString displayLabel() const;

    QVariantMap toJson() const;

    /**
     * \brief Parses a descriptor, returning nothing when the map is not one.
     * \param error Optional output for why it was rejected.
     */
    static std::optional<AutomationSessionDescriptor> fromJson(const QVariantMap& json, QString* error = nullptr);

    /// The directory descriptors live in: a per-user runtime directory, overridable with
    /// OVITO_AUTOMATION_SESSION_DIR for tests and for a caller that wants a private discovery scope.
    static QString directory();

    /// The complete path of the descriptor of a session.
    static QString filePath(const QString& sessionId);

    /// Writes the descriptor into directory() with owner-only permissions, creating the directory if needed. The write
    /// is a file rename, so a concurrent reader never sees a half-written descriptor.
    bool writeToDisk(QString* error = nullptr) const;

    /// Removes this session's descriptor. Removing one that is not there is not an error.
    bool removeFromDisk() const;

    /**
     * \brief The descriptors found in directory(), newest session first.
     * \param problems Optional output listing files that are not usable descriptors, by name.
     * \param limit The maximum number of descriptors to return; the scan stops after it.
     */
    static QVector<AutomationSessionDescriptor> discover(QStringList* problems = nullptr,
                                                         int limit = defaultDiscoveryLimit);

    /// Whether the process that wrote the descriptor still exists. Only implemented for POSIX; other platforms report
    /// true, because a wrong "alive" costs a failed connection while a wrong "stale" could delete a live session.
    bool isProcessAlive() const;

    /// True when the owning process is known to be gone, i.e. when the descriptor is left over.
    bool isStale() const { return !isProcessAlive(); }

private:

    QString _sessionId;
    qint64 _processId = 0;
    QString _endpoint;
    QDateTime _startedAt;
    QString _contractVersion;
};

}   // namespace Ovito
