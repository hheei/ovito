// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/automation/python/PythonContract.h>

#include <QStringList>
#include <QVector>

namespace Ovito {

/**
 * \brief What a Python interpreter reported about itself.
 *
 * Every field comes from the interpreter's own modules (`sys`, `platform`, `sys.implementation`) and is reported to
 * the application unchanged. Two of them are load-bearing:
 *
 *  - `executable` is `sys.executable`, i.e. the interpreter that is *actually* running user code. The application
 *    compares it with the executable it started, because a wrapper or a shim would silently run user Python somewhere
 *    else than the selected environment.
 *  - `cacheTag` is the ABI tag of the interpreter (`cpython-312`), which is what a native bridge built for one
 *    specific CPython reports back. The application records it; whether a bridge is compatible is decided by the
 *    package, which is the artifact that owns an ABI.
 */
class OVITO_CORE_EXPORT PythonEnvironmentInfo
{
public:

    static std::optional<PythonEnvironmentInfo> fromJson(const QVariantMap& json, QString* errorMessage = nullptr);
    QVariantMap toJson() const;

    /// Whether the parser found everything it cannot do without. A structurally invalid description is rejected before
    /// any compatibility rule is applied.
    bool isValid() const;

    // Fluent setup, mirroring the other wire types of the automation layer.
    PythonEnvironmentInfo& setImplementation(QString implementation) { _implementation = std::move(implementation); return *this; }
    PythonEnvironmentInfo& setVersion(QString version) { _version = std::move(version); return *this; }
    PythonEnvironmentInfo& setVersionInfo(int major, int minor, int micro) { _versionMajor = major; _versionMinor = minor; _versionMicro = micro; return *this; }
    PythonEnvironmentInfo& setExecutable(QString executable) { _executable = std::move(executable); return *this; }
    PythonEnvironmentInfo& setPrefix(QString prefix) { _prefix = std::move(prefix); return *this; }
    PythonEnvironmentInfo& setBasePrefix(QString basePrefix) { _basePrefix = std::move(basePrefix); return *this; }
    PythonEnvironmentInfo& setPlatform(QString platform) { _platform = std::move(platform); return *this; }
    PythonEnvironmentInfo& setMachine(QString machine) { _machine = std::move(machine); return *this; }
    PythonEnvironmentInfo& setArchitecture(QString architecture) { _architecture = std::move(architecture); return *this; }
    PythonEnvironmentInfo& setCacheTag(QString cacheTag) { _cacheTag = std::move(cacheTag); return *this; }
    PythonEnvironmentInfo& setFreeThreaded(bool freeThreaded) { _freeThreaded = freeThreaded; return *this; }
    PythonEnvironmentInfo& setIsolated(bool isolated) { _isolated = isolated; return *this; }

    const QString& implementation() const { return _implementation; }
    const QString& version() const { return _version; }
    int versionMajor() const { return _versionMajor; }
    int versionMinor() const { return _versionMinor; }
    int versionMicro() const { return _versionMicro; }
    const QString& executable() const { return _executable; }
    const QString& prefix() const { return _prefix; }
    const QString& basePrefix() const { return _basePrefix; }
    const QString& platform() const { return _platform; }
    const QString& machine() const { return _machine; }
    const QString& architecture() const { return _architecture; }
    const QString& cacheTag() const { return _cacheTag; }
    bool isFreeThreaded() const { return _freeThreaded; }
    bool isIsolated() const { return _isolated; }

    /// "CPython 3.12.3" - the phrase the probe's messages use.
    QString displayName() const;

private:

    QString _implementation;
    QString _version;
    int _versionMajor = 0;
    int _versionMinor = 0;
    int _versionMicro = 0;
    QString _executable;
    QString _prefix;
    QString _basePrefix;
    QString _platform;
    QString _machine;
    QString _architecture;
    QString _cacheTag;
    bool _freeThreaded = false;
    bool _isolated = false;
};

/**
 * \brief What the `ovito` package in the probed environment reported about itself.
 *
 * `found` and `imported` are deliberately separate. `found` comes from an `importlib` metadata lookup that does not
 * execute the package, so it is what an installer's dependency resolution can see; `imported` means the package was
 * really imported and answered the handshake. A package that is found but cannot be imported is reported with the
 * error text - the case of a mixed or half-installed environment, which is exactly what the design says a manifest
 * cannot rule out.
 *
 * `features` is the same feature vocabulary as PythonContract::Feature. An unknown feature name is kept as a raw
 * string in the details of a failure rather than dropped, so that an environment built against a newer protocol still
 * explains itself; only the features this build knows can satisfy a requirement.
 */
class OVITO_CORE_EXPORT PythonPackageInfo
{
public:

