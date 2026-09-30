// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

/**
 * \brief Tests of the Python schema preview (Phase 2.6, deliverable 7).
 *
 * The preview has two halves and this suite tests both: the value type of the report (which needs no Python at all) and
 * the introspector that runs `ovito_schema.py` to produce it. Like the environment probe, the whole class is
 * independent of the OVITO object model - it spawns one interpreter, reads one JSON object and returns a report - so
 * the suite runs without an Application instance, without an ambient task and without a GUI.
 *
 * What the cases are really about is the promise the AST mode makes: **nothing in the file runs**. The fixtures
 * therefore write a marker file from their top level, and the suite asserts that the marker does *not* exist after a
 * preview and *does* exist after an explicit import - one case per half of that rule. The other rules with a test each
 * are the position of a syntax error, a decorator or decorator argument that is computed at run time (reported, never
 * guessed), the mapped traceback of a failing user script, and the failures of the exchange itself (no interpreter, no
 * script, an answer that is not a report, a timeout, a cancellation).
 *
 * A machine without a `python3` executable skips the cases that need one instead of failing; the value-type cases run
 * everywhere.
 */

#include <ovito/core/automation/python/PythonIntrospector.h>
#include <ovito/core/automation/python/PythonEnvironmentProbe.h>
#include <ovito/core/automation/python/PythonSchemaPreview.h>

#include <QtTest>

#include <algorithm>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

using namespace Ovito;

/// Skips the current case on a machine without Python, and defines `interpreter` for it.
#define SKIP_WITHOUT_PYTHON()                                                                                                    \
    const QString interpreter = PythonEnvironmentProbe::findInterpreter();                                                       \
    if(interpreter.isEmpty())                                                                                                    \
    QSKIP("This machine has no python3 executable on its path.")

class PythonSchemaPreviewTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void initTestCase();

    // The value type of the report: the rules that need no interpreter.
    void preview_rejects_a_report_format_it_does_not_know();
    void preview_round_trips_through_json();

    // The AST mode: reading a schema without running anything.
    void preview_reads_a_modifier_without_running_the_script();
    void preview_reports_a_syntax_error_with_its_position();
    void preview_reports_a_decorator_that_is_computed_at_run_time();
    void preview_reports_a_decorator_argument_that_is_computed_at_run_time();

    // The explicit import mode, which is the only one allowed to execute the file.
    void preview_imports_the_script_only_when_asked();
    void preview_maps_a_traceback_of_the_user_script();

    // The failures of reading the file rather than of the file.
    void preview_reports_a_missing_file();
    void preview_reports_a_missing_interpreter();
    void preview_reports_a_missing_script();
    void preview_reports_an_answer_that_is_not_a_report();
    void preview_reports_a_timeout();
    void preview_can_be_cancelled();
    void preview_emits_exactly_one_report();

private:

    /// Writes one fixture script into the temporary directory and returns its path.
    QString writeFixture(const QString& name, const QString& contents);

    /// The path of the marker a fixture writes when its top level runs, for the cases that prove nothing ran.
    QString markerPath(const QString& name) const { return QDir(_tempDirectory.path()).filePath(name + QStringLiteral(".marker")); }

    /// A request that reads the given script with the introspection script of this build.
    PythonIntrospectionRequest requestFor(const QString& executable, const QString& targetPath,
                                          PythonSchemaPreview::Mode mode = PythonSchemaPreview::Mode::Ast) const;

    QTemporaryDir _tempDirectory;
};

void PythonSchemaPreviewTest::initTestCase()
{
    QVERIFY2(_tempDirectory.isValid(), "Cannot create a temporary directory for the preview fixtures.");
    // The introspection script is part of the build; without it the suite would test nothing.
    QVERIFY2(!PythonIntrospector::defaultScriptFile().isEmpty(), "This build does not know its Python introspection script.");
    QVERIFY(QFileInfo::exists(PythonIntrospector::defaultScriptFile()));
}

QString PythonSchemaPreviewTest::writeFixture(const QString& name, const QString& contents)
{
    const QString path = QDir(_tempDirectory.path()).filePath(name);
    QFile file(path);
    if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return {};
    file.write(contents.toUtf8());
    file.close();
    return path;
}

