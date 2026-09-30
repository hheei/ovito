// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>

#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <QVector>

#include <optional>

namespace Ovito {

/**
 * \brief What a Python script offers, as reported by `ovito_schema.py` - read from the source, not from running it.
 *
 * This is the wire-side value type of the Python preview: the script prints one JSON object, `PythonIntrospector`
 * parses it into this class, and everything the application needs to decide about a script is here. It has no state
 * beyond that report and no connection to an interpreter of its own, which is why it can be built, compared and
 * serialized in a test that never starts one.
 *
 * Four rules of the design are visible in the shape of this type rather than in a comment somewhere else:
 *
 *  - **Diagnostics are the failure channel.** A syntax error, a file that cannot be read and an interpreter that died
 *    before answering are all reported the same way: `diagnostics()` gains one entry whose `code` says which of them
 *    it was (the script produces `syntax_error`, `unreadable_file`, `internal_error` and warns with
 *    `dynamic_decorator_argument`; the introspector adds `interpreter_missing`, `interpreter_failed`, `timeout`,
 *    `protocol_error`, `script_missing` and `cancelled`). A caller therefore never has to look at an exit code, and a
 *    frontend has exactly one thing to render.
 *  - **Unreadable is a verdict, not an omission.** A modifier whose decorator or whose parameter defaults cannot be
 *    read off the syntax tree is reported with `Status::Unsupported` and a sentence saying why. It is never dropped
 *    and never guessed, because a schema that is *wrong* is worse than one that is missing.
 *  - **Annotations stay text.** `annotation` is the source text the author wrote (`"float"`, `"List[int]"`); resolving
 *    it would mean executing the module, which the preview promises not to do. Phase 4 owns the typing model.
 *  - **The import result is optional and explicit.** `importResult()` is present only when the caller asked for the
 *    import mode, and then it says whether the module loaded, and - if it did not - the mapped traceback
 *    (`TracebackFrame::fromUserScript` marks the frames that belong to the script itself).
 *
 * `isOk()` is the script's own verdict about the file: true when a schema was read, false when the file has a syntax
 * error or could not be read at all. It is not "the interpreter ran".
 */
class OVITO_CORE_EXPORT PythonSchemaPreview
{
public:

    /// How the script was read. The two modes differ in kind, not in thoroughness (see the class comment).
    enum class Mode {
        /// The syntax tree only: nothing in the file ran, not even an import or a decorator call.
        Ast,
        /// The syntax tree, plus an explicit `importlib` load of the file - which does run its top-level code.
        Import,
    };

    /// How bad a diagnostic is. Everything the preview cannot do is at least a warning.
    enum class Severity {
        Error,
        Warning,
    };

    /// What a listed function is, from the caller's point of view.
    enum class Status {
        /// An ordinary function: no schema decorator, so it is not an entry point a client may select.
        Plain,
        /// A modifier whose declared schema was read completely.
        Supported,
        /// A modifier (or a function whose decorators cannot be told apart from one) whose schema could not be read.
        Unsupported,
    };

    /// One parameter of a function signature, read from the syntax tree.
    struct Parameter
    {
        QString name;
        /// The Python parameter kind: `positional_only`, `positional_or_keyword`, `keyword_only`, `var_positional`,
        /// `var_keyword`.
        QString kind;
        /// The annotation as written, or empty. Never resolved to a type object.
        QString annotation;
        /// The default value when it could be read without running anything.
        QVariant defaultValue;
        bool hasDefault = false;
        /// A parameter without a readable default is required - the same rule the operation schema uses.
        bool isRequired = true;
        /// The default is computed at run time, so `defaultValue` is empty and the parameter counts as required.
        bool isDynamicDefault = false;
    };

    /// One top-level function of the script.
    struct Function
    {
        QString name;
        int line = -1;
        QString docstring;
        /// The decorators as written, innermost last, in the order Python applies them.
        QStringList decorators;
        /// Whether a schema decorator was recognized among them.
        bool isModifier = false;
        Status status = Status::Plain;
        /// Why the schema could not be read. Empty unless `status` is `Unsupported`.
        QString reason;
        QVector<Parameter> parameters;
    };

