// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/automation/python/PythonEnvironmentProbe.h>

#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

namespace Ovito {

namespace {

/**
 * Whether the interpreter that answered is the interpreter that was started.
 *
 * The executable path is compared in its canonical form, so a symlink (`python3` -> `python3.12`) is not a mismatch.
 * Two cases are accepted on purpose:
 *
 *  - the requested executable is inside the environment's `prefix` while the reported `prefix` differs from its
 *    `base_prefix`: this is a virtual environment whose `bin/python` (or `Scripts\python.exe`) is a link to the base
 *    interpreter, and `sys.executable` legitimately names the file that runs;
 *  - both paths are unreadable, so only the cleaned-up spelling is left to compare.
 *
 * Everything else is reported: a wrapper or shim that runs user Python somewhere other than the selected environment
 * is exactly what this check exists for.
 */
bool isSameInterpreter(const QString& requested, const PythonEnvironmentInfo& info)
{
    const QString reported = info.executable();

    const QString requestedCanonical = QFileInfo(requested).canonicalFilePath();
    const QString reportedCanonical = QFileInfo(reported).canonicalFilePath();
    if(!requestedCanonical.isEmpty() && !reportedCanonical.isEmpty() && requestedCanonical == reportedCanonical)
        return true;

    if(!info.prefix().isEmpty() && info.prefix() != info.basePrefix()) {
        const QString requestedAbsolute = QFileInfo(requested).absoluteFilePath();
        const QString prefix = QDir::cleanPath(QDir(info.prefix()).absolutePath());
        if(!requestedAbsolute.isEmpty() && requestedAbsolute.startsWith(prefix + u'/'))
            return true;
    }

    if(requestedCanonical.isEmpty() || reportedCanonical.isEmpty())
        return QDir::cleanPath(requested) == QDir::cleanPath(reported);

    return false;
}

}   // namespace

/******************************************************************************
* Constructs a request for one interpreter.
******************************************************************************/
PythonProbeRequest PythonProbeRequest::forExecutable(QString executable, int timeoutMs)
{
    PythonProbeRequest request;
    request.setExecutable(std::move(executable));
    request.setTimeoutMs(timeoutMs);
    // Fill in the script this build ships, so that a caller can inspect the command line it is about to run. A request
    // that is built by hand gets the same default when the probe starts it.
    request.setScriptFile(PythonEnvironmentProbe::defaultScriptFile());
    return request;
}

QStringList PythonProbeRequest::commandLine() const
{
    QStringList line;
    line << _executable;
    if(_isolated)
        line << QStringLiteral("-I");
    line << _scriptFile;
    return line;
}

/******************************************************************************
* Constructs probe results.
******************************************************************************/
PythonProbeResult PythonProbeResult::compatible(PythonHandshake handshake)
{
    PythonProbeResult result;
    result._status = PythonContract::ProbeStatus::Compatible;
    result._handshake = std::move(handshake);
    result.addDetail(QStringLiteral("executable"), result._handshake->environment().executable());
    result.addDetail(QStringLiteral("detectedPython"), result._handshake->environment().version());
    result.addDetail(QStringLiteral("implementation"), result._handshake->environment().implementation());
    result.addDetail(QStringLiteral("platform"), result._handshake->environment().platform());
    result.addDetail(QStringLiteral("architecture"), result._handshake->environment().machine());
    result.addDetail(QStringLiteral("cacheTag"), result._handshake->environment().cacheTag());
    result.addDetail(QStringLiteral("packageVersion"), result._handshake->package().version());
    result.addDetail(QStringLiteral("protocolVersion"), result._handshake->protocolVersion());
    result.addDetail(QStringLiteral("features"), result._handshake->featureNames());
    return result;
}

PythonProbeResult PythonProbeResult::failure(PythonContract::ProbeStatus status, QString message, QVariantMap details)
{
    PythonProbeResult result;
    result._status = status;
    result._message = std::move(message);
    result._details = std::move(details);
    return result;
}

PythonProbeResult& PythonProbeResult::addDetail(QString key, QVariant value)
{
    _details.insert(std::move(key), std::move(value));
    return *this;
}

QVariantMap PythonProbeResult::toJson() const
{
    QVariantMap json;
    json.insert(QStringLiteral("status"), statusName());
    json.insert(QStringLiteral("compatible"), isCompatible());
    json.insert(QStringLiteral("message"), _message);
    json.insert(QStringLiteral("details"), _details);
    json.insert(QStringLiteral("handshake"), _handshake ? QVariant(_handshake->toJson()) : QVariant());
    return json;
}

/******************************************************************************
* The probe itself.
******************************************************************************/
PythonEnvironmentProbe::PythonEnvironmentProbe(QObject* parent) : QObject(parent) {}

PythonEnvironmentProbe::~PythonEnvironmentProbe()
{
    // A probe that is destroyed while its interpreter runs must not leave the process behind.
    if(_process) {
        _process->kill();
        _process->waitForFinished(1000);
    }
}

QString PythonEnvironmentProbe::defaultScriptFile()
{
#ifdef OVITO_AUTOMATION_PYTHON_DIR
    // Phase 2.6 has no installable package yet; see the header. An installation without the sources gets an empty
    // string, which the probe reports as NotConfigured instead of probing something else.
    const QFileInfo script(QStringLiteral(OVITO_AUTOMATION_PYTHON_DIR "/ovito_probe.py"));
    if(script.exists())
        return script.absoluteFilePath();
#endif
    return {};
}

QString PythonEnvironmentProbe::findInterpreter()
{
    // A candidate, not a fallback: the caller shows this path and the user may change it. Nothing here installs or
    // silently substitutes an interpreter.
    for(const QString& name : { QStringLiteral("python3"), QStringLiteral("python") }) {
        const QString path = QStandardPaths::findExecutable(name);
        if(!path.isEmpty())
            return path;
    }
    return {};
}

bool PythonEnvironmentProbe::isRunning() const
{
    return _running;
}

void PythonEnvironmentProbe::start(const PythonProbeRequest& request)
{
    if(_running) {
        // One probe at a time: this start() does not disturb the running one, it just has no environment to report.
        Q_EMIT finished(PythonProbeResult::failure(
            PythonContract::ProbeStatus::NotConfigured,
            QStringLiteral("Another Python environment probe is still running. Wait for its verdict or cancel it.")));
        return;
    }

    _request = request;
    _cancelled = false;
    if(_request.scriptFile().isEmpty())
        _request.setScriptFile(defaultScriptFile());

    if(_request.executable().isEmpty()) {
        finish(PythonProbeResult::failure(
            PythonContract::ProbeStatus::NotConfigured,
            QStringLiteral("No Python interpreter was given to the probe. Select a Python environment; OVITO does not "
                           "search for an interpreter on its own and does not install one.")));
        return;
    }
    if(_request.scriptFile().isEmpty() || !QFileInfo::exists(_request.scriptFile())) {
        QVariantMap details;
        details.insert(QStringLiteral("scriptFile"), _request.scriptFile());
        details.insert(QStringLiteral("executable"), _request.executable());
        finish(PythonProbeResult::failure(
            PythonContract::ProbeStatus::NotConfigured,
            _request.scriptFile().isEmpty()
                ? QStringLiteral("This build cannot locate its Python probe script, so it cannot inspect a Python "
                                 "environment.")
                : QStringLiteral("The Python probe script \"%1\" does not exist.").arg(_request.scriptFile()),
            details));
        return;
    }

    QStringList arguments;
    if(_request.isIsolated())
        arguments << QStringLiteral("-I");
    arguments << _request.scriptFile();

    _process = new QProcess(this);
    _process->setProcessEnvironment(_request.environment() ? *_request.environment() : QProcessEnvironment::systemEnvironment());
    if(!_request.workingDirectory().isEmpty())
        _process->setWorkingDirectory(_request.workingDirectory());

    connect(_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if(!_running)
            return;
        if(error == QProcess::FailedToStart) {
            QVariantMap details;
            details.insert(QStringLiteral("executable"), _request.executable());
            details.insert(QStringLiteral("error"), _process->errorString());
            details.insert(QStringLiteral("commandLine"), _request.commandLine());
            finish(PythonProbeResult::failure(
                PythonContract::ProbeStatus::InterpreterMissing,
                QStringLiteral("The Python interpreter \"%1\" could not be started (%2). Select an interpreter "
                               "executable that exists.")
                    .arg(_request.executable(), _process->errorString()),
                details));
        }
        else if(error != QProcess::Crashed) {
            QVariantMap details;
            details.insert(QStringLiteral("executable"), _request.executable());
            details.insert(QStringLiteral("error"), _process->errorString());
            finish(PythonProbeResult::failure(
                PythonContract::ProbeStatus::InterpreterFailed,
                QStringLiteral("The Python interpreter \"%1\" failed: %2.").arg(_request.executable(), _process->errorString()),
                details));
        }
    });

