// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/automation/python/PythonIntrospector.h>

#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTimer>

namespace Ovito {

/******************************************************************************
* The request.
******************************************************************************/
PythonIntrospectionRequest PythonIntrospectionRequest::forScript(QString executable, QString targetPath,
                                                                PythonSchemaPreview::Mode mode, int timeoutMs)
{
    PythonIntrospectionRequest request;
    request.setExecutable(std::move(executable));
    request.setTargetPath(std::move(targetPath));
    request.setMode(mode);
    request.setTimeoutMs(timeoutMs);
    return request;
}

QStringList PythonIntrospectionRequest::commandLine() const
{
    QStringList arguments;
    if(_isolated)
        arguments << QStringLiteral("-I");
    arguments << _scriptFile;
    arguments << QStringLiteral("--path") << _targetPath;
    arguments << QStringLiteral("--mode") << PythonSchemaPreview::modeName(_mode);
    return arguments;
}

/******************************************************************************
* The introspector.
******************************************************************************/
PythonIntrospector::PythonIntrospector(QObject* parent) : QObject(parent) {}

PythonIntrospector::~PythonIntrospector()
{
    // An introspection that is destroyed while its interpreter runs must not leave the process behind.
    if(_process) {
        _process->kill();
        _process->waitForFinished(1000);
    }
}

QString PythonIntrospector::defaultScriptFile()
{
#ifdef OVITO_AUTOMATION_PYTHON_DIR
    // Phase 2.6 has no installable package yet; see the header and open item O16.
    const QFileInfo script(QStringLiteral(OVITO_AUTOMATION_PYTHON_DIR "/ovito_schema.py"));
    if(script.exists())
        return script.absoluteFilePath();
#endif
    return {};
}

bool PythonIntrospector::isRunning() const
{
    return _running;
}

void PythonIntrospector::start(const PythonIntrospectionRequest& request)
{
    if(_running) {
        // One introspection at a time: this start() does not disturb the running one, it just has nothing to report.
        Q_EMIT finished(PythonSchemaPreview::failure(
            QStringLiteral("internal_error"),
            QStringLiteral("Another script is still being read. Wait for its report or cancel it."),
            request.mode(), request.targetPath()));
        return;
    }

    _request = request;
    _cancelled = false;
    if(_request.scriptFile().isEmpty())
        _request.setScriptFile(defaultScriptFile());

    if(_request.executable().isEmpty()) {
        Q_EMIT finished(PythonSchemaPreview::failure(
            QStringLiteral("interpreter_missing"),
            QStringLiteral("No Python interpreter was given, so the script cannot be read. Select a Python environment; "
                           "OVITO does not search for one on its own and does not install one."),
            _request.mode(), _request.targetPath()));
        return;
    }
    if(_request.scriptFile().isEmpty() || !QFileInfo::exists(_request.scriptFile())) {
        Q_EMIT finished(PythonSchemaPreview::failure(
            QStringLiteral("script_missing"),
            _request.scriptFile().isEmpty()
                ? QStringLiteral("This build cannot locate its Python introspection script, so it cannot read the "
                                 "schema of a script.")
                : QStringLiteral("The Python introspection script \"%1\" does not exist.").arg(_request.scriptFile()),
            _request.mode(), _request.targetPath()));
        return;
    }
    if(_request.targetPath().isEmpty()) {
        Q_EMIT finished(PythonSchemaPreview::failure(
            QStringLiteral("script_missing"),
            QStringLiteral("No script was named to read the schema of."),
            _request.mode(), _request.targetPath()));
        return;
    }

    _process = new QProcess(this);
    _process->setProcessEnvironment(_request.environment() ? *_request.environment() : QProcessEnvironment::systemEnvironment());
    if(!_request.workingDirectory().isEmpty())
        _process->setWorkingDirectory(_request.workingDirectory());

    const auto annotate = [this](PythonSchemaPreview preview) {
        // Every failure of the exchange carries the same facts, so a caller can log or display why it happened without
        // knowing which failure it was.
        preview.addDetail(QStringLiteral("executable"), _request.executable());
        preview.addDetail(QStringLiteral("commandLine"), _request.commandLine().join(u' '));
        preview.addDetail(QStringLiteral("mode"), PythonSchemaPreview::modeName(_request.mode()));
        return preview;
    };

    connect(_process, &QProcess::errorOccurred, this, [this, annotate](QProcess::ProcessError error) {
        if(!_running || error == QProcess::Crashed)
            return;
        // `finished` reports a crash; this reports the cases where the process never got that far.
        PythonSchemaPreview preview = PythonSchemaPreview::failure(
            error == QProcess::FailedToStart ? QStringLiteral("interpreter_missing") : QStringLiteral("interpreter_failed"),
            error == QProcess::FailedToStart
                ? QStringLiteral("The Python interpreter \"%1\" could not be started (%2). Select an interpreter "
                                 "executable that exists.").arg(_request.executable(), _process->errorString())
                : QStringLiteral("The Python interpreter \"%1\" failed: %2.").arg(_request.executable(), _process->errorString()),
            _request.mode(), _request.targetPath());
        preview.addDetail(QStringLiteral("error"), _process->errorString());
        finish(annotate(std::move(preview)));
    });

    connect(_process, &QProcess::finished, this, [this, annotate](int exitCode, QProcess::ExitStatus exitStatus) {
        if(!_running)
            return;
        onProcessFinished(exitCode, exitStatus);
    });

    _timer = new QTimer(this);
    _timer->setSingleShot(true);
    connect(_timer, &QTimer::timeout, this, [this, annotate]() {
        if(!_running)
            return;
        PythonSchemaPreview preview = PythonSchemaPreview::failure(
            QStringLiteral("timeout"),
            QStringLiteral("The Python interpreter \"%1\" did not report the schema of \"%2\" within %3 ms.")
                .arg(_request.executable(), _request.targetPath())
                .arg(_request.timeoutMs()),
            _request.mode(), _request.targetPath());
        preview.addDetail(QStringLiteral("timeoutMs"), _request.timeoutMs());
        finish(annotate(std::move(preview)));
    });
    _timer->start(_request.timeoutMs());

    _running = true;
    _process->start(_request.executable(), _request.commandLine());
}

void PythonIntrospector::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    const QByteArray standardOutput = _process->readAllStandardOutput();
    const QByteArray standardError = _process->readAllStandardError();