PythonIntrospectionRequest PythonSchemaPreviewTest::requestFor(const QString& executable, const QString& targetPath,
                                                               PythonSchemaPreview::Mode mode) const
{
    PythonIntrospectionRequest request = PythonIntrospectionRequest::forScript(executable, targetPath, mode, 20000);
    // The suite must be hermetic: the interpreter that reads the fixture is the one the machine has, and it inherits
    // nothing that would change what it sees.
    request.setEnvironment(QProcessEnvironment::systemEnvironment());
    return request;
}

/******************************************************************************
* The value type of the report.
******************************************************************************/
void PythonSchemaPreviewTest::preview_rejects_a_report_format_it_does_not_know()
{
    // A report from a *newer* introspection script is not read leniently: a mixed installation is what this check is
    // for, and guessing at the shape of a format this build does not know is how a schema becomes silently wrong.
    QVariantMap json;
    json.insert(QStringLiteral("schemaVersion"), 2);
    json.insert(QStringLiteral("functions"), QVariantList());
    json.insert(QStringLiteral("diagnostics"), QVariantList());
    QString problem;
    QVERIFY(!PythonSchemaPreview::fromJson(json, &problem).has_value());
    QVERIFY(problem.contains(QStringLiteral("schema version")));

    // A report that is missing the lists is not a report either.
    QVariantMap incomplete;
    incomplete.insert(QStringLiteral("schemaVersion"), 1);
    problem.clear();
    QVERIFY(!PythonSchemaPreview::fromJson(incomplete, &problem).has_value());
    QVERIFY(problem.contains(QStringLiteral("functions")));
}

void PythonSchemaPreviewTest::preview_round_trips_through_json()
{
    // The report is a value type: what a caller gets from the wire can be stored, compared and re-serialized.
    QVariantMap json;
    json.insert(QStringLiteral("ok"), true);
    json.insert(QStringLiteral("schemaVersion"), 1);
    json.insert(QStringLiteral("mode"), QStringLiteral("ast"));
    json.insert(QStringLiteral("path"), QStringLiteral("/tmp/fixture.py"));
    json.insert(QStringLiteral("moduleDocstring"), QStringLiteral("A fixture."));
    json.insert(QStringLiteral("imports"), QVariantList{
        QVariantMap{ { QStringLiteral("module"), QStringLiteral("math") }, { QStringLiteral("names"), QStringList{ QStringLiteral("math") } }, { QStringLiteral("line"), 1 } },
    });
    json.insert(QStringLiteral("functions"), QVariantList{
        QVariantMap{
            { QStringLiteral("name"), QStringLiteral("modify") },
            { QStringLiteral("line"), 4 },
            { QStringLiteral("docstring"), QStringLiteral("Counts.") },
            { QStringLiteral("decorators"), QStringList{ QStringLiteral("python_script") } },
            { QStringLiteral("isModifier"), true },
            { QStringLiteral("status"), QStringLiteral("supported") },
            { QStringLiteral("reason"), QString() },
            { QStringLiteral("parameters"), QVariantList{
                QVariantMap{
                    { QStringLiteral("name"), QStringLiteral("cutoff") },
                    { QStringLiteral("kind"), QStringLiteral("keyword_only") },
                    { QStringLiteral("annotation"), QStringLiteral("float") },
                    { QStringLiteral("default"), 2.5 },
                    { QStringLiteral("required"), false },
                    { QStringLiteral("defaultDynamic"), false },
                },
            } },
        },
    });
    json.insert(QStringLiteral("diagnostics"), QVariantList{
        QVariantMap{
            { QStringLiteral("severity"), QStringLiteral("warning") },
            { QStringLiteral("code"), QStringLiteral("dynamic_decorator_argument") },
            { QStringLiteral("message"), QStringLiteral("...") },
            { QStringLiteral("line"), 3 },
            { QStringLiteral("column"), -1 },
        },
    });

    std::optional<PythonSchemaPreview> preview = PythonSchemaPreview::fromJson(json);
    QVERIFY(preview.has_value());
    QVERIFY(preview->isOk());
    QCOMPARE(preview->mode(), PythonSchemaPreview::Mode::Ast);
    QCOMPARE(preview->path(), QStringLiteral("/tmp/fixture.py"));
    QCOMPARE(preview->moduleDocstring(), QStringLiteral("A fixture."));
    QCOMPARE(preview->importedModules().size(), 1);
    QCOMPARE(preview->functions().size(), 1);
    QVERIFY(preview->diagnostics().size() == 1);
    QCOMPARE(preview->diagnostics().first().severity, PythonSchemaPreview::Severity::Warning);

    const PythonSchemaPreview::Function* function = preview->findFunction(QStringLiteral("modify"));
    QVERIFY(function != nullptr);
    QCOMPARE(function->status, PythonSchemaPreview::Status::Supported);
    QVERIFY(function->isModifier);
    QCOMPARE(function->docstring, QStringLiteral("Counts."));
    QCOMPARE(function->parameters.size(), 1);
    QCOMPARE(function->parameters.first().name, QStringLiteral("cutoff"));
    QCOMPARE(function->parameters.first().kind, QStringLiteral("keyword_only"));
    QCOMPARE(function->parameters.first().annotation, QStringLiteral("float"));
    QCOMPARE(function->parameters.first().defaultValue.toDouble(), 2.5);
    QVERIFY(!function->parameters.first().isRequired);
    QCOMPARE(preview->functionsOfStatus(PythonSchemaPreview::Status::Supported).size(), 1);
    QCOMPARE(preview->functionsOfStatus(PythonSchemaPreview::Status::Unsupported).size(), 0);
    QVERIFY(preview->findFunction(QStringLiteral("missing")) == nullptr);
    QVERIFY(!preview->importResult().has_value());
    QVERIFY(preview->summary().contains(QStringLiteral("1 of them modifiers")));

    // The wire form of the parsed report is a report again.
    std::optional<PythonSchemaPreview> second = PythonSchemaPreview::fromJson(preview->toJson());
    QVERIFY(second.has_value());
    QCOMPARE(second->path(), preview->path());
    QCOMPARE(second->functions().size(), 1);
    QCOMPARE(second->functions().first().parameters.size(), 1);
    QCOMPARE(second->diagnostics().size(), 1);
}

