// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include "AutomationSessionDescriptor.h"
#include "AutomationContract.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>

#ifdef Q_OS_UNIX
    #include <cerrno>
    #include <csignal>
#endif

namespace Ovito {

namespace {

/// The largest descriptor this build reads; anything bigger is not one of ours.
constexpr qint64 maximumDescriptorBytes = 64 * 1024;

/// True if a session ID is one this build would have written: no separator, no dot, no escaping.
bool isPlainName(const QString& name)
{
    if(name.isEmpty() || name.size() > 96)
        return false;
    for(QChar character : name) {
        if(!character.isLetterOrNumber() && character != u'-' && character != u'_')
            return false;
    }
    return true;
}

/// True if an endpoint is something a client may connect to: an absolute socket path or, on a platform that has no
/// such thing, a plain name. A relative path is refused because it would resolve differently for the two processes
/// that have to agree on it, and ".." is refused because a descriptor is a file a client reads, not a router.
bool isUsableEndpoint(const QString& endpoint)
{
    if(endpoint.isEmpty() || endpoint.size() > 104 || endpoint.contains(QStringLiteral("..")))
        return false;
    if(endpoint.startsWith(u'/'))
        return true;
    return isPlainName(endpoint);
}

/// Compares two contract versions by major version only, which is what a client needs to decide whether it can use the
/// session at all. A minor difference is compatible by the contract's own rule.
bool contractMajorMatches(const QString& version)
{
    return version.section(u'.', 0, 0) == AutomationContract::version().section(u'.', 0, 0);
}

}   // namespace

/******************************************************************************
* Constructs a descriptor from its parts.
******************************************************************************/
AutomationSessionDescriptor::AutomationSessionDescriptor(QString sessionId, qint64 processId, QString endpoint,
                                                         QDateTime startedAt)
    : _sessionId(std::move(sessionId)), _processId(processId), _endpoint(std::move(endpoint)),
      _startedAt(startedAt.isValid() ? std::move(startedAt) : QDateTime::currentDateTime()),
      _contractVersion(AutomationContract::version())
{
}

/******************************************************************************
* Allocates an ID and an endpoint name for a new session.
******************************************************************************/
AutomationSessionDescriptor AutomationSessionDescriptor::allocateNewSession(QString endpointPrefix)
{
    if(endpointPrefix.isEmpty() || !isPlainName(endpointPrefix))
        endpointPrefix = QStringLiteral("ovito");
    // The suffix has to be unique among the sessions of this user and short: the endpoint name becomes part of a UNIX
    // socket path, which macOS limits to 104 characters.
    const QString suffix = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    const QString name = QStringLiteral("%1-%2-%3").arg(endpointPrefix).arg(QCoreApplication::applicationPid()).arg(suffix);
    // The endpoint is the socket path, not merely a name: a socket created from a relative name is placed in the
    // system's temporary directory, which every user of the machine can write to, while the session directory belongs
    // to its owner alone. The descriptor carries the path, so a client connects to exactly the socket the session made
    // and cannot be redirected by whoever got to a temporary name first.
    return AutomationSessionDescriptor(name, QCoreApplication::applicationPid(), directory() + u'/' + name);
}

/******************************************************************************
* States why a descriptor cannot be used.
******************************************************************************/
QString AutomationSessionDescriptor::validityProblem() const
{
    if(!isPlainName(_sessionId))
        return QStringLiteral("the session ID '%1' is not a plain name").arg(_sessionId);
    if(_processId <= 0)
        return QStringLiteral("the process ID is missing");
    if(!isUsableEndpoint(_endpoint))
        return QStringLiteral("the endpoint '%1' is neither an absolute socket path nor a plain name, or is too long").arg(_endpoint);
    if(!_startedAt.isValid())
        return QStringLiteral("the start time is missing");
    if(_contractVersion.isEmpty())
        return QStringLiteral("the contract version is missing");
    if(!contractMajorMatches(_contractVersion))
        return QStringLiteral("the session speaks contract version %1, this build speaks %2")
            .arg(_contractVersion, AutomationContract::version());
    return {};
}

bool AutomationSessionDescriptor::isValid() const
{
    return validityProblem().isEmpty();
}

/******************************************************************************
* A summary with no path and no user data in it.
******************************************************************************/
QString AutomationSessionDescriptor::displayLabel() const
{
    return QStringLiteral("%1 (process %2, started %3)")
        .arg(_sessionId)
        .arg(_processId)
        .arg(_startedAt.toString(Qt::ISODate));
}

/******************************************************************************
* Serializes the descriptor.
******************************************************************************/
QVariantMap AutomationSessionDescriptor::toJson() const
{
    QVariantMap json;
    json.insert(QStringLiteral("discoveryVersion"), discoveryVersion);
    json.insert(QStringLiteral("sessionId"), _sessionId);
    json.insert(QStringLiteral("processId"), QVariant::fromValue<qlonglong>(_processId));
    json.insert(QStringLiteral("endpoint"), _endpoint);
    json.insert(QStringLiteral("startedAt"), _startedAt.toString(Qt::ISODateWithMs));
    json.insert(QStringLiteral("contractVersion"), _contractVersion.isEmpty() ? AutomationContract::version() : _contractVersion);
    return json;
}

/******************************************************************************
* Parses a descriptor.
******************************************************************************/
std::optional<AutomationSessionDescriptor> AutomationSessionDescriptor::fromJson(const QVariantMap& json, QString* error)
{
    const auto fail = [error](QString message) -> std::optional<AutomationSessionDescriptor> {
        if(error)
            *error = std::move(message);
        return std::nullopt;
    };

    if(json.isEmpty())
        return fail(QStringLiteral("the descriptor is empty"));

    const QVariant versionValue = json.value(QStringLiteral("discoveryVersion"));
    if(versionValue.isValid() && versionValue.toInt() != discoveryVersion)
        return fail(QStringLiteral("the descriptor was written by discovery version %1, this build reads version %2")
                        .arg(versionValue.toInt()).arg(discoveryVersion));

    const QString sessionId = json.value(QStringLiteral("sessionId")).toString();
    if(sessionId.isEmpty())
        return fail(QStringLiteral("the descriptor names no session"));

    bool processIdOk = false;
    const qint64 processId = json.value(QStringLiteral("processId")).toLongLong(&processIdOk);
    if(!processIdOk || processId <= 0)
        return fail(QStringLiteral("the descriptor names no process"));

    const QString endpoint = json.value(QStringLiteral("endpoint")).toString();
    const QDateTime startedAt = QDateTime::fromString(json.value(QStringLiteral("startedAt")).toString(), Qt::ISODateWithMs);

    AutomationSessionDescriptor descriptor(sessionId, processId, endpoint, startedAt);
    // A descriptor from a session that speaks another major protocol is a real session this build cannot talk to, so it
    // is reported as itself rather than as a broken file.
    descriptor._contractVersion = json.value(QStringLiteral("contractVersion"), AutomationContract::version()).toString();

    const QString problem = descriptor.validityProblem();
    if(!problem.isEmpty())
        return fail(problem);
    return descriptor;
}

/******************************************************************************
* The directory descriptors live in.
******************************************************************************/
QString AutomationSessionDescriptor::directory()
{
    // The environment override exists for two callers: a test that must not touch the user's real discovery scope, and a
    // user (or a future remote helper) who wants sessions of a private scope. It is not a security boundary - the
    // directory permissions are what keeps other users out.
    const QByteArray override = qgetenv("OVITO_AUTOMATION_SESSION_DIR");
    if(!override.isEmpty())
        return QString::fromLocal8Bit(override);

    // The runtime directory is the right place for endpoints: it is per user, it is cleared on logout, and it is where a
    // socket of a limited name length fits. Windows has no such concept, hence the fallback.
    QString base = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if(base.isEmpty())
        base = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if(base.isEmpty())
        base = QDir::tempPath();
    return base + QStringLiteral("/ovito/automation");
}

/******************************************************************************
* The complete path of a session's descriptor.
******************************************************************************/
QString AutomationSessionDescriptor::filePath(const QString& sessionId)
{
    return directory() + u'/' + sessionId + QString::fromLatin1(fileSuffix);
}

/******************************************************************************
* Writes the descriptor with owner-only permissions.
******************************************************************************/
bool AutomationSessionDescriptor::writeToDisk(QString* error) const
{
    const auto fail = [error](const QString& message) {
        if(error)
            *error = message;
        return false;
    };

    if(!isValid())
        return fail(validityProblem());

    const QString path = filePath(_sessionId);
    QDir dir(directory());
    if(!dir.exists() && !dir.mkpath(QStringLiteral(".")))
        return fail(QStringLiteral("cannot create the session directory '%1'").arg(dir.absolutePath()));
#if defined(Q_OS_UNIX)
    // Only the owner may list the directory that names this user's sessions.
    QFile::setPermissions(dir.absolutePath(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
#endif

    // Write a sibling file first and rename it over the target: a client that scans the directory while a session
    // starts sees either no descriptor or a complete one, never a half-written file.
    QFile file(path + QStringLiteral(".tmp"));
    if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return fail(QStringLiteral("cannot write '%1': %2").arg(file.fileName(), file.errorString()));
#if defined(Q_OS_UNIX)
    // Nobody else may read what this user is running, even if the file mode of the directory is permissive.
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
#endif
    const QByteArray bytes = QJsonDocument(QJsonObject::fromVariantMap(toJson())).toJson(QJsonDocument::Compact) + '\n';
    if(file.write(bytes) != bytes.size()) {
        file.remove();
        return fail(QStringLiteral("cannot write '%1': %2").arg(file.fileName(), file.errorString()));
    }
    file.close();
    if(!QFile::rename(file.fileName(), path)) {
        file.remove();
        return fail(QStringLiteral("cannot publish '%1'").arg(path));
    }
    return true;
}

/******************************************************************************
* Removes this session's descriptor.
******************************************************************************/
bool AutomationSessionDescriptor::removeFromDisk() const
{
    const QString path = filePath(_sessionId);
    return !QFile::exists(path) || QFile::remove(path);
}

/******************************************************************************
* Scans the session directory.
******************************************************************************/
QVector<AutomationSessionDescriptor> AutomationSessionDescriptor::discover(QStringList* problems, int limit)
{
    QVector<AutomationSessionDescriptor> found;
    const QString path = directory();
    const QFileInfoList entries = QDir(path).entryInfoList({ QStringLiteral("*") + QString::fromLatin1(fileSuffix) },
                                                           QDir::Files | QDir::Readable, QDir::Time);
    for(const QFileInfo& entry : entries) {
        if(limit > 0 && found.size() >= limit)
            break;
        if(entry.size() > maximumDescriptorBytes) {
            if(problems)
                problems->push_back(entry.fileName());
            continue;
        }
        QFile file(entry.absoluteFilePath());
        if(!file.open(QIODevice::ReadOnly)) {
            if(problems)
                problems->push_back(entry.fileName());
            continue;
        }
        const QByteArray bytes = file.read(maximumDescriptorBytes);
        file.close();

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
        QString reason = parseError.error == QJsonParseError::NoError ? QString() : parseError.errorString();
        if(parseError.error == QJsonParseError::NoError && document.isObject()) {
            if(std::optional<AutomationSessionDescriptor> descriptor =
                   fromJson(document.object().toVariantMap(), &reason)) {
                found.push_back(std::move(*descriptor));
                continue;
            }
        }
        else if(parseError.error == QJsonParseError::NoError) {
            reason = QStringLiteral("the descriptor is not a JSON object");
        }
        // A file that is not a descriptor of this build is reported by name - and nothing else about it - so a client
        // can say "these files are not mine" instead of silently ignoring a leftover.
        if(problems)
            problems->push_back(QStringLiteral("%1 (%2)").arg(entry.fileName(), reason));
    }
    // Newest first: the session a user just started is the one a bare "connect" should mean.
    std::stable_sort(found.begin(), found.end(), [](const AutomationSessionDescriptor& a, const AutomationSessionDescriptor& b) {
        return a.startedAt() > b.startedAt();
    });
    return found;
}

/******************************************************************************
* Whether the owning process still exists.
******************************************************************************/
bool AutomationSessionDescriptor::isProcessAlive() const
{
    if(_processId <= 0)
        return false;
#if defined(Q_OS_UNIX)
    // Signal 0 performs the permission and existence checks without delivering anything. A failure with EPERM means the
    // process is there and belongs to someone else, which is still a live session.
    if(::kill(static_cast<pid_t>(_processId), 0) == 0)
        return true;
    return errno == EPERM;
#else
    // No portable existence check here, and the two possible mistakes are not symmetric: reporting a gone session as
    // alive costs a failed connection, while reporting a live one as stale would invite a client to delete its
    // descriptor. So unknown answers "alive" and the connection attempt decides.
    return true;
#endif
}

}   // namespace Ovito
