// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/automation/python/PythonSchemaPreview.h>

#include <QJsonDocument>

namespace Ovito {

namespace {

/**
 * Reads the parameters of one function.
 *
 * The `kind` of a parameter is kept as the string the script wrote (`positional_or_keyword`, `keyword_only`, ...)
 * rather than mapped to an enum: it is a vocabulary of the Python language that Phase 4 will extend when the typing
 * model arrives, and an enum here would make every addition a change of this class.
 */
QVector<PythonSchemaPreview::Parameter> readParameters(const QVariantList& list)
{
    QVector<PythonSchemaPreview::Parameter> parameters;
    parameters.reserve(list.size());
    for(const QVariant& entry : list) {
        const QVariantMap map = entry.toMap();
        PythonSchemaPreview::Parameter parameter;
        parameter.name = map.value(QStringLiteral("name")).toString();
        parameter.kind = map.value(QStringLiteral("kind")).toString();
        parameter.annotation = map.value(QStringLiteral("annotation")).toString();
        if(map.contains(QStringLiteral("default")) && !map.value(QStringLiteral("default")).isNull()) {
            parameter.defaultValue = map.value(QStringLiteral("default"));
            parameter.hasDefault = true;
        }
        parameter.isRequired = map.value(QStringLiteral("required"), true).toBool();
        parameter.isDynamicDefault = map.value(QStringLiteral("defaultDynamic")).toBool();
        parameters.push_back(std::move(parameter));
    }
    return parameters;
}

}   // namespace

/******************************************************************************
* The vocabulary.
******************************************************************************/
PythonSchemaPreview::Mode PythonSchemaPreview::modeFromName(const QString& name)
{
    if(name == QStringLiteral("import"))
        return Mode::Import;
    return Mode::Ast;
}

QString PythonSchemaPreview::modeName(Mode mode)
{
    return mode == Mode::Import ? QStringLiteral("import") : QStringLiteral("ast");
}

QString PythonSchemaPreview::severityName(Severity severity)
{
    return severity == Severity::Warning ? QStringLiteral("warning") : QStringLiteral("error");
}

QString PythonSchemaPreview::statusName(Status status)
{
    switch(status) {
        case Status::Supported: return QStringLiteral("supported");
        case Status::Unsupported: return QStringLiteral("unsupported");
        case Status::Plain: break;
    }
    return QStringLiteral("plain");
}

/******************************************************************************
* Reading the report.
******************************************************************************/
std::optional<PythonSchemaPreview> PythonSchemaPreview::fromJson(const QVariantMap& json, QString* error)
{
    const auto fail = [error](const QString& reason) -> std::optional<PythonSchemaPreview> {
        if(error)
            *error = reason;
        return std::nullopt;
    };

    // The report format is versioned, and only the version this build knows is read. A newer schema report comes from
    // a newer script, which can only mean a mixed installation; reading it anyway would be guessing.
    if(!json.contains(QStringLiteral("schemaVersion")))
        return fail(QStringLiteral("the report has no \"schemaVersion\" field"));
    const int version = json.value(QStringLiteral("schemaVersion")).toInt();
    if(version != 1)
        return fail(QStringLiteral("the report announces schema version %1, this build reads version 1").arg(version));
    if(!json.contains(QStringLiteral("functions")) || json.value(QStringLiteral("functions")).typeId() != QMetaType::QVariantList)
        return fail(QStringLiteral("the report has no \"functions\" list"));
    if(!json.contains(QStringLiteral("diagnostics")) || json.value(QStringLiteral("diagnostics")).typeId() != QMetaType::QVariantList)
        return fail(QStringLiteral("the report has no \"diagnostics\" list"));

    PythonSchemaPreview preview;
    preview._ok = json.value(QStringLiteral("ok"), true).toBool();
    preview._mode = modeFromName(json.value(QStringLiteral("mode")).toString());
    preview._path = json.value(QStringLiteral("path")).toString();
    preview._moduleDocstring = json.value(QStringLiteral("moduleDocstring")).toString();

    for(const QVariant& entry : json.value(QStringLiteral("functions")).toList()) {
        const QVariantMap map = entry.toMap();
        Function function;
        function.name = map.value(QStringLiteral("name")).toString();
        function.line = map.value(QStringLiteral("line"), -1).toInt();
        function.docstring = map.value(QStringLiteral("docstring")).toString();
        function.isModifier = map.value(QStringLiteral("isModifier")).toBool();
        const QString status = map.value(QStringLiteral("status")).toString();
        if(status == QStringLiteral("supported"))
            function.status = Status::Supported;
        else if(status == QStringLiteral("unsupported"))
            function.status = Status::Unsupported;
        else
            function.status = Status::Plain;
        function.reason = map.value(QStringLiteral("reason")).toString();
        for(const QVariant& decorator : map.value(QStringLiteral("decorators")).toList()) {
            const QVariantMap decoratorMap = decorator.toMap();
            QString name = decoratorMap.value(QStringLiteral("name")).toString();
            if(name.isEmpty()) {
                // A decorator whose name could not be read still has to appear in the list, or the caller would think
                // the function had none. The source text is the honest thing to show.
                name = decoratorMap.value(QStringLiteral("argumentText")).toString();
            }
            if(name.isEmpty())
                name = QStringLiteral("<computed at run time>");
            function.decorators.push_back(name);
        }
        function.parameters = readParameters(map.value(QStringLiteral("parameters")).toList());
        preview._functions.push_back(std::move(function));
    }

    for(const QVariant& entry : json.value(QStringLiteral("imports")).toList()) {
        const QVariantMap map = entry.toMap();
        const QStringList names = map.value(QStringLiteral("names")).toStringList();
        const QString module = map.value(QStringLiteral("module")).toString();
        if(module.isEmpty())
            preview._importedModules.push_back(names.join(QStringLiteral(", ")));
        else
            preview._importedModules.push_back(QStringLiteral("from %1 import %2").arg(module, names.join(QStringLiteral(", "))));
    }

    for(const QVariant& entry : json.value(QStringLiteral("diagnostics")).toList()) {
        const QVariantMap map = entry.toMap();
        Diagnostic diagnostic;
        diagnostic.severity = map.value(QStringLiteral("severity")).toString() == QStringLiteral("warning")
            ? Severity::Warning : Severity::Error;
        diagnostic.code = map.value(QStringLiteral("code")).toString();
        diagnostic.message = map.value(QStringLiteral("message")).toString();
        diagnostic.text = map.value(QStringLiteral("text")).toString();
        diagnostic.line = map.value(QStringLiteral("line"), -1).toInt();
        diagnostic.column = map.value(QStringLiteral("column"), -1).toInt();
        preview._diagnostics.push_back(std::move(diagnostic));
    }

    // The import section is optional: it is absent in the AST mode, which is what the caller asked for, not a defect.
    const QVariant importValue = json.value(QStringLiteral("import"));
    if(importValue.typeId() == QMetaType::QVariantMap) {
        const QVariantMap map = importValue.toMap();
        ImportResult result;
        result.attempted = map.value(QStringLiteral("attempted")).toBool();
        result.loaded = map.value(QStringLiteral("loaded")).toBool();
        const QVariantMap importError = map.value(QStringLiteral("error")).toMap();
        result.errorCode = importError.value(QStringLiteral("code")).toString();
        result.errorType = importError.value(QStringLiteral("type")).toString();
        result.errorMessage = importError.value(QStringLiteral("message")).toString();
        for(const QVariant& frame : importError.value(QStringLiteral("traceback")).toList()) {
            const QVariantMap frameMap = frame.toMap();
            TracebackFrame mapped;
            mapped.file = frameMap.value(QStringLiteral("file")).toString();
            mapped.line = frameMap.value(QStringLiteral("line"), -1).toInt();
            mapped.function = frameMap.value(QStringLiteral("function")).toString();
            mapped.text = frameMap.value(QStringLiteral("text")).toString();
            mapped.fromUserScript = frameMap.value(QStringLiteral("fromUserScript")).toBool();
            result.traceback.push_back(std::move(mapped));
        }
        preview._importResult = std::move(result);
    }
    return preview;
}

QVariantMap PythonSchemaPreview::toJson() const
{
    QVariantMap json;
    json.insert(QStringLiteral("ok"), _ok);
    json.insert(QStringLiteral("schemaVersion"), 1);
    json.insert(QStringLiteral("mode"), modeName(_mode));
    json.insert(QStringLiteral("path"), _path);
    json.insert(QStringLiteral("moduleDocstring"), _moduleDocstring.isEmpty() ? QVariant() : QVariant(_moduleDocstring));

    QVariantList imports;
    for(const QString& module : _importedModules)
        imports.push_back(QVariantMap{{ QStringLiteral("display"), module }});
    json.insert(QStringLiteral("imports"), imports);

    QVariantList functions;
    for(const Function& function : _functions) {
        QVariantList parameters;
        for(const Parameter& parameter : function.parameters) {
            QVariantMap entry;
            entry.insert(QStringLiteral("name"), parameter.name);
            entry.insert(QStringLiteral("kind"), parameter.kind);
            entry.insert(QStringLiteral("annotation"), parameter.annotation);
            entry.insert(QStringLiteral("default"), parameter.hasDefault ? parameter.defaultValue : QVariant());
            entry.insert(QStringLiteral("required"), parameter.isRequired);
            entry.insert(QStringLiteral("defaultDynamic"), parameter.isDynamicDefault);
            parameters.push_back(entry);
        }
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), function.name);
        entry.insert(QStringLiteral("line"), function.line);
        entry.insert(QStringLiteral("docstring"), function.docstring);
        entry.insert(QStringLiteral("decorators"), function.decorators);
        entry.insert(QStringLiteral("isModifier"), function.isModifier);
        entry.insert(QStringLiteral("status"), statusName(function.status));
        entry.insert(QStringLiteral("reason"), function.reason);
        entry.insert(QStringLiteral("parameters"), parameters);
        functions.push_back(entry);
    }
    json.insert(QStringLiteral("functions"), functions);

    QVariantList diagnostics;
    for(const Diagnostic& diagnostic : _diagnostics) {
        diagnostics.push_back(QVariantMap{
            { QStringLiteral("severity"), severityName(diagnostic.severity) },
            { QStringLiteral("code"), diagnostic.code },
            { QStringLiteral("message"), diagnostic.message },
            { QStringLiteral("text"), diagnostic.text.isEmpty() ? QVariant() : QVariant(diagnostic.text) },
            { QStringLiteral("line"), diagnostic.line },
            { QStringLiteral("column"), diagnostic.column },
        });
    }
    json.insert(QStringLiteral("diagnostics"), diagnostics);

    if(_importResult) {
        QVariantList traceback;
        for(const TracebackFrame& frame : _importResult->traceback) {
            traceback.push_back(QVariantMap{
                { QStringLiteral("file"), frame.file },
                { QStringLiteral("line"), frame.line },
                { QStringLiteral("function"), frame.function },
                { QStringLiteral("text"), frame.text },
                { QStringLiteral("fromUserScript"), frame.fromUserScript },
            });
        }
        json.insert(QStringLiteral("import"), QVariantMap{
            { QStringLiteral("attempted"), _importResult->attempted },
            { QStringLiteral("loaded"), _importResult->loaded },
            { QStringLiteral("error"), _importResult->errorCode.isEmpty() ? QVariant() : QVariant(QVariantMap{
                { QStringLiteral("code"), _importResult->errorCode },
                { QStringLiteral("type"), _importResult->errorType },
                { QStringLiteral("message"), _importResult->errorMessage },
                { QStringLiteral("traceback"), traceback },
            }) },
        });
    }

    if(!_details.isEmpty())
        json.insert(QStringLiteral("details"), _details);
    return json;
}