/******************************************************************************
* The AST mode.
******************************************************************************/
void PythonSchemaPreviewTest::preview_reads_a_modifier_without_running_the_script()
{
    SKIP_WITHOUT_PYTHON();
    // The fixture writes a marker from its top level. If the preview ever ran the file, the marker would exist - which
    // is the whole point of the AST mode, and the reason this case asserts its absence first.
    const QString path = writeFixture(QStringLiteral("modifier.py"), QStringLiteral(
        "import pathlib\n"
        "import math\n"
        "pathlib.Path(__file__).with_suffix('.marker').write_text('ran')\n"
        "\n"
        "@python_script(cutoff=2.5, label='Bond check')\n"
        "def modify(positions, threshold: float = 2.5, *, frames=1, **kwargs):\n"
        "    \"\"\"Counts neighbours.\"\"\"\n"
        "    return math.pi\n"
        "\n"
        "def helper(value, other, /, third=3):\n"
        "    return value\n"));
    QFile::remove(markerPath(QStringLiteral("modifier")));   // whatever an earlier run left behind
    QVERIFY(!QFileInfo::exists(markerPath(QStringLiteral("modifier"))));

    PythonSchemaPreview preview = PythonIntrospector::introspectBlocking(requestFor(interpreter, path));
    QVERIFY2(!QFileInfo::exists(markerPath(QStringLiteral("modifier"))),
             "Reading the schema of a script executed its top level.");
    QVERIFY(preview.isOk());
    QCOMPARE(preview.mode(), PythonSchemaPreview::Mode::Ast);
    QCOMPARE(preview.path(), path);
    QCOMPARE(preview.importedModules().size(), 2);
    QVERIFY(!preview.importResult().has_value());   // Nothing was imported, and the report says so by staying silent.

    const PythonSchemaPreview::Function* modifier = preview.findFunction(QStringLiteral("modify"));
    QVERIFY(modifier != nullptr);
    QCOMPARE(modifier->status, PythonSchemaPreview::Status::Supported);
    QVERIFY(modifier->isModifier);
    QCOMPARE(modifier->docstring, QStringLiteral("Counts neighbours."));
    QVERIFY(modifier->decorators.contains(QStringLiteral("python_script")));
    QVERIFY(modifier->reason.isEmpty());
    QCOMPARE(modifier->parameters.size(), 4);
    QCOMPARE(modifier->parameters[0].name, QStringLiteral("positions"));
    QCOMPARE(modifier->parameters[0].kind, QStringLiteral("positional_or_keyword"));
    QVERIFY(modifier->parameters[0].isRequired);
    QVERIFY(!modifier->parameters[0].hasDefault);
    QCOMPARE(modifier->parameters[1].name, QStringLiteral("threshold"));
    QCOMPARE(modifier->parameters[1].annotation, QStringLiteral("float"));   // As written, never resolved.
    QCOMPARE(modifier->parameters[1].defaultValue.toDouble(), 2.5);
    QVERIFY(!modifier->parameters[1].isRequired);
    QCOMPARE(modifier->parameters[2].kind, QStringLiteral("keyword_only"));
    QCOMPARE(modifier->parameters[2].defaultValue.toInt(), 1);
    QCOMPARE(modifier->parameters[3].kind, QStringLiteral("var_keyword"));
    QVERIFY(modifier->parameters[3].isRequired);

    // A function without a schema decorator is listed as an ordinary function, not as a modifier - and its positional-only
    // parameter keeps the kind the language gave it.
    const PythonSchemaPreview::Function* helper = preview.findFunction(QStringLiteral("helper"));
    QVERIFY(helper != nullptr);
    QCOMPARE(helper->status, PythonSchemaPreview::Status::Plain);
    QVERIFY(!helper->isModifier);
    QCOMPARE(helper->parameters[0].kind, QStringLiteral("positional_only"));
    QCOMPARE(helper->parameters[2].kind, QStringLiteral("positional_or_keyword"));
    QCOMPARE(preview.functionsOfStatus(PythonSchemaPreview::Status::Supported).size(), 1);
    QVERIFY(preview.diagnostics().isEmpty());
}

