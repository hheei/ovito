// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/automation/AutomationProtocol.h>
#include <ovito/core/automation/AutomationObjectId.h>

#include <QJsonValue>

namespace Ovito {

// Wire type names. A client sees exactly these strings in a descriptor.
const QString AutomationParameter::String = QStringLiteral("string");
const QString AutomationParameter::Integer = QStringLiteral("integer");
const QString AutomationParameter::Number = QStringLiteral("number");
const QString AutomationParameter::Boolean = QStringLiteral("boolean");
const QString AutomationParameter::StringList = QStringLiteral("string-list");
const QString AutomationParameter::ObjectId = QStringLiteral("object-id");
const QString AutomationParameter::Json = QStringLiteral("json");

namespace {

/// Returns whether a value is an integer and nothing else. A double is rejected on purpose: an operation that takes a
/// particle index should not silently truncate `1.7`, and a client that means the number 2 can send 2.
bool isIntegerValue(const QVariant& value)
{
    switch(value.metaType().id()) {
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
        case QMetaType::Short:
        case QMetaType::UShort:
        case QMetaType::Char:
        case QMetaType::UChar:
            return true;
        default:
            return false;
    }
}

/// Returns whether a value is an integer or a floating point number.
bool isNumberValue(const QVariant& value)
{
    return isIntegerValue(value) || value.metaType().id() == QMetaType::Double || value.metaType().id() == QMetaType::Float;
}

/// Returns a value as a double for range checking.
double toDouble(const QVariant& value)
{
    bool ok = false;
    const double result = value.toDouble(&ok);
    OVITO_ASSERT(ok);
    return result;
}

/// Renders a value for an error message in a way that names its type rather than dumping arbitrary content.
QString describeValue(const QVariant& value)
{
    switch(value.metaType().id()) {
        case QMetaType::UnknownType: return QStringLiteral("nothing");
        case QMetaType::QString: return QStringLiteral("the string \"%1\"").arg(value.toString());
        case QMetaType::QStringList: return QStringLiteral("a list of %1 strings").arg(value.toStringList().size());
        default: return QStringLiteral("a value of type %1").arg(QString::fromLatin1(value.typeName()));
    }
}

}   // End of anonymous namespace

/******************************************************************************
* Constructs a parameter.
******************************************************************************/
AutomationParameter::AutomationParameter(QString name, QString type, bool required)
    : _name(std::move(name)), _type(std::move(type)), _required(required)
{
    OVITO_ASSERT(!_name.isEmpty());
    OVITO_ASSERT(!_type.isEmpty());
}

AutomationParameter& AutomationParameter::setDescription(QString description)
{
    _description = std::move(description);
    return *this;
}

AutomationParameter& AutomationParameter::setDefaultValue(QVariant defaultValue)
{
    _defaultValue = std::move(defaultValue);
    return *this;
}

AutomationParameter& AutomationParameter::setUnit(QString unit)
{
    _unit = std::move(unit);
    return *this;
}

AutomationParameter& AutomationParameter::setRange(double minimum, double maximum)
{
    OVITO_ASSERT(minimum <= maximum);
    _minimum = minimum;
    _maximum = maximum;
    return *this;
}

AutomationParameter& AutomationParameter::setAllowedValues(QStringList allowedValues)
{
    _allowedValues = std::move(allowedValues);
    return *this;
}

/******************************************************************************
* Checks one argument value.
******************************************************************************/
QString AutomationParameter::validate(const QVariant& value) const
{
    // The type first: every other rule assumes the value has been understood.
    const QMetaType::Type typeId = static_cast<QMetaType::Type>(value.metaType().id());
    if(_type == String) {
        if(typeId != QMetaType::QString)
            return QStringLiteral("%1: expected a string, got %2").arg(_name, describeValue(value));
    }
    else if(_type == Integer) {
        if(!isIntegerValue(value))
            return QStringLiteral("%1: expected an integer, got %2").arg(_name, describeValue(value));
    }
    else if(_type == Number) {
        if(!isNumberValue(value))
            return QStringLiteral("%1: expected a number, got %2").arg(_name, describeValue(value));
    }
    else if(_type == Boolean) {
        if(typeId != QMetaType::Bool)
            return QStringLiteral("%1: expected a boolean, got %2").arg(_name, describeValue(value));
    }
    else if(_type == StringList) {
        if(typeId != QMetaType::QStringList)
            return QStringLiteral("%1: expected a list of strings, got %2").arg(_name, describeValue(value));
    }
    else if(_type == ObjectId) {
        if(typeId != QMetaType::QString)
            return QStringLiteral("%1: expected an object ID string, got %2").arg(_name, describeValue(value));
        if(!AutomationObjectId::parse(value.toString()))
            return QStringLiteral("%1: \"%2\" is not an object ID").arg(_name, value.toString());
    }
    else if(_type == Json) {
        // Anything goes; the operation documents what it expects.
    }
    else {
        OVITO_ASSERT_MSG(false, "AutomationParameter::validate()", "Unknown parameter type");
    }

    // Then the declared constraints.
    if(!_allowedValues.isEmpty()) {
        if(!_allowedValues.contains(value.toString()))
            return QStringLiteral("%1: \"%2\" is not one of %3").arg(_name, value.toString(), _allowedValues.join(QStringLiteral(", ")));
    }
    if(_minimum || _maximum) {
        const double number = toDouble(value);
        if(_minimum && number < *_minimum)
            return QStringLiteral("%1: %2 is below the minimum %3").arg(_name).arg(number).arg(*_minimum);
        if(_maximum && number > *_maximum)
            return QStringLiteral("%1: %2 is above the maximum %3").arg(_name).arg(number).arg(*_maximum);
    }
    return {};
}

/******************************************************************************
* Returns the descriptor as a JSON-compatible map.
******************************************************************************/
QVariantMap AutomationParameter::toJson() const
{
    QVariantMap json;
    json.insert(QStringLiteral("name"), _name);
    json.insert(QStringLiteral("type"), _type);
    json.insert(QStringLiteral("required"), _required);
    if(!_description.isEmpty())
        json.insert(QStringLiteral("description"), _description);
    if(_defaultValue.isValid())
        json.insert(QStringLiteral("default"), _defaultValue);
    if(!_unit.isEmpty())
        json.insert(QStringLiteral("unit"), _unit);
    if(_minimum)
        json.insert(QStringLiteral("minimum"), *_minimum);
    if(_maximum)
        json.insert(QStringLiteral("maximum"), *_maximum);
    if(!_allowedValues.isEmpty())
        json.insert(QStringLiteral("allowedValues"), _allowedValues);
    return json;
}

/******************************************************************************
* Constructs an operation descriptor.
******************************************************************************/
AutomationOperationDescriptor::AutomationOperationDescriptor(QString id, AutomationContract::OperationKind kind, QString summary)
    : _id(std::move(id)), _kind(kind), _summary(std::move(summary)), _undoLabel(_id)
{
    OVITO_ASSERT(!_id.isEmpty());
    OVITO_ASSERT(!_summary.isEmpty());
}

AutomationOperationDescriptor& AutomationOperationDescriptor::setUndoLabel(QString undoLabel)
{
    OVITO_ASSERT(!undoLabel.isEmpty());
    _undoLabel = std::move(undoLabel);
    return *this;
}

AutomationOperationDescriptor& AutomationOperationDescriptor::addParameter(AutomationParameter parameter)
{
    OVITO_ASSERT(!findParameter(parameter.name()));
    _parameters.push_back(std::move(parameter));
    return *this;
}

AutomationOperationDescriptor& AutomationOperationDescriptor::addRequiredCapability(AutomationContract::Capability capability)
{
    OVITO_ASSERT(!_requiredCapabilities.contains(capability));
    // A query that needs a mutation capability would make a read-only client impossible, so that combination is a
    // programming error rather than a runtime condition. See the phase's exit gate.
    OVITO_ASSERT_MSG(isQuery() == AutomationContract::isReadCapability(capability),
                     "AutomationOperationDescriptor::addRequiredCapability()",
                     "A query must require read capabilities only, and a command must require at least one capability that permits a change.");
    _requiredCapabilities.push_back(capability);
    return *this;
}

/******************************************************************************
* Finds a declared parameter by name.
******************************************************************************/
const AutomationParameter* AutomationOperationDescriptor::findParameter(QStringView name) const
{
    for(const AutomationParameter& parameter : _parameters) {
        if(parameter.name() == name)
            return &parameter;
    }
    return nullptr;
}

/******************************************************************************
* Validates the arguments of a request against the parameter schema.
******************************************************************************/
QStringList AutomationOperationDescriptor::validateArguments(const QVariantMap& arguments) const
{
    QStringList errors;

    // An argument the operation does not declare is an error, not something to ignore: a client that misspells a
    // parameter has to hear about it, or it silently operates on the default value.
    for(auto it = arguments.constBegin(); it != arguments.constEnd(); ++it) {
        if(!findParameter(it.key()))
            errors.push_back(QStringLiteral("%1: this operation has no parameter of that name").arg(it.key()));
    }

    for(const AutomationParameter& parameter : _parameters) {
        const auto it = arguments.constFind(parameter.name());
        if(it == arguments.constEnd()) {
            if(parameter.isRequired() && !parameter.defaultValue().isValid())
                errors.push_back(QStringLiteral("%1: missing required parameter").arg(parameter.name()));
            continue;
        }
        const QString error = parameter.validate(*it);
        if(!error.isEmpty())
            errors.push_back(error);
    }
    return errors;
}

/******************************************************************************
* Returns the descriptor as a JSON-compatible map.
******************************************************************************/
QVariantMap AutomationOperationDescriptor::toJson() const
{
    QVariantMap json;
    json.insert(QStringLiteral("id"), _id);
    json.insert(QStringLiteral("kind"), AutomationContract::kindName(_kind));
    json.insert(QStringLiteral("summary"), _summary);
    json.insert(QStringLiteral("undoLabel"), _undoLabel);
    QVariantList parameters;
    parameters.reserve(_parameters.size());
    for(const AutomationParameter& parameter : _parameters)
        parameters.push_back(parameter.toJson());
    json.insert(QStringLiteral("parameters"), parameters);
    QStringList capabilities;
    capabilities.reserve(_requiredCapabilities.size());
    for(AutomationContract::Capability capability : _requiredCapabilities)
        capabilities.push_back(AutomationContract::capabilityName(capability));
    json.insert(QStringLiteral("requiredCapabilities"), capabilities);
    return json;
}

/******************************************************************************
* Constructs an artifact description.
******************************************************************************/
AutomationArtifact::AutomationArtifact(QString id, QString mediaType, QString operationId)
    : _id(std::move(id)), _mediaType(std::move(mediaType)), _operationId(std::move(operationId))
{
    OVITO_ASSERT(!_id.isEmpty());
    OVITO_ASSERT(!_mediaType.isEmpty());
}

AutomationArtifact& AutomationArtifact::setDimensions(int width, int height)
{
    OVITO_ASSERT(width > 0 && height > 0);
    _width = width;
    _height = height;
    return *this;
}

AutomationArtifact& AutomationArtifact::setFrame(int frame)
{
    _frame = frame;
    return *this;
}

AutomationArtifact& AutomationArtifact::setFilePath(QString filePath)
{
    _filePath = std::move(filePath);
    return *this;
}

QVariantMap AutomationArtifact::toJson() const
{
    QVariantMap json;
    json.insert(QStringLiteral("id"), _id);
    json.insert(QStringLiteral("mediaType"), _mediaType);
    json.insert(QStringLiteral("operationId"), _operationId);
    if(_width > 0 && _height > 0) {
        json.insert(QStringLiteral("width"), _width);
        json.insert(QStringLiteral("height"), _height);
    }
    if(_frame >= 0)
        json.insert(QStringLiteral("frame"), _frame);
    if(!_filePath.isEmpty())
        json.insert(QStringLiteral("filePath"), _filePath);
    return json;
}

AutomationArtifact AutomationArtifact::fromJson(const QVariantMap& json)
{
    AutomationArtifact artifact(json.value(QStringLiteral("id")).toString(),
                                json.value(QStringLiteral("mediaType")).toString(),
                                json.value(QStringLiteral("operationId")).toString());
    if(json.contains(QStringLiteral("width")) && json.contains(QStringLiteral("height")))
        artifact.setDimensions(json.value(QStringLiteral("width")).toInt(), json.value(QStringLiteral("height")).toInt());
    if(json.contains(QStringLiteral("frame")))
        artifact.setFrame(json.value(QStringLiteral("frame")).toInt());
    if(json.contains(QStringLiteral("filePath")))
        artifact.setFilePath(json.value(QStringLiteral("filePath")).toString());
    return artifact;
}

/******************************************************************************
* Constructs a request.
******************************************************************************/
AutomationRequest::AutomationRequest(QString operationId)
    : _operationId(std::move(operationId))
{
}

AutomationRequest& AutomationRequest::setArguments(QVariantMap arguments)
{
    _arguments = std::move(arguments);
    return *this;
}

AutomationRequest& AutomationRequest::setBaseRevision(quint64 baseRevision)
{
    _baseRevision = baseRevision;
    return *this;
}

AutomationRequest& AutomationRequest::setRequestId(QString requestId)
{
    _requestId = std::move(requestId);
    return *this;
}

QVariantMap AutomationRequest::toJson() const
{
    QVariantMap json;
    json.insert(QStringLiteral("operationId"), _operationId);
    if(!_arguments.isEmpty())
        json.insert(QStringLiteral("arguments"), _arguments);
    if(_baseRevision)
        json.insert(QStringLiteral("baseRevision"), QVariant::fromValue<qulonglong>(*_baseRevision));
    if(!_requestId.isEmpty())
        json.insert(QStringLiteral("requestId"), _requestId);
    return json;
}

AutomationRequest AutomationRequest::fromJson(const QVariantMap& json)
{
    AutomationRequest request(json.value(QStringLiteral("operationId")).toString());
    request.setArguments(json.value(QStringLiteral("arguments")).toMap());
    if(json.contains(QStringLiteral("baseRevision")))
        request.setBaseRevision(json.value(QStringLiteral("baseRevision")).toULongLong());
    request.setRequestId(json.value(QStringLiteral("requestId")).toString());
    return request;
}

/******************************************************************************
* Returns a successful result.
******************************************************************************/
AutomationResult AutomationResult::success(QVariantMap data)
{
    AutomationResult result;
    result._success = true;
    result._data = std::move(data);
    return result;
}

/******************************************************************************
* Returns a failed result.
******************************************************************************/
AutomationResult AutomationResult::failure(AutomationContract::ErrorCode code, QString message, QVariantMap details)
{
    AutomationResult result;
    result.setError(code, std::move(message), std::move(details));
    return result;
}

void AutomationResult::setError(AutomationContract::ErrorCode code, QString message, QVariantMap details)
{
    _success = false;
    _errorCode = code;
    _errorMessage = std::move(message);
    _errorDetails = std::move(details);
    // A failed result carries no payload: an operation that half answered a request must not leave values behind that
    // a client could mistake for a complete answer.
    _data.clear();
}

void AutomationResult::addWarning(QString warning)
{
    _warnings.push_back(std::move(warning));
}

void AutomationResult::setTaskId(QString taskId)
{
    _taskId = std::move(taskId);
}

void AutomationResult::setTransactionId(QString transactionId)
{
    _transactionId = std::move(transactionId);
}

void AutomationResult::addArtifact(AutomationArtifact artifact)
{
    _artifacts.push_back(std::move(artifact));
}

/******************************************************************************
* Returns the result as a JSON-compatible map.
******************************************************************************/
QVariantMap AutomationResult::toJson() const
{
    QVariantMap json;
    json.insert(QStringLiteral("ok"), _success);
    json.insert(QStringLiteral("contractVersion"), AutomationContract::version());
    json.insert(QStringLiteral("revision"), QVariant::fromValue<qulonglong>(_revision));
    if(_success) {
        if(!_data.isEmpty())
            json.insert(QStringLiteral("data"), _data);
    }
    else {
        QVariantMap error;
        error.insert(QStringLiteral("code"), AutomationContract::errorCodeName(_errorCode));
        error.insert(QStringLiteral("message"), _errorMessage);
        if(!_errorDetails.isEmpty())
            error.insert(QStringLiteral("details"), _errorDetails);
        json.insert(QStringLiteral("error"), error);
    }
    if(!_warnings.isEmpty())
        json.insert(QStringLiteral("warnings"), _warnings);
    if(!_taskId.isEmpty())
        json.insert(QStringLiteral("taskId"), _taskId);
    if(!_transactionId.isEmpty())
        json.insert(QStringLiteral("transactionId"), _transactionId);
    if(!_artifacts.isEmpty()) {
        QVariantList artifacts;
        artifacts.reserve(_artifacts.size());
        for(const AutomationArtifact& artifact : _artifacts)
            artifacts.push_back(artifact.toJson());
        json.insert(QStringLiteral("artifacts"), artifacts);
    }
    return json;
}

/******************************************************************************
* Restores a result from its JSON form.
******************************************************************************/
AutomationResult AutomationResult::fromJson(const QVariantMap& json)
{
    AutomationResult result;
    result._success = json.value(QStringLiteral("ok"), true).toBool();
    result._revision = json.value(QStringLiteral("revision")).toULongLong();
    if(result._success) {
        result._data = json.value(QStringLiteral("data")).toMap();
    }
    else {
        const QVariantMap error = json.value(QStringLiteral("error")).toMap();
        const QString codeName = error.value(QStringLiteral("code")).toString();
        result._errorCode = AutomationContract::ErrorCode::InternalError;
        for(int i = 0; i <= static_cast<int>(AutomationContract::ErrorCode::InternalError); ++i) {
            const auto code = static_cast<AutomationContract::ErrorCode>(i);
            if(AutomationContract::errorCodeName(code) == codeName) {
                result._errorCode = code;
                break;
            }
        }
        result._errorMessage = error.value(QStringLiteral("message")).toString();
        result._errorDetails = error.value(QStringLiteral("details")).toMap();
    }
    result._warnings = json.value(QStringLiteral("warnings")).toStringList();
    result._taskId = json.value(QStringLiteral("taskId")).toString();
    result._transactionId = json.value(QStringLiteral("transactionId")).toString();
    for(const QVariant& artifact : json.value(QStringLiteral("artifacts")).toList())
        result._artifacts.push_back(AutomationArtifact::fromJson(artifact.toMap()));
    return result;
}

}   // End of namespace
