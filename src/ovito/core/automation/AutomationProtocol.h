// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/automation/AutomationContract.h>

namespace Ovito {

/**
 * \brief One parameter of an automation operation: its name, its type, and what makes a value acceptable.
 *
 * The descriptor of an operation carries one of these per parameter, which is what lets a client - or an agent that
 * has never seen OVITO - discover how to call an operation and lets the gateway reject a bad call with a specific
 * message instead of failing somewhere inside a modifier.
 *
 * Types are wire types, not C++ types. Use the constants below; a client sees the same names.
 */
class OVITO_CORE_EXPORT AutomationParameter
{
public:

    /// Wire type names.
    static const QString String;
    static const QString Integer;
    static const QString Number;
    static const QString Boolean;
    static const QString StringList;
    static const QString ObjectId;
    static const QString Json;

    /// Constructs a parameter. `required` means the caller has to pass it; an optional parameter without a default
    /// value is simply absent from the arguments when the caller omits it.
    AutomationParameter(QString name, QString type, bool required = true);

    // Fluent setup, so a descriptor reads as one expression.
    AutomationParameter& setDescription(QString description);
    AutomationParameter& setDefaultValue(QVariant defaultValue);
    AutomationParameter& setUnit(QString unit);
    AutomationParameter& setRange(double minimum, double maximum);
    AutomationParameter& setAllowedValues(QStringList allowedValues);

    const QString& name() const { return _name; }
    const QString& type() const { return _type; }
    bool isRequired() const { return _required; }
    const QString& description() const { return _description; }
    const QVariant& defaultValue() const { return _defaultValue; }
    const QString& unit() const { return _unit; }
    const std::optional<double>& minimum() const { return _minimum; }
    const std::optional<double>& maximum() const { return _maximum; }
    const QStringList& allowedValues() const { return _allowedValues; }

    /**
     * \brief Checks one argument value.
     * \return An empty string when the value is acceptable, otherwise a message naming the parameter and the reason.
     */
    QString validate(const QVariant& value) const;

    /// Returns the descriptor as a JSON-compatible map, the form a client sees.
    QVariantMap toJson() const;

private:

    QString _name;
    QString _type;
    bool _required = true;
    QString _description;
    QVariant _defaultValue;
    QString _unit;
    std::optional<double> _minimum;
    std::optional<double> _maximum;
    QStringList _allowedValues;
};

/**
 * \brief Describes one operation of the automation gateway: what it does, what it needs, and how it is called.
 *
 * A descriptor is data. It is registered at startup, served to clients as part of the catalog, and used by the
 * gateway to validate a request before the operation's implementation runs. An operation that is declared but not
 * implemented (a capability this phase assigns to a later phase) is registered without a handler and answers with
 * NotSupported, so a client can enumerate what will exist without being able to rely on it.
 */
class OVITO_CORE_EXPORT AutomationOperationDescriptor
{
public:

    AutomationOperationDescriptor(QString id, AutomationContract::OperationKind kind, QString summary);

    AutomationOperationDescriptor& addParameter(AutomationParameter parameter);
    AutomationOperationDescriptor& addRequiredCapability(AutomationContract::Capability capability);

    /**
     * \brief Sets the human-readable name a command's changes appear under on the undo stack.
     *
     * A command is dispatched as one transaction, so what its handler changes becomes one undo step whose text is this
     * label - which is why it is a short imperative phrase and not the operation ID. A frontend that shows undo
     * history in a translated user interface sets a translated string; without one the operation ID is used, which is
     * wrong for a user but never empty.
     */
    AutomationOperationDescriptor& setUndoLabel(QString undoLabel);

    const QString& id() const { return _id; }
    AutomationContract::OperationKind kind() const { return _kind; }
    const QString& summary() const { return _summary; }

    /// What the command's changes are called on the undo stack; defaults to the operation ID.
    const QString& undoLabel() const { return _undoLabel; }
    const QVector<AutomationParameter>& parameters() const { return _parameters; }
    const QVector<AutomationContract::Capability>& requiredCapabilities() const { return _requiredCapabilities; }

    /// Returns whether this operation only reads the session.
    bool isQuery() const { return _kind == AutomationContract::OperationKind::Query; }

    /// Finds a declared parameter by name; nothing when the operation does not have one.
    const AutomationParameter* findParameter(QStringView name) const;

    /**
     * \brief Validates the arguments of a request against the parameter schema.
     * \return One message per problem: an unknown argument name, a missing required parameter, a wrong type, a value
     *         outside the declared range or not among the allowed values. Empty when the arguments are acceptable.
     */
    QStringList validateArguments(const QVariantMap& arguments) const;

    /// Returns the descriptor as a JSON-compatible map.
    QVariantMap toJson() const;

private:

    QString _id;
    AutomationContract::OperationKind _kind;
    QString _summary;
    QString _undoLabel;
    QVector<AutomationParameter> _parameters;
    QVector<AutomationContract::Capability> _requiredCapabilities;
};

/// Metadata of an output an operation produced. The request that produced it, and what it is.
class OVITO_CORE_EXPORT AutomationArtifact
{
public:

    AutomationArtifact() = default;
    AutomationArtifact(QString id, QString mediaType, QString operationId);

    AutomationArtifact& setDimensions(int width, int height);
    AutomationArtifact& setFrame(int frame);
    AutomationArtifact& setFilePath(QString filePath);

    const QString& id() const { return _id; }
    const QString& mediaType() const { return _mediaType; }
    const QString& operationId() const { return _operationId; }
    int width() const { return _width; }
    int height() const { return _height; }
    int frame() const { return _frame; }

    /// The file the artifact was written to, if any. Empty for an in-memory artifact, which is all this phase
    /// produces: a bounded image buffer is returned to the client and nothing is written to disk.
    const QString& filePath() const { return _filePath; }

    QVariantMap toJson() const;
    static AutomationArtifact fromJson(const QVariantMap& json);

private:

    QString _id;
    QString _mediaType;
    QString _operationId;
    int _width = 0;
    int _height = 0;
    int _frame = -1;
    QString _filePath;
};

/**
 * \brief One call of an automation operation.
 *
 * Carries the operation ID, its arguments and - for a client that has looked at the session before - the revision its
 * decision was based on. The gateway rejects a request whose base revision is no longer current rather than applying
 * it to a scene whose meaning has changed underneath the client.
 */
class OVITO_CORE_EXPORT AutomationRequest
{
public:

    AutomationRequest() = default;
    explicit AutomationRequest(QString operationId);

    AutomationRequest& setArguments(QVariantMap arguments);
    AutomationRequest& setBaseRevision(quint64 baseRevision);
    AutomationRequest& setRequestId(QString requestId);

    const QString& operationId() const { return _operationId; }
    const QVariantMap& arguments() const { return _arguments; }
    const std::optional<quint64>& baseRevision() const { return _baseRevision; }
    const QString& requestId() const { return _requestId; }

    QVariantMap toJson() const;
    static AutomationRequest fromJson(const QVariantMap& json);

private:

    QString _operationId;
    QVariantMap _arguments;
    std::optional<quint64> _baseRevision;
    QString _requestId;
};

/**
 * \brief The structured answer to an AutomationRequest.
 *
 * Every answer carries the contract version and the session revision it was computed from, so a client can tell
 * whether its view is still current. A failure carries a machine-readable error code, a message for a human, and
 * - where the code is about something the client asked for - details that name it (the missing capabilities, the
 * validation errors, the current revision). A success carries the operation's payload, warnings, the IDs of the task
 * and the transaction it created, and the artifacts it produced.
 */
class OVITO_CORE_EXPORT AutomationResult
{
public:

    AutomationResult() = default;

    /// Returns a successful result, optionally with the payload of the operation.
    static AutomationResult success(QVariantMap data = {});

    /// Returns a failed result.
    static AutomationResult failure(AutomationContract::ErrorCode code, QString message, QVariantMap details = {});

    /**
     * \brief Turns this result into a failure, keeping the revision and the warnings it already carries.
     *
     * This is how a request is answered when it cannot be served: the result has already been given the revision it
     * was computed from - and a caller may have added warnings - so replacing it with a bare error would throw that
     * away. `failure()` is the factory for a result that starts as a failure, this is the in-place form the gateway
     * and the operations use.
     */
    void setError(AutomationContract::ErrorCode code, QString message, QVariantMap details = {});

    bool isSuccess() const { return _success; }
    bool isError() const { return !_success; }

    AutomationContract::ErrorCode errorCode() const { return _errorCode; }
    const QString& errorMessage() const { return _errorMessage; }
    const QVariantMap& errorDetails() const { return _errorDetails; }

    const QVariantMap& data() const { return _data; }
    QVariantMap& data() { return _data; }

    quint64 revision() const { return _revision; }
    void setRevision(quint64 revision) { _revision = revision; }

    const QStringList& warnings() const { return _warnings; }
    void addWarning(QString warning);

    const QString& taskId() const { return _taskId; }
    void setTaskId(QString taskId);
    const QString& transactionId() const { return _transactionId; }
    void setTransactionId(QString transactionId);

    const QVector<AutomationArtifact>& artifacts() const { return _artifacts; }
    void addArtifact(AutomationArtifact artifact);

    QVariantMap toJson() const;
    static AutomationResult fromJson(const QVariantMap& json);

private:

    bool _success = true;
    AutomationContract::ErrorCode _errorCode = AutomationContract::ErrorCode::InternalError;
    QString _errorMessage;
    QVariantMap _errorDetails;
    QVariantMap _data;
    quint64 _revision = 0;
    QStringList _warnings;
    QString _taskId;
    QString _transactionId;
    QVector<AutomationArtifact> _artifacts;
};

}   // End of namespace