    /// One problem with the file, or with reading it.
    struct Diagnostic
    {
        Severity severity = Severity::Error;
        /// The machine-readable kind: `syntax_error`, `unreadable_file`, `dynamic_decorator_argument`, ...
        QString code;
        QString message;
        /// The offending source line, when there is one.
        QString text;
        int line = -1;
        int column = -1;
    };

    /// One frame of a traceback the import mode mapped.
    struct TracebackFrame
    {
        QString file;
        int line = -1;
        QString function;
        QString text;
        /// Whether the frame belongs to the script the caller asked about, rather than to the machinery that ran it.
        bool fromUserScript = false;
    };

    /// What the import mode found. Present only when the caller asked for it.
    struct ImportResult
    {
        bool attempted = false;
        bool loaded = false;
        QString errorCode;
        QString errorType;
        QString errorMessage;
        QVector<TracebackFrame> traceback;
    };

    /// The wire names of the enums, which are part of the report format.
    static Mode modeFromName(const QString& name);
    static QString modeName(Mode mode);
    static QString severityName(Severity severity);
    static QString statusName(Status status);

    bool isOk() const { return _ok; }
    Mode mode() const { return _mode; }
    const QString& path() const { return _path; }
    const QString& moduleDocstring() const { return _moduleDocstring; }
    const QVector<Function>& functions() const { return _functions; }
    const QVector<Diagnostic>& diagnostics() const { return _diagnostics; }

    /// The modules the script named in its import statements, in the script's order, for display.
    const QStringList& importedModules() const { return _importedModules; }

    /// The functions of a given status, which is what a frontend shows as "the modifiers this script offers".
    QVector<const Function*> functionsOfStatus(Status status) const;

    /// The named function, or null.
    const Function* findFunction(const QString& name) const;

    /// The import result, when the caller asked for the import mode.
    const std::optional<ImportResult>& importResult() const { return _importResult; }

    /// One sentence for a status bar or a log line, derived from the report rather than from a second guess.
    QString summary() const;

    /// The wire form, identical to what the script printed (plus whatever an introspector failure added).
    QVariantMap toJson() const;

    /// Parses the script's answer. Returns nothing when the object is not a schema report, with `error` naming what
    /// was missing - the introspector turns that into a `protocol_error` diagnostic.
    static std::optional<PythonSchemaPreview> fromJson(const QVariantMap& json, QString* error = nullptr);

    /**
     * \brief A report that says the schema could not be read, for the introspector's own failures.
     *
     * `code` comes from the introspector's documented vocabulary (`interpreter_missing`, `interpreter_failed`,
     * `timeout`, `protocol_error`, `script_missing`, `cancelled`), and the result is `isOk() == false` with exactly
     * one error diagnostic, so a caller sees no difference in shape between "the file is broken" and "the interpreter
     * never ran".
     */
    static PythonSchemaPreview failure(const QString& code, const QString& message, Mode mode = Mode::Ast,
                                      QString path = {});

    /// Adds a diagnostic. The introspector uses it to annotate a report it did receive.
    PythonSchemaPreview& addDiagnostic(Diagnostic diagnostic);

    /// Adds the detail that made a report unusable, without pretending it was a report about the file.
    PythonSchemaPreview& addDetail(QString key, QVariant value);

    /// The structured details of the exchange (command line, exit code, stderr, ...), for a CLI's JSON mode.
    const QVariantMap& details() const { return _details; }

private:

    bool _ok = false;
    Mode _mode = Mode::Ast;
    QString _path;
    QString _moduleDocstring;
    QStringList _importedModules;
    QVector<Function> _functions;
    QVector<Diagnostic> _diagnostics;
    std::optional<ImportResult> _importResult;
    QVariantMap _details;
};

}   // namespace Ovito

Q_DECLARE_METATYPE(Ovito::PythonSchemaPreview)