    static std::optional<PythonPackageInfo> fromJson(const QVariantMap& json, QString* errorMessage = nullptr);
    QVariantMap toJson() const;

    bool isFound() const { return _found; }
    bool isImported() const { return _imported; }
    bool isUsable() const { return _imported && _error.isEmpty(); }
    const QString& name() const { return _name; }
    const QString& moduleFile() const { return _moduleFile; }
    const QString& version() const { return _version; }
    const QString& protocolVersion() const { return _protocolVersion; }
    const QVector<PythonContract::Feature>& features() const { return _features; }
    const QStringList& unknownFeatures() const { return _unknownFeatures; }
    const QVariantMap& bridge() const { return _bridge; }
    const QString& error() const { return _error; }

    PythonPackageInfo& setFound(bool found) { _found = found; return *this; }
    PythonPackageInfo& setImported(bool imported) { _imported = imported; return *this; }
    PythonPackageInfo& setName(QString name) { _name = std::move(name); return *this; }
    PythonPackageInfo& setModuleFile(QString moduleFile) { _moduleFile = std::move(moduleFile); return *this; }
    PythonPackageInfo& setVersion(QString version) { _version = std::move(version); return *this; }
    PythonPackageInfo& setProtocolVersion(QString protocolVersion) { _protocolVersion = std::move(protocolVersion); return *this; }
    PythonPackageInfo& setFeatures(QVector<PythonContract::Feature> features) { _features = std::move(features); return *this; }
    PythonPackageInfo& setUnknownFeatures(QStringList features) { _unknownFeatures = std::move(features); return *this; }
    PythonPackageInfo& setBridge(QVariantMap bridge) { _bridge = std::move(bridge); return *this; }
    PythonPackageInfo& setError(QString error) { _error = std::move(error); return *this; }

private:

    bool _found = false;
    bool _imported = false;
    QString _name;
    QString _moduleFile;
    QString _version;
    QString _protocolVersion;
    QVector<PythonContract::Feature> _features;
    QStringList _unknownFeatures;
    QVariantMap _bridge;
    QString _error;
};

/**
 * \brief The answer of a Python environment to the application's compatibility handshake.
 *
 * This is the parsed form of the single JSON line that `automation/python/ovito_probe.py` prints, and the message the
 * eventually supplied package will produce from inside its own module. It is a value type: the probe validates it and
 * hands it on, so nothing that observes an environment has to parse JSON itself.
 *
 * Only structural problems (a missing field, an unparseable version, a different protocol name) make `fromJson` fail.
 * Every compatibility rule - the interpreter identity, the version envelope, the platform, the package and its
 * features - belongs to the probe, which reports one status and one explanation for all of them.
 */
class OVITO_CORE_EXPORT PythonHandshake
{
public:

    static std::optional<PythonHandshake> fromJson(const QVariantMap& json, QString* errorMessage = nullptr);
    QVariantMap toJson() const;

    /// The protocol name of the message, which must be PythonContract::handshakeName().
    const QString& protocolName() const { return _protocolName; }
    /// The handshake protocol version the environment speaks, "major.minor".
    const QString& protocolVersion() const { return _protocolVersion; }
    int protocolVersionMajor() const { return _protocolVersionMajor; }
    int protocolVersionMinor() const { return _protocolVersionMinor; }

    const PythonEnvironmentInfo& environment() const { return _environment; }
    const PythonPackageInfo& package() const { return _package; }

    /// The features the environment offers for use, which is the package's list: without a package there is nothing to
    /// run user code with.
    const QVector<PythonContract::Feature>& features() const { return _features; }
    const QStringList& unknownFeatures() const { return _unknownFeatures; }

    /// True when the message carries everything the validation needs: the right protocol name, a parseable version
    /// and a valid interpreter description.
    bool isValid() const;

    // Fluent setup for the tests and for callers that build a handshake themselves.
    PythonHandshake& setProtocolName(QString name) { _protocolName = std::move(name); return *this; }
    PythonHandshake& setProtocolVersion(QString version);
    PythonHandshake& setEnvironment(PythonEnvironmentInfo environment) { _environment = std::move(environment); return *this; }
    PythonHandshake& setPackage(PythonPackageInfo package) { _package = std::move(package); return *this; }
    PythonHandshake& setFeatures(QVector<PythonContract::Feature> features) { _features = std::move(features); return *this; }

    /// The wire form of a feature list: known names first, then names this build does not know.
    QStringList featureNames() const;

private:

    QString _protocolName;
    QString _protocolVersion;
    int _protocolVersionMajor = 0;
    int _protocolVersionMinor = 0;
    PythonEnvironmentInfo _environment;
    PythonPackageInfo _package;
    QVector<PythonContract::Feature> _features;
    QStringList _unknownFeatures;
};

}   // namespace Ovito