void PythonSchemaPreviewTest::preview_reports_a_syntax_error_with_its_position()
{
    SKIP_WITHOUT_PYTHON();
    const QString path = writeFixture(QStringLiteral("broken.py"), QStringLiteral("def broken(:\n    pass\n"));
    PythonSchemaPreview preview = PythonIntrospector::introspectBlocking(requestFor(interpreter, path));

    QVERIFY(!preview.isOk());
    QCOMPARE(preview.functions().size(), 0);
    QCOMPARE(preview.diagnostics().size(), 1);
    const PythonSchemaPreview::Diagnostic& diagnostic = preview.diagnostics().first();
    QCOMPARE(diagnostic.severity, PythonSchemaPreview::Severity::Error);
    QCOMPARE(diagnostic.code, QStringLiteral("syntax_error"));
    QCOMPARE(diagnostic.line, 1);
    QVERIFY(diagnostic.column > 0);
    QVERIFY(diagnostic.text.contains(QStringLiteral("def broken(")));
    // The summary of a report that failed is the reason it failed, so a frontend has one thing to display.
    QCOMPARE(preview.summary(), diagnostic.message);
}

void PythonSchemaPreviewTest::preview_reports_a_decorator_that_is_computed_at_run_time()
{
    SKIP_WITHOUT_PYTHON();
    // Two ways a decorator can be out of reach of a syntax tree: it is looked up by an expression, or it is chosen by a
    // function call. Neither may be reported as "the modifier's schema is ...".
    const QString path = writeFixture(QStringLiteral("computed.py"), QStringLiteral(
        "import os\n"
        "registry = {}\n"
        "\n"
        "def pick(function):\n"
        "    return function\n"
        "\n"
        "def make_name():\n"
        "    return 'x'\n"
        "\n"
        "@registry[os.environ['KIND']]\n"
        "def first(a=1):\n"
        "    pass\n"
        "\n"
        "@pick(make_name())\n"
        "def second(b=2):\n"
        "    pass\n"));
    PythonSchemaPreview preview = PythonIntrospector::introspectBlocking(requestFor(interpreter, path));

    // The report itself is still a report: the file parses, and what cannot be read is said rather than left out.
    QVERIFY(preview.isOk());
    const PythonSchemaPreview::Function* first = preview.findFunction(QStringLiteral("first"));
    QVERIFY(first != nullptr);
    QCOMPARE(first->status, PythonSchemaPreview::Status::Unsupported);
    QVERIFY(first->reason.contains(QStringLiteral("evaluated at run time")));
    QVERIFY(first->decorators.contains(QStringLiteral("registry[os.environ['KIND']]")));

    // `@pick(...)` *is* a named decorator, so the function is an ordinary one - but the argument it was configured with
    // is not knowable, which the report states without pretending the value is known.
    const PythonSchemaPreview::Function* second = preview.findFunction(QStringLiteral("second"));
    QVERIFY(second != nullptr);
    QCOMPARE(second->status, PythonSchemaPreview::Status::Plain);
    QVERIFY(second->decorators.contains(QStringLiteral("pick")));
    const auto warning = std::find_if(preview.diagnostics().cbegin(), preview.diagnostics().cend(), [](const PythonSchemaPreview::Diagnostic& diagnostic) {
        return diagnostic.code == QStringLiteral("dynamic_decorator_argument");
    });
    QVERIFY(warning != preview.diagnostics().cend());
    QCOMPARE(warning->severity, PythonSchemaPreview::Severity::Warning);
}

