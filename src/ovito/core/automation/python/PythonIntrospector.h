// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/automation/python/PythonSchemaPreview.h>

#include <QProcessEnvironment>
#include <QStringList>

#include <optional>

QT_BEGIN_NAMESPACE
class QProcess;
class QTimer;
QT_END_NAMESPACE

namespace Ovito {

/**
 * \brief What to read from one Python script.
 *
 * The request names the interpreter, the target script and the mode. Everything else has a documented default, so the
 * common case is one line: `PythonIntrospectionRequest::forScript(interpreter, path)`.
 *
 * `mode` is the important field, and its default is the safe one. `PythonSchemaPreview::Mode::Ast` parses the file
 * without running anything, which is what a preview, an AI plan or a file-open handler uses. Asking for
 * `Mode::Import` is a statement that the caller has the user's consent to execute the file's top-level code - the
 * design keeps those two apart on purpose, and this request is where the decision is made explicit.
 *
 * `isolated` starts the interpreter with `-I` (no user site packages, no `PYTHON*` environment variables) for the same
 * reason the environment probe does it: the application needs the interpreter the user selected and nothing that the
 * ambient environment happens to add. The introspection script itself needs no third-party package in either mode.
 */
class OVITO_CORE_EXPORT PythonIntrospectionRequest
{
public:

    /// A request for one script, read in the given mode (AST by default).
    static PythonIntrospectionRequest forScript(QString executable, QString targetPath,
                                               PythonSchemaPreview::Mode mode = PythonSchemaPreview::Mode::Ast,
                                               int timeoutMs = DefaultTimeoutMs);

    /// The interpreter to run. Empty means "nothing to read", which the introspector reports as `script_missing`.
    PythonIntrospectionRequest& setExecutable(QString executable) { _executable = std::move(executable); return *this; }
    const QString& executable() const { return _executable; }

    /**
     * The script that reads the schema, which defaults to PythonIntrospector::defaultScriptFile(). A caller sets it
     * explicitly to read a script other than the one this build ships, which is what the tests do.
     */
    PythonIntrospectionRequest& setScriptFile(QString scriptFile) { _scriptFile = std::move(scriptFile); return *this; }
    const QString& scriptFile() const { return _scriptFile; }

    /// The file whose schema is read. It is passed to the script, never opened by this process.
    PythonIntrospectionRequest& setTargetPath(QString targetPath) { _targetPath = std::move(targetPath); return *this; }
    const QString& targetPath() const { return _targetPath; }

    /// Whether the file may be executed (see the class comment). Defaults to the AST mode.
    PythonIntrospectionRequest& setMode(PythonSchemaPreview::Mode mode) { _mode = mode; return *this; }
    PythonSchemaPreview::Mode mode() const { return _mode; }

    /// Starts the interpreter with `-I`. Default true.
    PythonIntrospectionRequest& setIsolated(bool isolated) { _isolated = isolated; return *this; }
    bool isIsolated() const { return _isolated; }

    /// How long the interpreter has to answer. A timeout is a verdict about reading the file, not about the file.
    PythonIntrospectionRequest& setTimeoutMs(int timeoutMs) { _timeoutMs = timeoutMs; return *this; }
    int timeoutMs() const { return _timeoutMs; }

    /// Environment variables for the interpreter, inherited from the application unless a caller sets them.
    PythonIntrospectionRequest& setEnvironment(QProcessEnvironment environment) { _environment = std::move(environment); return *this; }
    const std::optional<QProcessEnvironment>& environment() const { return _environment; }

    /// The working directory of the interpreter. Empty inherits the application's.
    PythonIntrospectionRequest& setWorkingDirectory(QString directory) { _workingDirectory = std::move(directory); return *this; }
    const QString& workingDirectory() const { return _workingDirectory; }

    /// The command line the introspector would run, for logs, for a CLI's JSON mode and for a failure message.
    QStringList commandLine() const;

    static constexpr int DefaultTimeoutMs = 20000;

private:

    QString _executable;
    QString _scriptFile;
    QString _targetPath;
    PythonSchemaPreview::Mode _mode = PythonSchemaPreview::Mode::Ast;
    bool _isolated = true;
    int _timeoutMs = DefaultTimeoutMs;
    std::optional<QProcessEnvironment> _environment;
    QString _workingDirectory;
};

/**
 * \brief Reads the schema of a Python script by running the introspection script of this build.
 *
 * The class is the process half of the preview and it owns no schema logic of its own: it starts the interpreter, waits
 * for one JSON object, parses it with PythonSchemaPreview and reports the failures of the exchange as diagnostics of
 * that same report. That is deliberate - a caller then has exactly one result type to render, and the difference
 * between "the file has a syntax error" and "the interpreter died" is a code in a diagnostic rather than a second
 * return channel.
 *
 * It is asynchronous because it spawns a process and its callers are a frontend that must not block; `finished` is
 * emitted exactly once per `start()`, whatever happens. `introspectBlocking()` exists for the callers that are
 * already asynchronous themselves (a task, a CLI command, a test) and runs a nested event loop.
 *
 * It never writes anything, never installs anything and never imports a module of its own.
 */
class OVITO_CORE_EXPORT PythonIntrospector : public QObject
{
    Q_OBJECT

public:

    explicit PythonIntrospector(QObject* parent = nullptr);
    ~PythonIntrospector() override;

    /**
     * \brief The introspection script that ships with this build.
     *
     * Phase 2.6 has no installable package yet, so the script is found next to its own sources and the build bakes
     * that path in - the same seam as PythonEnvironmentProbe::defaultScriptFile(), and open item O16 requires Phase 4
     * to replace both with an installed-package lookup. An installation that cannot see the sources gets an empty
     * string, which is reported as `script_missing` rather than silently reading something else.
     */
    static QString defaultScriptFile();

    /// Starts reading one script. `finished` is emitted exactly once, with a report for every outcome.
    void start(const PythonIntrospectionRequest& request);

    /// Asks the running introspection to stop; the verdict is `cancelled`. A request that is not running does nothing.
    void cancel();

    bool isRunning() const;

    /// The request of the running introspection; empty before the first start().
    const PythonIntrospectionRequest& request() const { return _request; }

    /**
     * \brief Reads a script and waits for the report.
     *
     * A convenience for callers that are already asynchronous. It runs a nested event loop, so it must not be called
     * from the GUI thread; the asynchronous interface is the real one.
     */
    static PythonSchemaPreview introspectBlocking(const PythonIntrospectionRequest& request);

Q_SIGNALS:

    /// The report. Emitted exactly once per start(), including for a cancelled or failed introspection.
    void finished(const Ovito::PythonSchemaPreview& preview);

private:

    /// Reads the interpreter's answer and turns it into a report.
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

    /// Ends the introspection with one report: kills the process, stops the timer and emits.
    void finish(PythonSchemaPreview preview);

    PythonIntrospectionRequest _request;
    QProcess* _process = nullptr;
    QTimer* _timer = nullptr;
    bool _running = false;
    bool _cancelled = false;
};

}   // namespace Ovito