/******************************************************************************
* Queries.
******************************************************************************/
QVector<const PythonSchemaPreview::Function*> PythonSchemaPreview::functionsOfStatus(Status status) const
{
    QVector<const Function*> result;
    for(const Function& function : _functions) {
        if(function.status == status)
            result.push_back(&function);
    }
    return result;
}

const PythonSchemaPreview::Function* PythonSchemaPreview::findFunction(const QString& name) const
{
    for(const Function& function : _functions) {
        if(function.name == name)
            return &function;
    }
    return nullptr;
}

QString PythonSchemaPreview::summary() const
{
    if(!_ok) {
        for(const Diagnostic& diagnostic : _diagnostics) {
            if(diagnostic.severity == Severity::Error)
                return diagnostic.message;
        }
        return QStringLiteral("The schema of \"%1\" could not be read.").arg(_path);
    }
    const int modifiers = functionsOfStatus(Status::Supported).size();
    const int unsupported = functionsOfStatus(Status::Unsupported).size();
    QString summary = QStringLiteral("%1 offers %2 function(s), %3 of them modifiers")
                          .arg(_path)
                          .arg(_functions.size())
                          .arg(modifiers);
    if(unsupported > 0)
        summary += QStringLiteral("; the schema of %1 of them could not be read without running the script").arg(unsupported);
    if(_importResult && !_importResult->loaded)
        summary += QStringLiteral("; importing the file failed");
    return summary + u'.';
}

/******************************************************************************
* Building a report.
******************************************************************************/
PythonSchemaPreview PythonSchemaPreview::failure(const QString& code, const QString& message, Mode mode, QString path)
{
    PythonSchemaPreview preview;
    preview._ok = false;
    preview._mode = mode;
    preview._path = std::move(path);
    Diagnostic diagnostic;
    diagnostic.severity = Severity::Error;
    diagnostic.code = code;
    diagnostic.message = message;
    preview._diagnostics.push_back(std::move(diagnostic));
    return preview;
}

PythonSchemaPreview& PythonSchemaPreview::addDiagnostic(Diagnostic diagnostic)
{
    _diagnostics.push_back(std::move(diagnostic));
    if(_diagnostics.back().severity == Severity::Error)
        _ok = false;
    return *this;
}

PythonSchemaPreview& PythonSchemaPreview::addDetail(QString key, QVariant value)
{
    _details.insert(std::move(key), std::move(value));
    return *this;
}

}   // namespace Ovito