void PythonSchemaPreviewTest::preview_reports_a_decorator_argument_that_is_computed_at_run_time()
{
    SKIP_WITHOUT_PYTHON();
    // The schema decorator is recognized, but how it was configured is not: the declared schema cannot be read, so the
    // function must not be offered as a modifier whose schema is known.
    const QString path = writeFixture(QStringLiteral("dynamic_argument.py"), QStringLiteral(
        "import os\n"
        "\n"
        "@python_script(label=os.environ['LABEL'], cutoff=2.5)\n"
        "def modify(a=1):\n"
        "    pass\n"
        "\n"
        "@python_script(cutoff=2.5)\n"
        "def other(b=2):\n"
        "    pass\n"));
    PythonSchemaPreview preview = PythonIntrospector::introspectBlocking(requestFor(interpreter, path));

    const PythonSchemaPreview::Function* dynamic = preview.findFunction(QStringLiteral("modify"));
    QVERIFY(dynamic != nullptr);
    QVERIFY(dynamic->isModifier);
    QCOMPARE(dynamic->status, PythonSchemaPreview::Status::Unsupported);
    QVERIFY(dynamic->reason.contains(QStringLiteral("computed at run time")));
    // The arguments it *could* read are still reported; the one it could not is marked, not guessed.
    QVERIFY(preview.diagnostics().size() >= 1);

    const PythonSchemaPreview::Function* readable = preview.findFunction(QStringLiteral("other"));
    QVERIFY(readable != nullptr);
    QCOMPARE(readable->status, PythonSchemaPreview::Status::Supported);
    QCOMPARE(preview.functionsOfStatus(PythonSchemaPreview::Status::Supported).size(), 1);
    QVERIFY(preview.summary().contains(QStringLiteral("could not be read")));
}

/******************************************************************************
* The explicit import mode.
******************************************************************************/
void PythonSchemaPreviewTest::preview_imports_the_script_only_when_asked()
{
    SKIP_WITHOUT_PYTHON();
    // One fixture, two modes, one marker: this is the case that separates "preview" from "run it".
    // The fixture defines the decorator itself, so that the two modes differ in *one* thing only: whether the file
    // ran. (A script that expects the decorator to come from somewhere else would fail the import for that reason
    // instead - which is a real difference between the modes, and the reason the import mode exists.)
    const QString path = writeFixture(QStringLiteral("sideeffect.py"), QStringLiteral(
        "import pathlib\n"
        "pathlib.Path(__file__).with_suffix('.marker').write_text('ran')\n"
        "\n"
        "def python_script(**kwargs):\n"
        "    def decorate(function):\n"
        "        function.__schema__ = kwargs\n"
        "        return function\n"
        "    return decorate\n"
        "\n"
        "@python_script(cutoff=1.0)\n"
        "def modify(a=1):\n"
        "    pass\n"));
    QFile::remove(markerPath(QStringLiteral("sideeffect")));
    QVERIFY(!QFileInfo::exists(markerPath(QStringLiteral("sideeffect"))));

    PythonSchemaPreview preview = PythonIntrospector::introspectBlocking(requestFor(interpreter, path));
    QVERIFY(preview.isOk());
    QVERIFY2(!QFileInfo::exists(markerPath(QStringLiteral("sideeffect"))), "The AST mode executed the fixture.");
    QVERIFY(!preview.importResult().has_value());

    PythonSchemaPreview imported = PythonIntrospector::introspectBlocking(
        requestFor(interpreter, path, PythonSchemaPreview::Mode::Import));
    QVERIFY2(QFileInfo::exists(markerPath(QStringLiteral("sideeffect"))), "The import mode did not load the fixture.");
    QVERIFY(imported.isOk());
    QCOMPARE(imported.mode(), PythonSchemaPreview::Mode::Import);
    QVERIFY(imported.importResult().has_value());
    QVERIFY(imported.importResult()->attempted);
    QVERIFY(imported.importResult()->loaded);
    QVERIFY(imported.importResult()->errorCode.isEmpty());
    // The schema is the same in both modes: the import adds the runtime facts rather than replacing the reading.
    QVERIFY(imported.findFunction(QStringLiteral("modify")) != nullptr);
    QCOMPARE(imported.findFunction(QStringLiteral("modify"))->status, PythonSchemaPreview::Status::Supported);
}