    const auto annotate = [this, exitCode, standardError](PythonSchemaPreview preview) {
        preview.addDetail(QStringLiteral("executable"), _request.executable());
        preview.addDetail(QStringLiteral("commandLine"), _request.commandLine().join(u' '));
        preview.addDetail(QStringLiteral("exitCode"), exitCode);
        if(!standardError.isEmpty())
            preview.addDetail(QStringLiteral("stderr"), QString::fromLocal8Bit(standardError).trimmed());
        return preview;
    };

    if(exitStatus == QProcess::CrashExit) {
        finish(annotate(PythonSchemaPreview::failure(
            QStringLiteral("interpreter_failed"),
            QStringLiteral("The Python interpreter \"%1\" crashed while it read the schema of \"%2\".")
                .arg(_request.executable(), _request.targetPath()),
            _request.mode(), _request.targetPath())));
        return;
    }

    // The script may print warnings before its report; the answer is the last non-empty line.
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
        finish(annotate(PythonSchemaPreview::failure(
            QStringLiteral("protocol_error"),
            QStringLiteral("The Python interpreter \"%1\" reported nothing about \"%2\" (exit code %3).")
                .arg(_request.executable(), _request.targetPath())
                .arg(exitCode),
            _request.mode(), _request.targetPath())));
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(answer.toUtf8());
    if(!document.isObject()) {
        PythonSchemaPreview preview = PythonSchemaPreview::failure(
            QStringLiteral("protocol_error"),
            QStringLiteral("The Python interpreter \"%1\" answered with something other than a schema report.")
                .arg(_request.executable()),
            _request.mode(), _request.targetPath());
        preview.addDetail(QStringLiteral("answer"), answer.left(200));
        finish(annotate(std::move(preview)));
        return;
    }

    QString problem;
    std::optional<PythonSchemaPreview> preview = PythonSchemaPreview::fromJson(document.object().toVariantMap(), &problem);
    if(!preview) {
        finish(annotate(PythonSchemaPreview::failure(
            QStringLiteral("protocol_error"),
            QStringLiteral("The schema report of \"%1\" could not be read: %2.")
                .arg(_request.targetPath(), problem),
            _request.mode(), _request.targetPath())));
        return;
    }
    finish(annotate(std::move(*preview)));
}

void PythonIntrospector::finish(PythonSchemaPreview preview)
{
    _running = false;
    if(_timer) {
        _timer->stop();
        _timer->deleteLater();
        _timer = nullptr;
    }
    if(_process) {
        if(_process->state() != QProcess::NotRunning) {
            // A cancelled or timed-out introspection leaves an interpreter behind; it is killed before the report is
            // emitted so that no caller can observe a report and a running process at the same time.
            _process->kill();
            _process->waitForFinished(1000);
        }
        _process->deleteLater();
        _process = nullptr;
    }
    Q_EMIT finished(preview);
}

void PythonIntrospector::cancel()
{
    if(!_running)
        return;
    // A cancelled introspection is not a verdict about the script: the report says so, and the caller is expected to
    // retry (the frontend clears its "reading..." state on the same report).
    _cancelled = true;
    PythonSchemaPreview preview = PythonSchemaPreview::failure(
        QStringLiteral("cancelled"),
        QStringLiteral("Reading the schema of \"%1\" was cancelled.").arg(_request.targetPath()),
        _request.mode(), _request.targetPath());
    finish(std::move(preview));
}

PythonSchemaPreview PythonIntrospector::introspectBlocking(const PythonIntrospectionRequest& request)
{
    PythonIntrospector introspector;
    PythonSchemaPreview result;
    QEventLoop loop;
    QObject::connect(&introspector, &PythonIntrospector::finished, &loop, [&loop, &result](const PythonSchemaPreview& preview) {
        result = preview;
        loop.quit();
    });
    introspector.start(request);
    // start() may already have finished (a missing interpreter, a missing script); the loop then exits immediately.
    if(introspector.isRunning())
        loop.exec();
    return result;
}

}   // namespace Ovito