    connect(_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        if(!_running)
            return;
        onProcessFinished(exitCode, exitStatus);
    });

    _timer = new QTimer(this);
    _timer->setSingleShot(true);
    connect(_timer, &QTimer::timeout, this, [this]() {
        if(!_running)
            return;
        QVariantMap details;
        details.insert(QStringLiteral("executable"), _request.executable());
        details.insert(QStringLiteral("timeoutMs"), _request.timeoutMs());
        finish(PythonProbeResult::failure(
            PythonContract::ProbeStatus::Timeout,
            QStringLiteral("The Python interpreter \"%1\" did not answer within %2 ms. A slow or blocked environment "
                           "cannot be verified; select another environment or retry.")
                .arg(_request.executable())
                .arg(_request.timeoutMs()),
            details));
    });
    _timer->start(_request.timeoutMs());

    _running = true;
    _process->start(_request.executable(), arguments);
}

void PythonEnvironmentProbe::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    const QByteArray standardOutput = _process->readAllStandardOutput();
    const QByteArray standardError = _process->readAllStandardError();

    QVariantMap baseDetails;
    baseDetails.insert(QStringLiteral("executable"), _request.executable());
    baseDetails.insert(QStringLiteral("commandLine"), _request.commandLine());
    baseDetails.insert(QStringLiteral("exitCode"), exitCode);
    if(!standardError.isEmpty())
        baseDetails.insert(QStringLiteral("stderr"), QString::fromLocal8Bit(standardError).trimmed());

    if(exitStatus == QProcess::CrashExit) {
        finish(PythonProbeResult::failure(
            PythonContract::ProbeStatus::InterpreterFailed,
            QStringLiteral("The Python interpreter \"%1\" crashed while it described its environment.")
                .arg(_request.executable()),
            baseDetails));
        return;
    }

    // The interpreter may print warnings before the handshake; the answer is the last non-empty line.
    QString answer;
    const QList<QByteArray> lines = standardOutput.split('\n');
    for(auto line = lines.crbegin(); line != lines.crend(); ++line) {
        const QString candidate = QString::fromUtf8(*line).trimmed();
        if(!candidate.isEmpty()) {
            answer = candidate;
            break;
        }
    }
    if(answer.isEmpty()) {
        finish(PythonProbeResult::failure(
            PythonContract::ProbeStatus::InterpreterFailed,
            QStringLiteral("The Python interpreter \"%1\" answered without a handshake (exit code %2). It may not be a "
                           "Python interpreter, or it may be missing the probe script's dependencies.")
                .arg(_request.executable())
                .arg(exitCode),
            baseDetails));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(answer.toUtf8(), &parseError);
    if(parseError.error != QJsonParseError::NoError || !document.isObject()) {
        baseDetails.insert(QStringLiteral("answer"), answer.left(400));
        finish(PythonProbeResult::failure(
            PythonContract::ProbeStatus::InterpreterFailed,
            QStringLiteral("The Python interpreter \"%1\" did not answer with a JSON handshake (%2).")
                .arg(_request.executable(), parseError.errorString()),
            baseDetails));
        return;
    }

    // A handshake that cannot be parsed is a protocol problem, not a failure of the environment: the interpreter is
    // healthy, the thing it described is not the message this build speaks.
    QString handshakeError;
    const std::optional<PythonHandshake> handshake = PythonHandshake::fromJson(document.object().toVariantMap(), &handshakeError);
    if(!handshake) {
        baseDetails.insert(QStringLiteral("answer"), answer.left(400));
        finish(PythonProbeResult::failure(
            PythonContract::ProbeStatus::ProtocolError,
            QStringLiteral("%1 The environment may contain a different version of the ovito package.")
                .arg(handshakeError),
            baseDetails));
        return;
    }

    PythonProbeResult result = validate(*handshake);
    if(result.details().isEmpty() || !result.details().contains(QStringLiteral("exitCode")))
        result.addDetail(QStringLiteral("exitCode"), exitCode);
    finish(std::move(result));
}