void PythonSchemaPreviewTest::preview_maps_a_traceback_of_the_user_script()
{
    SKIP_WITHOUT_PYTHON();
    const QString path = writeFixture(QStringLiteral("raising.py"), QStringLiteral(
        "def helper():\n"
        "    raise ValueError('boom')\n"
        "\n"
        "helper()\n"));
    PythonSchemaPreview preview = PythonIntrospector::introspectBlocking(
        requestFor(interpreter, path, PythonSchemaPreview::Mode::Import));

    // The report is not ok, because the file did not run to the end - and it says where it stopped.
    QVERIFY(!preview.isOk());
    QVERIFY(preview.importResult().has_value());
    const PythonSchemaPreview::ImportResult& result = *preview.importResult();
    QVERIFY(result.attempted);
    QVERIFY(!result.loaded);
    QCOMPARE(result.errorCode, QStringLiteral("import_failed"));
    QCOMPARE(result.errorType, QStringLiteral("ValueError"));
    QVERIFY(result.errorMessage.contains(QStringLiteral("boom")));

    // The mapping the design asks for: the frames of the script itself, with file, function and line - the outermost
    // frame first, the innermost last, exactly as a traceback reads.
    QVector<const PythonSchemaPreview::TracebackFrame*> userFrames;
    for(const PythonSchemaPreview::TracebackFrame& frame : result.traceback) {
        if(frame.fromUserScript)
            userFrames.push_back(&frame);
    }
    QCOMPARE(userFrames.size(), 2);
    QCOMPARE(userFrames[0]->function, QStringLiteral("<module>"));
    QCOMPARE(userFrames[0]->line, 4);
    QVERIFY(userFrames[0]->text.contains(QStringLiteral("helper()")));
    QCOMPARE(userFrames[1]->function, QStringLiteral("helper"));
    QCOMPARE(userFrames[1]->line, 2);
    QVERIFY(userFrames[1]->text.contains(QStringLiteral("raise ValueError")));
    QCOMPARE(QFileInfo(userFrames[1]->file).absoluteFilePath(), QFileInfo(path).absoluteFilePath());
    // The frames of the machinery that ran the script are kept, but not marked as the user's.
    QVERIFY(result.traceback.size() > userFrames.size());

    // A failed import is also a diagnostic, so that "the schema could not be read" names one reason in one place - and
    // it points at the line of the script that raised, not at the machinery that loaded it.
    QCOMPARE(preview.diagnostics().size(), 1);
    QCOMPARE(preview.diagnostics().first().code, QStringLiteral("import_failed"));
    QCOMPARE(preview.diagnostics().first().line, 2);
    QVERIFY(preview.diagnostics().first().message.contains(QStringLiteral("ValueError")));
    QCOMPARE(preview.summary(), preview.diagnostics().first().message);
}

/******************************************************************************
* The failures of reading the file.
******************************************************************************/
void PythonSchemaPreviewTest::preview_reports_a_missing_file()
{
    SKIP_WITHOUT_PYTHON();
    // The file is missing, but the interpreter and the introspection script work: the report says which of the two
    // things is missing, which a caller cannot tell from an exit code.
    const QString path = QDir(_tempDirectory.path()).filePath(QStringLiteral("not-there.py"));
    PythonSchemaPreview preview = PythonIntrospector::introspectBlocking(requestFor(interpreter, path));

    QVERIFY(!preview.isOk());
    QCOMPARE(preview.functions().size(), 0);
    QCOMPARE(preview.diagnostics().size(), 1);
    QCOMPARE(preview.diagnostics().first().code, QStringLiteral("unreadable_file"));
    QVERIFY(preview.diagnostics().first().message.contains(QStringLiteral("could not be read")));
}

