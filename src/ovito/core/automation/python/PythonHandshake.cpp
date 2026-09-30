// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/automation/python/PythonHandshake.h>

#include <QJsonValue>

namespace Ovito {

namespace {

/// Reads a string member, returning an empty string for a missing, null or non-string value.
QString stringMember(const QVariantMap& json, const QString& key)
{
    const QVariant value = json.value(key);
    return value.canConvert<QString>() && value.isValid() && !value.isNull() ? value.toString() : QString();
}

/// Reads a nested object, tolerating a missing key.
QVariantMap mapMember(const QVariantMap& json, const QString& key)
{
    return json.value(key).toMap();
}

/**
 * Parses "major.minor[.micro]" as written in a version string. Returns false for anything else, including a missing
 * value; the caller turns that into a structural error, because a handshake without a readable protocol version
 * cannot be validated at all.
 */
bool parseVersion(const QString& text, int* major, int* minor, int* micro = nullptr)
{
    const QStringList parts = text.split(u'.');
    if(parts.size() < 2 || parts.size() > 3)
        return false;
    bool ok = false;
    const int parsedMajor = parts.at(0).toInt(&ok);
    if(!ok)
        return false;
    const int parsedMinor = parts.at(1).toInt(&ok);
    if(!ok)
        return false;
    int parsedMicro = 0;
    if(parts.size() == 3) {
        parsedMicro = parts.at(2).toInt(&ok);
        if(!ok)
            return false;
    }
    *major = parsedMajor;
    *minor = parsedMinor;
    if(micro)
        *micro = parsedMicro;
    return true;
}

}   // namespace

/******************************************************************************
* Parses the description of the interpreter.
******************************************************************************/
std::optional<PythonEnvironmentInfo> PythonEnvironmentInfo::fromJson(const QVariantMap& json, QString* errorMessage)
{
    PythonEnvironmentInfo info;
    info._implementation = stringMember(json, QStringLiteral("implementation"));
    info._version = stringMember(json, QStringLiteral("version"));
    info._executable = stringMember(json, QStringLiteral("executable"));
    info._prefix = stringMember(json, QStringLiteral("prefix"));
    info._basePrefix = stringMember(json, QStringLiteral("basePrefix"));
    info._platform = stringMember(json, QStringLiteral("platform"));
    info._machine = stringMember(json, QStringLiteral("machine"));
    info._architecture = stringMember(json, QStringLiteral("architecture"));
    info._cacheTag = stringMember(json, QStringLiteral("cacheTag"));
    info._freeThreaded = json.value(QStringLiteral("freeThreaded")).toBool();
    info._isolated = json.value(QStringLiteral("isolated")).toBool();

    // The three-digit form is what the validation uses; a description that only carries the string form is read
    // anyway, because the envelope check reports a bad version more usefully than a structural error would.
    if(json.contains(QStringLiteral("versionMajor"))) {
        info._versionMajor = json.value(QStringLiteral("versionMajor")).toInt();
        info._versionMinor = json.value(QStringLiteral("versionMinor")).toInt();
        info._versionMicro = json.value(QStringLiteral("versionMicro")).toInt();
    }
    else if(!parseVersion(info._version, &info._versionMajor, &info._versionMinor, &info._versionMicro)) {
        if(errorMessage)
            *errorMessage = QStringLiteral("The interpreter reported the version \"%1\", which is not a version number.").arg(info._version);
        return std::nullopt;
    }

    if(errorMessage)
        errorMessage->clear();
    return info;
}

bool PythonEnvironmentInfo::isValid() const
{
    return !_implementation.isEmpty() && !_executable.isEmpty() && !_platform.isEmpty() && _versionMajor > 0;
}

QString PythonEnvironmentInfo::displayName() const
{
    if(_implementation.isEmpty())
        return _version;
    // sys.implementation.name is lowercase ("cpython", "pypy"); only the one spelling this build supports is branded,
    // because that is the only one whose name is fixed. Everything else is reported as the interpreter names itself.
    const QString name = _implementation.compare(QStringLiteral("cpython"), Qt::CaseInsensitive) == 0
                             ? QStringLiteral("CPython")
                             : _implementation;
    return QStringLiteral("%1 %2").arg(name, _version);
}

QVariantMap PythonEnvironmentInfo::toJson() const
{
    QVariantMap json;
    json.insert(QStringLiteral("implementation"), _implementation);
    json.insert(QStringLiteral("version"), _version);
    json.insert(QStringLiteral("versionMajor"), _versionMajor);
    json.insert(QStringLiteral("versionMinor"), _versionMinor);
    json.insert(QStringLiteral("versionMicro"), _versionMicro);
    json.insert(QStringLiteral("executable"), _executable);
    json.insert(QStringLiteral("prefix"), _prefix);
    json.insert(QStringLiteral("basePrefix"), _basePrefix);
    json.insert(QStringLiteral("platform"), _platform);
    json.insert(QStringLiteral("machine"), _machine);
    json.insert(QStringLiteral("architecture"), _architecture);
    json.insert(QStringLiteral("cacheTag"), _cacheTag);
    json.insert(QStringLiteral("freeThreaded"), _freeThreaded);
    json.insert(QStringLiteral("isolated"), _isolated);
    return json;
}

/******************************************************************************
* Parses the description of the package.
******************************************************************************/
std::optional<PythonPackageInfo> PythonPackageInfo::fromJson(const QVariantMap& json, QString* errorMessage)
{
    PythonPackageInfo package;
    package._found = json.value(QStringLiteral("found")).toBool();
    package._imported = json.value(QStringLiteral("imported")).toBool();
    package._name = stringMember(json, QStringLiteral("name"));
    package._moduleFile = stringMember(json, QStringLiteral("moduleFile"));
    package._version = stringMember(json, QStringLiteral("version"));
    package._protocolVersion = stringMember(json, QStringLiteral("protocolVersion"));
    package._bridge = mapMember(json, QStringLiteral("bridge"));
    package._error = stringMember(json, QStringLiteral("error"));

    // A feature name this build does not know is kept instead of dropped: an environment that speaks a newer minor
    // protocol explains itself that way, and the report of a failed probe should quote it verbatim.
    const QVariantList features = json.value(QStringLiteral("features")).toList();
    for(const QVariant& value : features) {
        const QString name = value.toString();
        if(const std::optional<PythonContract::Feature> feature = PythonContract::featureFromName(name)) {
            if(!package._features.contains(*feature))
                package._features.push_back(*feature);
        }
        else if(!name.isEmpty() && !package._unknownFeatures.contains(name)) {
            package._unknownFeatures.push_back(name);
        }
    }

    if(errorMessage)
        errorMessage->clear();
    return package;
}

QVariantMap PythonPackageInfo::toJson() const
{
    QVariantMap json;
    json.insert(QStringLiteral("name"), _name);
    json.insert(QStringLiteral("found"), _found);
    json.insert(QStringLiteral("imported"), _imported);
    json.insert(QStringLiteral("moduleFile"), _moduleFile.isEmpty() ? QVariant() : QVariant(_moduleFile));
    json.insert(QStringLiteral("version"), _version.isEmpty() ? QVariant() : QVariant(_version));
    json.insert(QStringLiteral("protocolVersion"), _protocolVersion.isEmpty() ? QVariant() : QVariant(_protocolVersion));
    json.insert(QStringLiteral("features"), PythonContract::featureNames(_features));
    if(!_unknownFeatures.isEmpty())
        json.insert(QStringLiteral("unknownFeatures"), _unknownFeatures);
    if(!_bridge.isEmpty())
        json.insert(QStringLiteral("bridge"), _bridge);
    json.insert(QStringLiteral("error"), _error.isEmpty() ? QVariant() : QVariant(_error));
    return json;
}

/******************************************************************************
* Parses a handshake message.
******************************************************************************/
std::optional<PythonHandshake> PythonHandshake::fromJson(const QVariantMap& json, QString* errorMessage)
{
    PythonHandshake handshake;
    handshake._protocolName = stringMember(json, QStringLiteral("handshake"));
    if(handshake._protocolName.isEmpty()) {
        if(errorMessage)
            *errorMessage = QStringLiteral("The answer does not name a handshake protocol.");
        return std::nullopt;
    }
    if(handshake._protocolName != PythonContract::handshakeName()) {
        if(errorMessage)
            *errorMessage = QStringLiteral("The answer belongs to the protocol \"%1\" instead of \"%2\".")
                                .arg(handshake._protocolName, PythonContract::handshakeName());
        return std::nullopt;
    }

    handshake._protocolVersion = stringMember(json, QStringLiteral("protocolVersion"));
    if(!parseVersion(handshake._protocolVersion, &handshake._protocolVersionMajor, &handshake._protocolVersionMinor)) {
        if(errorMessage)
            *errorMessage = QStringLiteral("The environment speaks the protocol version \"%1\", which is not a version number.")
                                .arg(handshake._protocolVersion);
        return std::nullopt;
    }

    if(!json.contains(QStringLiteral("python"))) {
        if(errorMessage)
            *errorMessage = QStringLiteral("The answer does not describe the interpreter.");
        return std::nullopt;
    }
    const std::optional<PythonEnvironmentInfo> environment =
        PythonEnvironmentInfo::fromJson(mapMember(json, QStringLiteral("python")), errorMessage);
    if(!environment)
        return std::nullopt;
    handshake._environment = *environment;
    if(!handshake._environment.isValid()) {
        if(errorMessage)
            *errorMessage = QStringLiteral("The interpreter description is incomplete.");
        return std::nullopt;
    }

    // A missing package object means "no package"; the probe script always sends one, but a handshake from a caller
    // that only describes an interpreter stays readable.
    if(json.contains(QStringLiteral("package"))) {
        const std::optional<PythonPackageInfo> package =
            PythonPackageInfo::fromJson(mapMember(json, QStringLiteral("package")), errorMessage);
        if(!package)
            return std::nullopt;
        handshake._package = *package;
    }
    else {
        handshake._package.setName(PythonContract::packageName());
    }

    const QVariantList features = json.value(QStringLiteral("features")).toList();
    for(const QVariant& value : features) {
        const QString name = value.toString();
        if(const std::optional<PythonContract::Feature> feature = PythonContract::featureFromName(name)) {
            if(!handshake._features.contains(*feature))
                handshake._features.push_back(*feature);
        }
        else if(!name.isEmpty() && !handshake._unknownFeatures.contains(name)) {
            handshake._unknownFeatures.push_back(name);
        }
    }

    if(errorMessage)
        errorMessage->clear();
    return handshake;
}

PythonHandshake& PythonHandshake::setProtocolVersion(QString version)
{
    _protocolVersion = std::move(version);
    if(!parseVersion(_protocolVersion, &_protocolVersionMajor, &_protocolVersionMinor))
        _protocolVersionMajor = _protocolVersionMinor = 0;
    return *this;
}

bool PythonHandshake::isValid() const
{
    return _protocolName == PythonContract::handshakeName() && _protocolVersionMajor > 0 && _environment.isValid();
}

QStringList PythonHandshake::featureNames() const
{
    QStringList names = PythonContract::featureNames(_features);
    names.append(_unknownFeatures);
    return names;
}

QVariantMap PythonHandshake::toJson() const
{
    QVariantMap json;
    json.insert(QStringLiteral("handshake"), _protocolName);
    json.insert(QStringLiteral("protocolVersion"), _protocolVersion);
    json.insert(QStringLiteral("python"), _environment.toJson());
    json.insert(QStringLiteral("package"), _package.toJson());
    json.insert(QStringLiteral("features"), featureNames());
    return json;
}

}   // namespace Ovito
