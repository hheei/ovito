// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include "PropertyExpressionEvaluator.h"

namespace Ovito {

/// List of characters allowed in variable names.
/// Note: Keep this list in sync with the regex used in PropertyExpressionRewriter::tokenizeExpression().
mu::string_type PropertyExpressionEvaluator::_validVariableNameChars(_T("0123456789_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.@"));

/******************************************************************************
 * Creates the input variables.
 ******************************************************************************/
void PropertyExpressionEvaluator::initializeInputs(const PipelineFlowState& state, const ConstDataObjectPath& containerPath,
                                                   int animationFrame)
{
    const PropertyContainer* container = static_object_cast<PropertyContainer>(containerPath.back());

    // Build list of properties that will be made available as expression variables.
    std::vector<ConstPropertyPtr> inputProperties;
    for(const Property* property : container->properties()) inputProperties.push_back(property);
    _elementDescriptionName = container->getOOMetaClass().elementDescriptionName();

    // Get simulation cell information.
    const SimulationCell* simCell = state.getObject<SimulationCell>();

    // Determine number of input elements.
    _elementCount = container->elementCount();
    _referencedVariablesKnown = false;
    OVITO_ASSERT(inputProperties.empty() || _elementCount == inputProperties.front()->size());

    // Create list of input variables.
    createInputVariables(inputProperties, simCell, state.buildAttributesMap(), animationFrame);
}

/******************************************************************************
* Specifies the expressions to be evaluated for each element.
******************************************************************************/
void PropertyExpressionEvaluator::initializeExpressions(const QStringList& expressions)
{
    _referencedVariablesKnown = false;

    // Expression parser and rewriter for type-name lookups (Step 2).
    std::optional<PropertyExpressionRewriter::Parser> parser;
    std::optional<PropertyExpressionRewriter::ASTWriter> rewriter;

    // Reset the string comparison map so that variables from a previous initialize() call
    // do not carry over (initialize() may be called multiple times on the same evaluator).
    _stringComparisonMap.clear();

    // Process expressions being passed in.
    _expressions.clear();
    _expressions.reserve(expressions.size());
    for(QString expr : expressions) {

        // Step 0: Rewrite regex match patterns (=~ /pattern/ or =~ /pattern/i).
        // Replaces e.g. 'MyProp =~ /Fe.*/' with '__MyProp_rx_Fe____!=0'.
        if(PropertyExpressionRewriter::expressionNeedsRegexRewrite(expr, _stringPropertyNames)) {
            // Handle string properties.
            expr = PropertyExpressionRewriter::rewriteRegexComparisons(expr, _stringPropertyNames, _stringComparisonMap);
        }
        if(PropertyExpressionRewriter::expressionNeedsRegexRewrite(expr, _typedPropertyNames)) {
            // Handle typed properties.
            expr = PropertyExpressionRewriter::rewriteRegexComparisons(expr, _typedPropertyNames, _stringComparisonMap);
        }

        // Step 1: Token-based rewrite of string property comparisons.
        // Replaces e.g. 'MyProp == "Value"' with '__MyProp_eq_lit_Value__!=0'.
        if(PropertyExpressionRewriter::expressionNeedsStringRewrite(expr, _stringPropertyNames)) {
            expr = PropertyExpressionRewriter::rewriteStringPropertyComparisons(expr, _stringPropertyNames, _stringComparisonMap);
        }

        // Step 2: AST-based rewrite of typed-property type-name comparisons.
        if(PropertyExpressionRewriter::expressionNeedsTypeRewrite(expr)) {
            if(!parser) {
                parser.emplace(_typeMapping);
                rewriter.emplace(_typeMapping);
            }
            const QStringList& tokens = PropertyExpressionRewriter::tokenizeExpression(expr);
            std::unique_ptr<PropertyExpressionRewriter::ASTNode> ast = parser->parse(&expr, &tokens);
            expr = rewriter->write(ast.get());
        }

        _expressions.push_back(convertQString(expr));
    }

    // Step 3: Register one STRING_COMPARISON ExpressionVariable per unique string comparison extracted by the expression preprocessor.
    for(const auto& [varName, spec] : _stringComparisonMap) {
        ExpressionVariable cv;
        cv.type = STRING_COMPARISON;
        cv.name = convertQString(varName);
        cv.variableClass = 0; // Assign string comparisons to variable class 0 so that they will be updated after all other variables have been updated. Class 0 is the last update group.

        // Look up the left property's data in the already-registered variables.
        const mu::string_type leftMangledName = convertQString(spec.leftPropName);
        int leftVarIdx = -1;
        for(ExpressionVariable& v : _variables) {
            if((v.type == STRING_PROPERTY || v.type == INT32_PROPERTY) && v.mangledName == leftMangledName && v.isRegistered) {
                leftVarIdx = &v - _variables.data();
                v.isReferenced = true; // Mark the left property variable as being referenced in a comparison, so that it will be updated during evaluation.
                break;
            }
        }
        OVITO_ASSERT_MSG(leftVarIdx >= 0, "PropertyExpressionEvaluator::initialize",
            "String or typed property variable referenced in comparison not found");
        if(leftVarIdx < 0) continue;

        if(!spec.regexPattern.isEmpty()) {
            // Regex match comparison. Pre-compile the QRegularExpression once.
            QRegularExpression::PatternOptions options;
            if(spec.regexFlags.contains(QChar('i')))
                options |= QRegularExpression::CaseInsensitiveOption;
            QRegularExpression re(spec.regexPattern, options);
            if(!re.isValid())
                throw Exception(tr("Invalid regular expression '%1': %2").arg(spec.regexPattern, re.errorString()));
            cv.function = [re=std::move(re), leftVarIdx](size_t idx, const std::vector<ExpressionVariable>& vars) -> double {
                OVITO_ASSERT(leftVarIdx >= 0 && leftVarIdx < (int)vars.size());
                const ExpressionVariable& leftVar = vars[leftVarIdx];
                if(leftVar.type == STRING_PROPERTY) {
                    OVITO_ASSERT(leftVar.stringValue != nullptr);
                    return re.match(*leftVar.stringValue).hasMatch() ? 1.0 : 0.0;
                }
                else if(leftVar.type == INT32_PROPERTY) {
                    int typeId = static_cast<int>(leftVar.value);
                    if(const ElementType* type = leftVar.propertyRef->elementType(typeId)) {
                        return re.match(type->name()).hasMatch() ? 1.0 : 0.0;
                    }
                    return 0.0;
                }
                else {
                    OVITO_ASSERT(false); // Invalid variable type for regex comparison.
                    return 0.0;
                }
            };
        }
        else if(spec.rightPropName.isEmpty()) {
            // Variable-literal comparison.
            cv.function = [literal=spec.rightLiteral, leftVarIdx](size_t idx, const std::vector<ExpressionVariable>& vars) -> double {
                OVITO_ASSERT(leftVarIdx >= 0 && leftVarIdx < vars.size());
                const ExpressionVariable& leftVar = vars[leftVarIdx];
                OVITO_ASSERT(leftVar.type == STRING_PROPERTY);
                OVITO_ASSERT(leftVar.stringValue != nullptr);
                const QString& val = *leftVar.stringValue; // Access the string property variable's value directly from the stored pointer.
                return (val == literal) ? 1.0 : 0.0;
            };
        }
        else {
            // Variable–variable comparison.
            const mu::string_type rightMangledName = convertQString(spec.rightPropName);
            int rightVarIdx = -1;
            for(ExpressionVariable& v : _variables) {
                if(v.type == STRING_PROPERTY && v.mangledName == rightMangledName && v.isRegistered) {
                    rightVarIdx = &v - _variables.data();
                    v.isReferenced = true; // Mark the right property variable as being referenced in a comparison, so that it will be updated during evaluation.
                    break;
                }
            }
            OVITO_ASSERT_MSG(rightVarIdx >= 0, "PropertyExpressionEvaluator::initialize",
                "Right-hand string property variable referenced in comparison not found");
            if(rightVarIdx < 0) continue;

            cv.function = [leftVarIdx, rightVarIdx](size_t idx, const std::vector<ExpressionVariable>& vars) -> double {
                OVITO_ASSERT(leftVarIdx >= 0 && leftVarIdx < vars.size() && vars[leftVarIdx].type == STRING_PROPERTY);
                OVITO_ASSERT(rightVarIdx >= 0 && rightVarIdx < vars.size() && vars[rightVarIdx].type == STRING_PROPERTY);
                OVITO_ASSERT(vars[leftVarIdx].stringValue != nullptr);
                OVITO_ASSERT(vars[rightVarIdx].stringValue != nullptr);
                const QString& v1 = *vars[leftVarIdx].stringValue;
                const QString& v2 = *vars[rightVarIdx].stringValue;
                return (v1 == v2) ? 1.0 : 0.0;
            };
        }
        addVariable(std::move(cv));
    }
}

/******************************************************************************
* Initializes the list of input variables from the given input state.
******************************************************************************/
void PropertyExpressionEvaluator::createInputVariables(const std::vector<ConstPropertyPtr>& inputProperties, const SimulationCell* simCell, const QVariantMap& attributes, int animationFrame)
{
    // Register the list of expression variables that refer to input properties.
    registerPropertyVariables(inputProperties, 0);

    // Create index variable.
    if(!_indexVarName.isEmpty())
        registerIndexVariable(_indexVarName, 0, tr("zero-based"));

    // Create constant variables.
    ExpressionVariable constVar;

    // Number of elements
    registerGlobalParameter("N", elementCount(), tr("total number of %1").arg(_elementDescriptionName.isEmpty() ? tr("elements") : _elementDescriptionName));

    // Animation frame
    registerGlobalParameter("Frame", animationFrame, tr("animation frame number"));

    // Global attributes
    for(auto entry = attributes.constBegin(); entry != attributes.constEnd(); ++entry) {
        if(entry.value().canConvert<double>())
            registerGlobalParameter(entry.key(), entry.value().toDouble());
        else if(entry.value().canConvert<long>())
            registerGlobalParameter(entry.key(), entry.value().value<long>());
    }

    if(simCell) {
        // Store simulation cell data.
        _simCell = simCell;

        // Cell volume
        registerGlobalParameter("CellVolume", _simCell.is2D() ? _simCell.volume2D() : _simCell.volume3D(), tr("simulation cell volume"));

        // Cell size
        registerGlobalParameter("CellSize.X", std::abs(_simCell.cellMatrix().column(0).x()), tr("size along X"));
        registerGlobalParameter("CellSize.Y", std::abs(_simCell.cellMatrix().column(1).y()), tr("size along Y"));
        registerGlobalParameter("CellSize.Z", std::abs(_simCell.cellMatrix().column(2).z()), tr("size along Z"));
    }

    // Constant pi
    registerConstant("pi", M_PI, QStringLiteral("%1...").arg(M_PI));

    // Constant infinity
    if(std::numeric_limits<FloatType>::has_infinity) {
        registerConstant("inf", std::numeric_limits<FloatType>::infinity(), QStringLiteral("∞"));
    }
}

/******************************************************************************
* Registers the list of expression variables that refer to input properties.
******************************************************************************/
void PropertyExpressionEvaluator::registerPropertyVariables(const std::vector<ConstPropertyPtr>& inputProperties, int variableClass, const mu::char_type* namePrefix)
{
    int propertyIndex = 1;
    for(const ConstPropertyPtr& property : inputProperties) {
        ExpressionVariable v;

        // Determine variable type from the property's data type.
        if(property->dataType() == Property::Int8)
            v.type = INT8_PROPERTY;
        else if(property->dataType() == Property::Int32)
            v.type = INT32_PROPERTY;
        else if(property->dataType() == Property::Int64)
            v.type = INT64_PROPERTY;
        else if(property->dataType() == Property::Float32)
            v.type = FLOAT32_PROPERTY;
        else if(property->dataType() == Property::Float64)
            v.type = FLOAT64_PROPERTY;
        else if(property->dataType() == Property::String)
            v.type = STRING_PROPERTY;
        else
            continue; // Skip properties with unrecognized data types.
        v.variableClass = variableClass;
        v.propertyRef = property;
        v.propertyAccess = property.get();

        // Derive a valid variable name from the property name by removing all invalid characters.
        QString propertyName = property->name();
        // If the name is empty, generate one.
        if(propertyName.isEmpty())
            propertyName = QStringLiteral("Property%1").arg(propertyIndex);
        // If the name starts with a number, prepend an underscore.
        else if(propertyName[0].isDigit())
            propertyName.prepend(QChar('_'));

        for(size_t k = 0; k < property->componentCount(); k++) {

            QString fullPropertyName = propertyName;
            if(property->componentNames().size() == property->componentCount())
                fullPropertyName += QStringLiteral(".") + property->componentNames()[k];
            if(!namePrefix)
                v.name = convertQString(fullPropertyName);
            else
                v.name = namePrefix + convertQString(fullPropertyName);

            // Initialize data pointer into property storage.
            v.dataPointer = v.propertyAccess.cdata(k);
            v.stride = v.propertyAccess.stride();

            // Register variable.
            size_t idx = addVariable(v);

            // Register typed property to types map.
            if(property->isTypedProperty() && _variables[idx].isRegistered) {
                const QString mangledName = convertMuString(_variables[idx].mangledName);
                addTypedPropertyToMap(mangledName, property);
                _typedPropertyNames.insert(mangledName);
            }

            // Register string variable for the expression rewriter.
            if(v.type == STRING_PROPERTY && _variables[idx].isRegistered) {
                _stringPropertyNames.insert(convertMuString(_variables[idx].mangledName));
            }
        }

        propertyIndex++;
    }
}

/******************************************************************************
* Registers an input variable if the name does not exist yet.
******************************************************************************/
size_t PropertyExpressionEvaluator::addVariable(ExpressionVariable v)
{
    // Replace invalid characters in variable name with an underscore.
    v.mangledName.clear();
    v.mangledName.reserve(v.name.size());
    for(char c : v.name) {
        // Remove whitespace from variable names.
        if(c <= ' ') continue;
        // Replace other invalid characters in variable names with an underscore.
        v.mangledName.push_back(_validVariableNameChars.find(c) != mu::string_type::npos ? c : '_');
    }
    if(!v.mangledName.empty()) {
        // Prepend '_' if name starts with number.
        if(v.mangledName[0] >= '0' && v.mangledName[0] <= '9')
            v.mangledName.insert(v.mangledName.begin(), '_');
        // Check if mangled name is unique.
        if(std::none_of(_variables.begin(), _variables.end(), [&v](const ExpressionVariable& v2) -> bool { return v2.mangledName == v.mangledName; })) {
            v.isRegistered = true;
        }
    }
    _referencedVariablesKnown = false;
    _variables.push_back(std::move(v));
    return _variables.size() - 1;
}

/******************************************************************************
 * Registers a typed property with its mangled name and adds it to _typeMapping
 ******************************************************************************/
void PropertyExpressionEvaluator::addTypedPropertyToMap(const QString& mangledPropertyName, const ConstPropertyPtr& property)
{
    // Access or insert mangledName in _typeMapping (outer map)
    auto& omap = _typeMapping[mangledPropertyName];

    // Register names and numeric ids of all types defined for the property.
    // Type names get placed in double quotes to make them compatible with the format used in input expressions.
    // Numeric ids are converted to strings to make them compatible with the format used in output expressions.
    for(const ElementType* type : property->elementTypes()) {
        QString typeIdStr = QString::number(type->numericId());
        QString typeName = QStringLiteral("\"%1\"").arg(type->nameOrNumericId());

        // Access or insert typeName in omap.
        auto& imap = omap[typeName];

        // Append to QStringList
        if(!imap.contains(typeIdStr))
            imap << typeIdStr;
    }
}

/******************************************************************************
* Returns the list of available input variables.
******************************************************************************/
QStringList PropertyExpressionEvaluator::inputVariableNames() const
{
    QStringList vlist;
    for(const ExpressionVariable& v : _variables) {
        if(v.isRegistered)
            vlist << convertMuString(v.mangledName);
    }
    for(const auto& [_, ovalue] : _typeMapping) {
        for(const auto& [ikey, _] : ovalue) {
            if(!vlist.contains(ikey)) vlist << ikey;
        }
    }
    return vlist;
}

/******************************************************************************
* Returns whether a variable is being referenced in one of the expressions.
******************************************************************************/
bool PropertyExpressionEvaluator::isVariableUsed(const mu::char_type* varName)
{
    OVITO_ASSERT(!_expressions.empty()); // This can only called after initializeExpressions() has been called.
    if(!_referencedVariablesKnown) {
        Worker worker(*this);
        // Copy the list of variables back from the Worker, which has already analyzed the expressions and tagged the referenced variables.
        _variables = worker._variables;
        _referencedVariablesKnown = true;
    }
    for(const ExpressionVariable& var : _variables) {
        if(var.name == varName && var.isReferenced)
            return true;
    }
    return false;
}

/******************************************************************************
* Initializes the parser objects of this thread.
******************************************************************************/
PropertyExpressionEvaluator::Worker::Worker(PropertyExpressionEvaluator& evaluator) : _evaluator(evaluator)
{
    _parsers.resize(evaluator._expressions.size());

    // Make a per-thread copy of the input variables.
    _variables = evaluator._variables;

    auto parser = _parsers.begin();
    auto expr = evaluator._expressions.cbegin();
    for(size_t i = 0; i < evaluator._expressions.size(); i++, ++parser, ++expr) {

        if(expr->empty()) {
            if(evaluator._expressions.size() > 1)
                throw Exception(tr("Expression %1 is empty.").arg(i+1));
            else
                throw Exception(tr("Expression is empty."));
        }

        try {

            // Configure parser to accept alpha-numeric characters and '.' in variable names.
            parser->DefineNameChars(_validVariableNameChars.c_str());

            // Define some extra math functions.
            parser->DefineFun(_T("fmod"), static_cast<double (*)(double,double)>(fmod), false);

            // Let the muParser process the math expression.
            parser->SetExpr(*expr);

            // Register input variables.
            // STRING_PROPERTY variables are not numeric muParser variables, so skip them.
            // STRING_COMPARISON variables are standard double variables and are included.
            for(ExpressionVariable& v : _variables) {
                if(v.isRegistered && v.type != STRING_PROPERTY)
                    parser->DefineVar(v.mangledName, &v.value);
            }

            // Query list of variables actually referenced in the expression entered by the user.
            for(const auto& vname : parser->GetUsedVar()) {
                for(ExpressionVariable& var : _variables) {
                    if(var.isRegistered && var.mangledName == vname.first) {
                        // Tag variable as being referenced by at least one expression, so that its value will be updated during evaluation.
                        var.isReferenced = true;
                    }
                }
            }
        }
        catch(mu::Parser::exception_type& ex) {
            throw Exception(convertMuString(ex.GetMsg()));
        }
    }
}

/******************************************************************************
* The worker routine.
******************************************************************************/
void PropertyExpressionEvaluator::Worker::run(size_t startIndex, size_t endIndex, std::function<void(size_t,size_t,double)> callback, std::function<bool(size_t)> filter)
{
    try {
        for(size_t i = startIndex; i < endIndex; i++) {
            if(filter && !filter(i))
                continue;

            for(size_t j = 0; j < _parsers.size(); j++) {
                // Evaluate expression for the current data element.
                callback(i, j, evaluate(i, j));
            }
        }
    }
    catch(const Exception& ex) {
        _errorMsg = ex.message();
    }
}

/******************************************************************************
* The innermost evaluation routine.
******************************************************************************/
double PropertyExpressionEvaluator::Worker::evaluate(size_t elementIndex, size_t component)
{
    OVITO_ASSERT(component < _parsers.size());
    try {
        if(elementIndex != _lastElementIndex) {
            _lastElementIndex = elementIndex;

            // Update variable values for the current data element.
            _evaluator.updateVariables(*this, elementIndex);
        }

        // Evaluate expression for the current data element.
        return _parsers[component].Eval();
    }
    catch(const mu::Parser::exception_type& ex) {
        QString errMsg = convertMuString(ex.GetMsg());

        // Amend error message with a hint if the error is an "Unexpected token" error that may be caused by an invalid use of a string variable in the expression.
        if(errMsg.startsWith("Unexpected token")) {
            const auto& token = ex.GetToken();
            for(const ExpressionVariable& var : _variables) {
                // Check if the error token matches a registered name of a string property.
                // If so, provide a hint that the error may be caused by an invalid use of a string variable in the expression.
                // The expression preprocessor will miss references to string variables (and not replace them) unless they are used in a valid context.
                if(var.mangledName == token && var.type == STRING_PROPERTY) {
                    errMsg += tr(" Note: The string variable '%1' can only be used in equality comparisons (== / !=) with string literals or other string variables, or in regex matches (=~ /pattern/ or =~ /pattern/i).").arg(convertMuString(token));
                    break;
                }
            }
        }
        throw Exception(std::move(errMsg));
    }
}

/******************************************************************************
* Retrieves the value of the variable and stores it in the memory location
* passed to muparser.
******************************************************************************/
void PropertyExpressionEvaluator::ExpressionVariable::updateValue(size_t elementIndex, const std::vector<ExpressionVariable>& allVariables)
{
    if(!isReferenced)
        return;

    switch(type) {
    case FLOAT32_PROPERTY:
        if(elementIndex < propertyAccess.size())
            value = *reinterpret_cast<const float*>(dataPointer + stride * elementIndex);
        break;
    case FLOAT64_PROPERTY:
        if(elementIndex < propertyAccess.size())
            value = *reinterpret_cast<const double*>(dataPointer + stride * elementIndex);
        break;
    case INT8_PROPERTY:
        if(elementIndex < propertyAccess.size())
            value = *reinterpret_cast<const int8_t*>(dataPointer + stride * elementIndex);
        break;
    case INT32_PROPERTY:
        if(elementIndex < propertyAccess.size())
            value = *reinterpret_cast<const int32_t*>(dataPointer + stride * elementIndex);
        break;
    case INT64_PROPERTY:
        if(elementIndex < propertyAccess.size())
            value = *reinterpret_cast<const int64_t*>(dataPointer + stride * elementIndex);
        break;
    case STRING_PROPERTY:
        if(elementIndex < propertyAccess.size())
            stringValue = reinterpret_cast<const QString*>(dataPointer + stride * elementIndex);
        break;
    case STRING_COMPARISON:
        value = function(elementIndex, allVariables);
        break;
    case ELEMENT_INDEX:
        value = elementIndex;
        break;
    case DERIVED_PROPERTY:
        value = function(elementIndex, allVariables);
        break;
    case GLOBAL_PARAMETER:
    case CONSTANT:
        // Nothing to do.
        break;
    }
}

/******************************************************************************
* Returns a human-readable text listing the input variables.
******************************************************************************/
QString PropertyExpressionEvaluator::inputVariableTable() const
{
    QString str(tr("<p><b>Properties:</b><ul>"));
    for(const ExpressionVariable& v : _variables) {
        if((v.type == FLOAT32_PROPERTY || v.type == FLOAT64_PROPERTY || v.type == INT8_PROPERTY || v.type == INT32_PROPERTY || v.type == INT64_PROPERTY || v.type == STRING_PROPERTY || v.type == ELEMENT_INDEX || v.type == DERIVED_PROPERTY) && v.isRegistered && v.variableClass == 0) {
            if(v.description.isEmpty())
                str.append(QStringLiteral("<li>%1</li>").arg(convertMuString(v.mangledName)));
            else
                str.append(QStringLiteral("<li>%1 <span style=\"DESCRIPTION_STYLE_PLACEHOLDER\">(%2)</span></li>").arg(convertMuString(v.mangledName)).arg(v.description));
        }
    }
    str.append(QStringLiteral("</ul></p><p><b>Global values:</b><ul>"));
    for(const ExpressionVariable& v : _variables) {
        if(v.type == GLOBAL_PARAMETER && v.isRegistered) {
            if(v.description.isEmpty())
                str.append(QStringLiteral("<li>%1</li>").arg(convertMuString(v.mangledName)));
            else
                str.append(QStringLiteral("<li>%1 <span style=\"DESCRIPTION_STYLE_PLACEHOLDER\">(%2)</span></li>").arg(convertMuString(v.mangledName)).arg(v.description));
        }
    }
    str.append(QStringLiteral("</ul></p><p><b>Constants:</b><ul>"));
    for(const ExpressionVariable& v : _variables) {
        if(v.type == CONSTANT && v.isRegistered) {
            if(v.description.isEmpty())
                str.append(QStringLiteral("<li>%1</li>").arg(convertMuString(v.mangledName)));
            else
                str.append(QStringLiteral("<li>%1 <span style=\"DESCRIPTION_STYLE_PLACEHOLDER\">(%2)</span></li>").arg(convertMuString(v.mangledName)).arg(v.description));
        }
    }
    str.append(QStringLiteral("</ul></p>"));
    return str;
}

}  // namespace Ovito