void PythonSchemaPreviewTest::preview_reports_a_missing_interpreter()
{
    // No interpreter is given: nothing is started, and the report says so synchronously.
    PythonIntrospectionRequest request = PythonIntrospectionRequest::forScript(QString(), QStringLiteral("/tmp/whatever.py"));
    PythonSchemaPreview preview;
    int reports = 0;
    PythonIntrospector introspector;
    connect(&introspector, &PythonIntrospector::finished, this, [&](const PythonSchemaPreview& result) {
        preview = result;
        ++reports;
    });
    introspector.start(request);
    QCOMPARE(reports, 1);
    QVERIFY(!introspector.isRunning());
    QVERIFY(!preview.isOk());
    QCOMPARE(preview.diagnostics().first().code, QStringLiteral("interpreter_missing"));

    // An interpreter that does not exist is a different failure: the process was started and failed to start.
    PythonSchemaPreview missing = PythonIntrospector::introspectBlocking(PythonIntrospectionRequest::forScript(
        QDir(_tempDirectory.path()).filePath(QStringLiteral("no-such-interpreter")), QStringLiteral("/tmp/whatever.py")));
    QVERIFY(!missing.isOk());
    QCOMPARE(missing.diagnostics().first().code, QStringLiteral("interpreter_missing"));
    QVERIFY(!missing.details().value(QStringLiteral("commandLine")).toString().isEmpty());
}

void PythonSchemaPreviewTest::preview_reports_a_missing_script()
{
    SKIP_WITHOUT_PYTHON();
    // A build that cannot see the introspection script reports that, rather than probing something else - the same rule
    // the environment probe follows for its own script.
    PythonIntrospectionRequest request = requestFor(interpreter, QStringLiteral("/tmp/target.py"));
    request.setScriptFile(QDir(_tempDirectory.path()).filePath(QStringLiteral("no-such-script.py")));
    PythonSchemaPreview preview = PythonIntrospector::introspectBlocking(request);
    QVERIFY(!preview.isOk());
    QCOMPARE(preview.diagnostics().first().code, QStringLiteral("script_missing"));

    // And a request without a target has nothing to report about.
    PythonIntrospectionRequest noTarget = requestFor(interpreter, QString());
    PythonSchemaPreview empty = PythonIntrospector::introspectBlocking(noTarget);
    QVERIFY(!empty.isOk());
    QCOMPARE(empty.diagnostics().first().code, QStringLiteral("script_missing"));
}

void PythonSchemaPreviewTest::preview_reports_an_answer_that_is_not_a_report()
{
    SKIP_WITHOUT_PYTHON();
    // The interpreter runs and answers JSON, but not a schema report: the version field is what makes it one, and a
    // report this build does not understand is reported as a protocol problem instead of being guessed at.
    const QString script = writeFixture(QStringLiteral("not-a-report.py"), QStringLiteral(
        "import json\n"
        "print(json.dumps({'hello': 'world'}))\n"));
    PythonIntrospectionRequest request = requestFor(interpreter, QStringLiteral("/tmp/target.py"));
    request.setScriptFile(script);
    PythonSchemaPreview preview = PythonIntrospector::introspectBlocking(request);
    QVERIFY(!preview.isOk());
    QCOMPARE(preview.diagnostics().first().code, QStringLiteral("protocol_error"));
    QVERIFY(preview.diagnostics().first().message.contains(QStringLiteral("schemaVersion")));
}

void PythonSchemaPreviewTest::preview_reports_a_timeout()
{
    SKIP_WITHOUT_PYTHON();
    // A script that never answers is a verdict about reading the file, and the interpreter is killed rather than left
    // behind: the next case would otherwise have two of them.
    const QString script = writeFixture(QStringLiteral("slow-report.py"), QStringLiteral(
        "import time\n"
        "time.sleep(60)\n"));
    PythonIntrospectionRequest request = requestFor(interpreter, QStringLiteral("/tmp/target.py"));
    request.setScriptFile(script);
    request.setTimeoutMs(400);

    QElapsedTimer timer;
    timer.start();
    PythonSchemaPreview preview = PythonIntrospector::introspectBlocking(request);
    QVERIFY2(timer.elapsed() < 20000, "The introspection did not stop at its timeout.");
    QVERIFY(!preview.isOk());
    QCOMPARE(preview.diagnostics().first().code, QStringLiteral("timeout"));
    QCOMPARE(preview.details().value(QStringLiteral("timeoutMs")).toInt(), 400);
}