/******************************************************************************
* Validates an answered handshake in the documented order.
******************************************************************************/
PythonProbeResult PythonEnvironmentProbe::validate(PythonHandshake handshake) const
{
    const PythonEnvironmentInfo& environment = handshake.environment();
    const PythonPackageInfo& package = handshake.package();

    // The details every verdict carries, so a caller can always show what was found next to why it was rejected.
    QVariantMap details;
    details.insert(QStringLiteral("executable"), _request.executable());
    details.insert(QStringLiteral("sysExecutable"), environment.executable());
    details.insert(QStringLiteral("detectedPython"), environment.version());
    details.insert(QStringLiteral("implementation"), environment.implementation());
    details.insert(QStringLiteral("supportedPython"),
                   QStringLiteral("%1 - %2").arg(PythonContract::versionString(PythonContract::minimumPythonVersion()),
                                                 PythonContract::versionString(PythonContract::maximumPythonVersion())));
    details.insert(QStringLiteral("detectedPlatform"), environment.platform());
    details.insert(QStringLiteral("detectedArchitecture"), environment.machine());
    details.insert(QStringLiteral("packageName"), PythonContract::packageName());
    details.insert(QStringLiteral("packageFound"), package.isFound());
    details.insert(QStringLiteral("packageImported"), package.isImported());
    details.insert(QStringLiteral("packageVersion"), package.version());
    details.insert(QStringLiteral("packageProtocolVersion"), package.protocolVersion());
    details.insert(QStringLiteral("expectedProtocolVersion"), PythonContract::protocolVersion());
    details.insert(QStringLiteral("requiredFeatures"), PythonContract::featureNames(_request.requiredFeatures()));
    details.insert(QStringLiteral("advertisedFeatures"), handshake.featureNames());

    // 1. The interpreter that answered must be the interpreter that was started.
    if(!isSameInterpreter(_request.executable(), environment)) {
        PythonProbeResult result = PythonProbeResult::failure(
            PythonContract::ProbeStatus::InterpreterMismatch,
            QStringLiteral("The executable \"%1\" started an interpreter that reports sys.executable = \"%2\". User "
                           "Python would run in an environment other than the selected one.")
                .arg(_request.executable(), environment.executable()),
            details);
        result.setHandshake(std::move(handshake));
        return result;
    }

    // 2. The interpreter must be the implementation and the version this build supports.
    if(environment.implementation().compare(QStringLiteral("cpython"), Qt::CaseInsensitive) != 0) {
        PythonProbeResult result = PythonProbeResult::failure(
            PythonContract::ProbeStatus::PythonUnsupported,
            QStringLiteral("The environment \"%1\" is %2. This build supports CPython %3 to %4. Select an environment "
                           "with a supported interpreter.")
                .arg(_request.executable(), environment.displayName(),
                     PythonContract::versionString(PythonContract::minimumPythonVersion()),
                     PythonContract::versionString(PythonContract::maximumPythonVersion())),
            details);
        result.setHandshake(std::move(handshake));
        return result;
    }
    if(!PythonContract::supportsPythonVersion(environment.versionMajor(), environment.versionMinor())) {
        PythonProbeResult result = PythonProbeResult::failure(
            PythonContract::ProbeStatus::PythonUnsupported,
            QStringLiteral("The environment \"%1\" is %2, but this build supports CPython %3 to %4. Select an "
                           "environment with a supported interpreter.")
                .arg(_request.executable(), environment.displayName(),
                     PythonContract::versionString(PythonContract::minimumPythonVersion()),
                     PythonContract::versionString(PythonContract::maximumPythonVersion())),
            details);
        result.setHandshake(std::move(handshake));
        return result;
    }

    // 3. The platform and architecture matrix of this build.
    if(!PythonContract::supportedPlatforms().contains(environment.platform())) {
        PythonProbeResult result = PythonProbeResult::failure(
            PythonContract::ProbeStatus::PlatformUnsupported,
            QStringLiteral("The environment \"%1\" reports the platform \"%2\"; this build ships the ovito package for "
                           "%3.")
                .arg(_request.executable(), environment.platform(),
                     PythonContract::supportedPlatforms().join(QStringLiteral(", "))),
            details);
        result.setHandshake(std::move(handshake));
        return result;
    }
    if(!PythonContract::supportedArchitectures().contains(environment.machine())) {
        PythonProbeResult result = PythonProbeResult::failure(
            PythonContract::ProbeStatus::PlatformUnsupported,
            QStringLiteral("The environment \"%1\" reports the machine architecture \"%2\"; this build ships the ovito "
                           "package for %3.")
                .arg(_request.executable(), environment.machine(),
                     PythonContract::supportedArchitectures().join(QStringLiteral(", "))),
            details);
        result.setHandshake(std::move(handshake));
        return result;
    }

    // 4. The package must be there and must have been imported successfully. A package that is present but broken is
    // reported with its own error, because that is the case a manifest cannot rule out.
    if(!package.isFound()) {
        PythonProbeResult result = PythonProbeResult::failure(
            PythonContract::ProbeStatus::PackageMissing,
            QStringLiteral("The environment \"%1\" (%2) does not have the \"%3\" package installed. Install the package "
                           "that belongs to this OVITO build into this environment (for example with "
                           "\"uv pip install ovito\" for a virtual environment or \"python -m pip install ovito\" for a "
                           "system interpreter), or select another Python environment. OVITO does not install or "
                           "upgrade packages by itself.")
                .arg(_request.executable(), environment.displayName(), PythonContract::packageName()),
            details);
        result.setHandshake(std::move(handshake));
        return result;
    }
    if(!package.isImported() || !package.error().isEmpty()) {
        if(!package.error().isEmpty())
            details.insert(QStringLiteral("importError"), package.error());
        PythonProbeResult result = PythonProbeResult::failure(
            PythonContract::ProbeStatus::PackageIncompatible,
            package.error().isEmpty()
                ? QStringLiteral("The \"%1\" package in \"%2\" could not be imported.")
                      .arg(PythonContract::packageName(), _request.executable())
                : QStringLiteral("The \"%1\" package in \"%2\" could not be imported: %3")
                      .arg(PythonContract::packageName(), _request.executable(), package.error()),
            details);
        result.setHandshake(std::move(handshake));
        return result;
    }

    // 5. The protocol major version must match. The minor version is not compared on its own: within one major version
    // the protocol only grows, so the feature check below is the decision (see PythonContract).
    if(package.protocolVersion().isEmpty()) {
        PythonProbeResult result = PythonProbeResult::failure(
            PythonContract::ProbeStatus::PackageIncompatible,
            QStringLiteral("The \"%1\" package in \"%2\" does not report a protocol version, so it cannot be used by "
                           "this build (%3).")
                .arg(PythonContract::packageName(), _request.executable(), PythonContract::protocolVersion()),
            details);
        result.setHandshake(std::move(handshake));
        return result;
    }
    const QStringList protocolParts = package.protocolVersion().split(u'.');
    const int packageMajor = protocolParts.value(0).toInt();
    const int packageMinor = protocolParts.value(1).toInt();
    if(packageMajor != PythonContract::protocolVersionMajor) {
        PythonProbeResult result = PythonProbeResult::failure(
            PythonContract::ProbeStatus::PackageIncompatible,
            QStringLiteral("The \"%1\" package in \"%2\" speaks protocol %3, this build speaks %4. Install the package "
                           "version that belongs to this OVITO build.")
                .arg(PythonContract::packageName(), _request.executable(), package.protocolVersion(),
                     PythonContract::protocolVersion()),
            details);
        result.setHandshake(std::move(handshake));
        return result;
    }

    // 6. Every feature the caller requires must be offered. An unknown feature name in the answer is kept in the
    // details above; only features this build knows can satisfy a requirement.
    QVector<PythonContract::Feature> missing;
    for(PythonContract::Feature feature : _request.requiredFeatures()) {
        if(!package.features().contains(feature))
            missing.push_back(feature);
    }
    if(!missing.isEmpty()) {
        details.insert(QStringLiteral("missingFeatures"), PythonContract::featureNames(missing));
        PythonProbeResult result = PythonProbeResult::failure(
            PythonContract::ProbeStatus::FeatureMissing,
            QStringLiteral("The \"%1\" package in \"%2\" (version %3, protocol %4) does not offer: %5. Install the "
                           "package version that belongs to this OVITO build.")
                .arg(PythonContract::packageName(), _request.executable(),
                     package.version().isEmpty() ? QStringLiteral("unknown") : package.version(),
                     package.protocolVersion(), PythonContract::featureNames(missing).join(QStringLiteral(", "))),
            details);
        result.setHandshake(std::move(handshake));
        return result;
    }

    // 7. The environment is usable. The minor protocol version is reported next to the verdict for the record.
    if(packageMinor > PythonContract::protocolVersionMinor)
        details.insert(QStringLiteral("newerMinorProtocolVersion"), package.protocolVersion());
    PythonProbeResult result = PythonProbeResult::compatible(std::move(handshake));
    for(auto entry = details.cbegin(); entry != details.cend(); ++entry)
        result.addDetail(entry.key(), entry.value());
    return result;
}

