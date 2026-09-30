// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/automation/python/PythonContract.h>
#include <ovito/core/automation/python/PythonHandshake.h>

#include <QProcess>
#include <QProcessEnvironment>
#include <QStringList>

#include <optional>

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

namespace Ovito {

/**
 * \brief What a caller wants to know about a Python environment.
 *
 * A request names the interpreter to run (which the caller obtained from the user's setting, see the design) and the
 * requirements that decide compatibility. Everything else has a documented default, so a caller that only wants to
 * know "is this environment usable?" writes `PythonProbeRequest::forExecutable(path)`.
 *
 * Two fields are deliberate rather than convenient:
 *
 *  - `isolated` starts the interpreter with `-I`, so that user site packages and `PYTHON*` environment variables do not
 *    change what the probe sees. The application needs the same determinism when it later runs user code, so the
 *    default is the isolated form and a caller that wants to see a `PYTHONPATH`-style setup has to ask for it.
 *  - `environment` replaces the whole environment only if a caller sets it. A probe never edits the user's
 *    environment, and it never writes to the environment it probes.
 */
class OVITO_CORE_EXPORT PythonProbeRequest
{
public:

    /// A request for one interpreter with the default requirements and script.
    static PythonProbeRequest forExecutable(QString executable, int timeoutMs = DefaultTimeoutMs);

    /// The interpreter executable to start. Empty means "nothing to probe", which the probe reports as NotConfigured.
    PythonProbeRequest& setExecutable(QString executable) { _executable = std::move(executable); return *this; }
    const QString& executable() const { return _executable; }

    /**
     * The script the interpreter runs. Defaults to PythonEnvironmentProbe::defaultScriptFile(), the probe script that
     * ships with this build. A caller sets it explicitly to validate a handshake other than the built-in one, which is
     * what the tests do.
     */
    PythonProbeRequest& setScriptFile(QString scriptFile) { _scriptFile = std::move(scriptFile); return *this; }
    const QString& scriptFile() const { return _scriptFile; }

    /// The features the caller needs. Defaults to PythonContract::defaultRequiredFeatures().
    PythonProbeRequest& setRequiredFeatures(QVector<PythonContract::Feature> features) { _requiredFeatures = std::move(features); return *this; }
    const QVector<PythonContract::Feature>& requiredFeatures() const { return _requiredFeatures; }

    /// Starts the interpreter with `-I` (no user site, no `PYTHON*` environment variables). Default true.
    PythonProbeRequest& setIsolated(bool isolated) { _isolated = isolated; return *this; }
    bool isIsolated() const { return _isolated; }

    /// How long the interpreter has to answer. A timeout is a verdict about the probe, not about the environment.
    PythonProbeRequest& setTimeoutMs(int timeoutMs) { _timeoutMs = timeoutMs; return *this; }
    int timeoutMs() const { return _timeoutMs; }

    /// Environment variables for the interpreter, inherited from the application unless a caller sets them.
    PythonProbeRequest& setEnvironment(QProcessEnvironment environment) { _environment = std::move(environment); return *this; }
    const std::optional<QProcessEnvironment>& environment() const { return _environment; }

    /// The working directory of the interpreter. Empty inherits the application's.
    PythonProbeRequest& setWorkingDirectory(QString directory) { _workingDirectory = std::move(directory); return *this; }
    const QString& workingDirectory() const { return _workingDirectory; }

    /// The command line the probe would run, for logs and for the error message of a failed start.
    QStringList commandLine() const;

    static constexpr int DefaultTimeoutMs = 10000;

private:

    QString _executable;
    QString _scriptFile;
    QVector<PythonContract::Feature> _requiredFeatures = PythonContract::defaultRequiredFeatures();
    bool _isolated = true;
    int _timeoutMs = DefaultTimeoutMs;
    std::optional<QProcessEnvironment> _environment;
    QString _workingDirectory;
};

/**
 * \brief The verdict of one environment probe.
 *
 * The result is the only thing the application needs to decide what to do with an environment, and it is designed so
 * that no caller has to interpret a process exit code, read stderr, or parse JSON:
 *
 *  - `status` is the machine-readable verdict of PythonContract::ProbeStatus, in the documented check order;
 *  - `message` names what is missing and how to repair it, including the fact that OVITO never installs or upgrades
 *    anything itself;
 *  - `details` is the structured form of the same information (the interpreter that answered, the supported range, the
 *    features that were required and missing, the import error, the captured stderr), for a CLI's JSON mode and for a
 *    frontend that wants to show more than one sentence;
 *  - `handshake` is present whenever the interpreter answered, even when the verdict is negative, so a caller can
 *    display the environment it found instead of only the reason it was rejected.
 */
class OVITO_CORE_EXPORT PythonProbeResult
{
public:

    PythonProbeResult() = default;

    /// The environment passed every check.
    static PythonProbeResult compatible(PythonHandshake handshake);

    /// The environment was rejected, or could not be probed. `message` is required and names the repair.
    static PythonProbeResult failure(PythonContract::ProbeStatus status, QString message, QVariantMap details = {});

    bool isCompatible() const { return _status == PythonContract::ProbeStatus::Compatible; }
    PythonContract::ProbeStatus status() const { return _status; }
    const QString& message() const { return _message; }
    const QVariantMap& details() const { return _details; }

    /// The handshake that was received, if the interpreter answered at all. Present for a rejected environment as
    /// well, which is what lets a UI say "this is what I found" instead of only "this did not work".
    const std::optional<PythonHandshake>& handshake() const { return _handshake; }
    void setHandshake(PythonHandshake handshake) { _handshake = std::move(handshake); }

    /// Adds one entry to the structured details. The probe fills them while it validates.
    PythonProbeResult& addDetail(QString key, QVariant value);
    PythonProbeResult& addDetail(QString key, const QString& value) { return addDetail(std::move(key), QVariant(value)); }

    /// The wire form, used by a CLI's JSON mode and by the tests.
    QVariantMap toJson() const;

    /// The status as its wire name.
    QString statusName() const { return PythonContract::probeStatusName(_status); }

private:

    PythonContract::ProbeStatus _status = PythonContract::ProbeStatus::NotConfigured;
    QString _message;
    QVariantMap _details;
    std::optional<PythonHandshake> _handshake;
};

/**
 * \brief Runs a Python interpreter once and validates the environment it describes.
 *
 * The probe is the runtime half of the package contract: it starts the selected interpreter with the probe script,
 * reads the handshake the interpreter prints, and applies the rules of PythonContract in a documented order. Its
 * verdict is the only thing the rest of the application looks at, so the rules live in one place instead of being
 * restated by every caller that launches Python.
 *
 * It is asynchronous because it spawns a process, and because its callers are a frontend that must not block and a CLI
 * that wants to report progress: `start()` returns immediately and `finished` is emitted exactly once, whatever
 * happens - a start failure, a crash, a malformed answer or the timeout.
 *
 * The class never installs, upgrades or switches anything. A failing verdict is a report; the caller (and later the
 * user, in the environment selector of Phase 4) decides what to do about it, and the task/activity plumbing of the
 * automation session is what records that decision.
 */
class OVITO_CORE_EXPORT PythonEnvironmentProbe : public QObject
{
    Q_OBJECT

public:

    explicit PythonEnvironmentProbe(QObject* parent = nullptr);
    ~PythonEnvironmentProbe() override;

    /**
     * \brief The probe script that ships with this build.
     *
     * Phase 2.6 has no installable package yet, so the script is found next to its own sources and the build bakes that
     * path in. An installation that cannot see the sources gets an empty string, and a request without a script file
     * is reported as NotConfigured rather than silently probing something else.
     */
    static QString defaultScriptFile();

    /// Returns the interpreter to probe when the user has not selected one: the first `python3` on the path. Empty when
    /// there is none, which is not an error - the caller reports NotConfigured.
    static QString findInterpreter();

    /// Starts the probe. `finished` is emitted once, with a verdict for every outcome.
    void start(const PythonProbeRequest& request);

    /// Asks the running probe to stop. The interpreter is terminated, and the verdict is NotConfigured - a cancelled
    /// probe is not a verdict about the environment. A probe that is not running does nothing.
    void cancel();

    bool isRunning() const;

    /// The request of the running probe; empty before the first start().
    const PythonProbeRequest& request() const { return _request; }

    /**
     * \brief Probes an environment and waits for the verdict.
     *
     * A convenience for callers that are already asynchronous (a task, a CLI command, a test): it runs a nested event
     * loop, so it must not be called from the GUI thread. The asynchronous interface is the real one.
     */
    static PythonProbeResult probeBlocking(const PythonProbeRequest& request);

Q_SIGNALS:

    /// The verdict. Emitted exactly once per start(), including for a cancelled probe.
    void finished(const Ovito::PythonProbeResult& result);

private:

    /// Handles the answer of the interpreter: reads its output, parses the handshake and validates it.
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

    /// The documented validation order. Each step either returns a failure or leaves the verdict to the next one.
    PythonProbeResult validate(PythonHandshake handshake) const;

    /// Ends the probe with one verdict: stops the process, stops the timer and emits.
    void finish(PythonProbeResult result);

    PythonProbeRequest _request;
    QProcess* _process = nullptr;
    QTimer* _timer = nullptr;
    bool _running = false;
    bool _cancelled = false;
};

}   // namespace Ovito

Q_DECLARE_METATYPE(Ovito::PythonProbeResult)
