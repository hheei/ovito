// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/automation/python/PythonContract.h>

namespace Ovito {

/******************************************************************************
* Returns the wire name of a feature.
******************************************************************************/
QString PythonContract::featureName(Feature feature)
{
    switch(feature) {
    case Feature::SchemaIntrospection: return QStringLiteral("schema.introspection");
    case Feature::FunctionInplace: return QStringLiteral("function.inplace");
    case Feature::ParameterScalar: return QStringLiteral("parameter.scalar");
    case Feature::ArrayBuffer: return QStringLiteral("array.buffer");
    case Feature::SharedMemoryArray: return QStringLiteral("array.shared-memory");
    case Feature::TaskCancellation: return QStringLiteral("task.cancellation");
    case Feature::TracebackMapping: return QStringLiteral("traceback.mapping");
    }
    return {};
}

/******************************************************************************
* Parses the wire name of a feature.
******************************************************************************/
std::optional<PythonContract::Feature> PythonContract::featureFromName(const QString& name)
{
    const QVector<Feature> features = allFeatures();
    for(Feature feature : features)
        if(featureName(feature) == name)
            return feature;
    return std::nullopt;
}

/******************************************************************************
* Returns every feature this build knows.
******************************************************************************/
QVector<PythonContract::Feature> PythonContract::allFeatures()
{
    return { Feature::SchemaIntrospection, Feature::FunctionInplace, Feature::ParameterScalar, Feature::ArrayBuffer,
             Feature::SharedMemoryArray,    Feature::TaskCancellation,  Feature::TracebackMapping };
}

/******************************************************************************
* Returns the wire name of a probe status.
******************************************************************************/
QString PythonContract::probeStatusName(ProbeStatus status)
{
    switch(status) {
    case ProbeStatus::Compatible: return QStringLiteral("compatible");
    case ProbeStatus::NotConfigured: return QStringLiteral("not_configured");
    case ProbeStatus::InterpreterMissing: return QStringLiteral("interpreter_missing");
    case ProbeStatus::InterpreterFailed: return QStringLiteral("interpreter_failed");
    case ProbeStatus::Timeout: return QStringLiteral("timeout");
    case ProbeStatus::ProtocolError: return QStringLiteral("protocol_error");
    case ProbeStatus::PythonUnsupported: return QStringLiteral("python_unsupported");
    case ProbeStatus::PlatformUnsupported: return QStringLiteral("platform_unsupported");
    case ProbeStatus::PackageMissing: return QStringLiteral("package_missing");
    case ProbeStatus::PackageIncompatible: return QStringLiteral("package_incompatible");
    case ProbeStatus::FeatureMissing: return QStringLiteral("feature_missing");
    case ProbeStatus::InterpreterMismatch: return QStringLiteral("interpreter_mismatch");
    case ProbeStatus::Cancelled: return QStringLiteral("cancelled");
    }
    return {};
}

/******************************************************************************
* Parses the wire name of a probe status.
******************************************************************************/
std::optional<PythonContract::ProbeStatus> PythonContract::probeStatusFromName(const QString& name)
{
    // A linear scan is right here: the list is short and the order of the enumerators carries meaning, which a hash
    // would hide.
    for(int value = 0; value <= static_cast<int>(ProbeStatus::Cancelled); ++value) {
        const auto status = static_cast<ProbeStatus>(value);
        if(probeStatusName(status) == name)
            return status;
    }
    return std::nullopt;
}

/******************************************************************************
* Returns the handshake name and the package name.
******************************************************************************/
QString PythonContract::handshakeName()
{
    return QStringLiteral("ovito.automation.handshake");
}

QString PythonContract::packageName()
{
    return QStringLiteral("ovito");
}

/******************************************************************************
* Returns the protocol version of this build.
******************************************************************************/
QString PythonContract::protocolVersion()
{
    return QStringLiteral("%1.%2").arg(protocolVersionMajor).arg(protocolVersionMinor);
}

/******************************************************************************
* Returns the supported interpreter range and the supported platform matrix.
******************************************************************************/
QVector<int> PythonContract::minimumPythonVersion()
{
    // 3.10 is the oldest CPython the project's desktop targets still ship and the oldest with the C API the bridge
    // will be built against.
    return { 3, 10 };
}

QVector<int> PythonContract::maximumPythonVersion()
{
    // 3.13 is the newest CPython at the time of this adaptation. A newer interpreter is not silently accepted: it
    // would need a bridge built for it, which is what this envelope declares.
    return { 3, 13 };
}

bool PythonContract::supportsPythonVersion(int major, int minor)
{
    const QVector<int> minimum = minimumPythonVersion();
    const QVector<int> maximum = maximumPythonVersion();
    if(major != minimum.at(0) || major != maximum.at(0))
        return false;
    return minor >= minimum.at(1) && minor <= maximum.at(1);
}

QStringList PythonContract::supportedPlatforms()
{
    // The three platforms of the migration's target matrix, as `sys.platform` spells them.
    return { QStringLiteral("linux"), QStringLiteral("darwin"), QStringLiteral("win32") };
}

QStringList PythonContract::supportedArchitectures()
{
    // `platform.machine()` on the three targets: x86_64 and AMD64 both mean AMD64, arm64/aarch64 mean ARM64.
    return { QStringLiteral("x86_64"), QStringLiteral("AMD64"), QStringLiteral("amd64"),
             QStringLiteral("arm64"),  QStringLiteral("aarch64") };
}

/******************************************************************************
* Returns the features the application requires by default.
******************************************************************************/
QVector<PythonContract::Feature> PythonContract::defaultRequiredFeatures()
{
    return { Feature::SchemaIntrospection, Feature::FunctionInplace, Feature::ArrayBuffer };
}

/******************************************************************************
* Renders feature lists and versions for messages and details.
******************************************************************************/
QStringList PythonContract::featureNames(const QVector<Feature>& features)
{
    QStringList names;
    names.reserve(features.size());
    for(Feature feature : features)
        names.push_back(featureName(feature));
    return names;
}

QString PythonContract::versionString(const QVector<int>& version)
{
    QStringList parts;
    for(int part : version)
        parts.push_back(QString::number(part));
    return parts.join(u'.');
}

}   // namespace Ovito