void PythonSchemaPreviewTest::preview_can_be_cancelled()
{
    SKIP_WITHOUT_PYTHON();
    const QString script = writeFixture(QStringLiteral("cancel-report.py"), QStringLiteral(
        "import time\n"
        "time.sleep(60)\n"));

    PythonSchemaPreview preview;
    int reports = 0;
    PythonIntrospector introspector;
    connect(&introspector, &PythonIntrospector::finished, this, [&](const PythonSchemaPreview& result) {
        preview = result;
        ++reports;
    });

    PythonIntrospectionRequest request = requestFor(interpreter, QStringLiteral("/tmp/target.py"));
    request.setScriptFile(script);
    request.setTimeoutMs(60000);
    introspector.start(request);
    QVERIFY(introspector.isRunning());
    QTest::qWait(50);
    QCOMPARE(reports, 0);

    QElapsedTimer timer;
    timer.start();
    introspector.cancel();
    QTRY_VERIFY_WITH_TIMEOUT(reports == 1, 5000);
    QVERIFY2(timer.elapsed() < 5000, "Cancelling the introspection did not stop its interpreter.");
    QCOMPARE(preview.diagnostics().first().code, QStringLiteral("cancelled"));
    QVERIFY(!introspector.isRunning());

    // Cancelling again is harmless, and the report is only improved by the fields the caller asked for.
    introspector.cancel();
    QCOMPARE(reports, 1);
}

void PythonSchemaPreviewTest::preview_emits_exactly_one_report()
{
    SKIP_WITHOUT_PYTHON();
    const QString path = writeFixture(QStringLiteral("one-report.py"), QStringLiteral(
        "@python_script()\n"
        "def modify(a=1):\n"
        "    pass\n"));

    int reports = 0;
    PythonSchemaPreview preview;
    PythonIntrospector introspector;
    connect(&introspector, &PythonIntrospector::finished, this, [&](const PythonSchemaPreview& result) {
        preview = result;
        ++reports;
    });

    // The asynchronous interface starts without waiting for the interpreter.
    QElapsedTimer timer;
    timer.start();
    introspector.start(requestFor(interpreter, path));
    const qint64 startElapsed = timer.elapsed();
    QVERIFY(introspector.isRunning());
    QVERIFY2(startElapsed < 1000, qPrintable(QStringLiteral("start() blocked for %1 ms.").arg(startElapsed)));

    QTRY_VERIFY_WITH_TIMEOUT(reports == 1, 20000);
    QTest::qWait(50);
    QCOMPARE(reports, 1);
    QVERIFY(!introspector.isRunning());
    QVERIFY(preview.isOk());
    QCOMPARE(preview.findFunction(QStringLiteral("modify"))->status, PythonSchemaPreview::Status::Supported);

    // The blocking form of the same request gives the same report, which is what makes the two interfaces one contract.
    PythonSchemaPreview blocking = PythonIntrospector::introspectBlocking(requestFor(interpreter, path));
    QCOMPARE(blocking.isOk(), preview.isOk());
    QCOMPARE(blocking.functions().size(), preview.functions().size());
    QCOMPARE(blocking.path(), preview.path());

    // A second start() while one is running disturbs neither of them.
    PythonIntrospector busy;
    int busyReports = 0;
    connect(&busy, &PythonIntrospector::finished, this, [&](const PythonSchemaPreview&) { ++busyReports; });
    PythonIntrospectionRequest request = requestFor(interpreter, path);
    request.setScriptFile(writeFixture(QStringLiteral("slow-report-2.py"), QStringLiteral("import time\ntime.sleep(60)\n")));
    request.setTimeoutMs(60000);
    busy.start(request);
    QVERIFY(busy.isRunning());
    busy.start(requestFor(interpreter, path));
    QCOMPARE(busyReports, 1);          // the second start reports immediately ...
    QVERIFY(busy.isRunning());         // ... and leaves the running one alone
    busy.cancel();
    QCOMPARE(busyReports, 2);
}

QTEST_MAIN(PythonSchemaPreviewTest)
#include "tst_python_schema_preview.moc"