/******************************************************************************
* Ends the probe with one verdict.
******************************************************************************/
void PythonEnvironmentProbe::finish(PythonProbeResult result)
{
    const bool wasRunning = _running;
    _running = false;

    if(_timer) {
        _timer->stop();
        _timer->deleteLater();
        _timer = nullptr;
    }
    if(_process) {
        if(wasRunning && _process->state() != QProcess::NotRunning) {
            // A cooperative shutdown first, then the hard stop: a hanging interpreter is what the timeout is for.
            _process->terminate();
            if(!_process->waitForFinished(100))
                _process->kill();
        }
        _process->deleteLater();
        _process = nullptr;
    }

    Q_EMIT finished(result);
}

void PythonEnvironmentProbe::cancel()
{
    if(!_running)
        return;

    _cancelled = true;
    QVariantMap details;
    details.insert(QStringLiteral("executable"), _request.executable());
    finish(PythonProbeResult::failure(PythonContract::ProbeStatus::Cancelled,
                                      QStringLiteral("The probe of \"%1\" was cancelled before the environment "
                                                     "answered.")
                                          .arg(_request.executable()),
                                      details));
}

PythonProbeResult PythonEnvironmentProbe::probeBlocking(const PythonProbeRequest& request)
{
    PythonProbeResult verdict;
    QEventLoop loop;
    PythonEnvironmentProbe probe;
    QObject::connect(&probe, &PythonEnvironmentProbe::finished, &loop, [&verdict, &loop](const PythonProbeResult& result) {
        verdict = result;
        loop.quit();
    });
    probe.start(request);
    if(probe.isRunning())
        loop.exec();
    return verdict;
}

}   // namespace Ovito
