// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/dataset/animation/TimeInterval.h>
#include "UnitsManager.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
UnitsManager::UnitsManager()
{
    // Create standard unit objects.
    _units[&FloatParameterUnit::staticMetaObject] = _floatIdentityUnit = new FloatParameterUnit(this);
    _units[&IntegerParameterUnit::staticMetaObject] = _integerIdentityUnit = new IntegerParameterUnit(this);
    _units[&TimeParameterUnit::staticMetaObject] = _timeUnit = new TimeParameterUnit(this);
    _units[&PercentParameterUnit::staticMetaObject] = _percentUnit = new PercentParameterUnit(this);
    _units[&AngleParameterUnit::staticMetaObject] = _angleUnit = new AngleParameterUnit(this);
    _units[&WorldParameterUnit::staticMetaObject] = _worldUnit = new WorldParameterUnit(this);
}

/******************************************************************************
* Returns the global instance of the given parameter unit service.
******************************************************************************/
ParameterUnit* UnitsManager::getUnit(const QMetaObject* parameterUnitClass)
{
    OVITO_CHECK_POINTER(parameterUnitClass);

    if(auto item = _units.find(parameterUnitClass); item != _units.end())
        return item->second;

    // Create an instance of this class.
    ParameterUnit* unit = qobject_cast<ParameterUnit*>(parameterUnitClass->newInstance(Q_ARG(QObject*, this)));
    OVITO_ASSERT_MSG(unit != nullptr, "UnitsManager::getUnit()", "Failed to create instance of requested parameter unit type.");
    _units.insert({ parameterUnitClass, unit });

    return unit;
}

/******************************************************************************
* Converts the given string to a value.
******************************************************************************/
FloatType FloatParameterUnit::parseString(const QString& valueString)
{
    double value;
    bool ok;
    value = valueString.toDouble(&ok);
    if(!ok)
        throw Exception(tr("Invalid floating-point value: %1").arg(valueString));
    return (FloatType)value;
}

/******************************************************************************
* Returns the positive step size used by spinner widgets for this parameter unit type.
******************************************************************************/
FloatType FloatParameterUnit::stepSize(FloatType currentValue, bool upDirection)
{
    int exponent;
    currentValue = nativeToUser(currentValue);
    if(currentValue != 0) {
        exponent = (int)std::floor(std::log10(std::abs(currentValue)) - FloatType(1));
        if(exponent < -12) exponent = -12;
        else if(exponent > 6) exponent = 6;
    }
    else exponent = 0;
    return userToNative(std::pow(FloatType(10), exponent));
}

/******************************************************************************
* Given an arbitrary value, which is potentially invalid, rounds it to the closest valid value.
******************************************************************************/
FloatType FloatParameterUnit::roundValue(FloatType value)
{
    return value;
}

/******************************************************************************
* Converts a numeric value to a string.
******************************************************************************/
QString FloatParameterUnit::formatValue(FloatType value)
{
    return QString::number(value);
}

/******************************************************************************
* Converts the given string to a value.
******************************************************************************/
FloatType IntegerParameterUnit::parseString(const QString& valueString)
{
    int value;
    bool ok;
    value = valueString.toInt(&ok);
    if(!ok)
        throw Exception(tr("Invalid integer value: %1").arg(valueString));
    return (FloatType)value;
}

/******************************************************************************
 * Converts the given string to a value.
 ******************************************************************************/
FloatType AngleParameterUnit::parseString(const QString& valueString)
{
    return FloatParameterUnit::parseString(QString(valueString).remove(QStringLiteral("°")));
}

/******************************************************************************
 * Converts a numeric value to a string.
 ******************************************************************************/
QString AngleParameterUnit::formatValue(FloatType value) { return FloatParameterUnit::formatValue(value) + QStringLiteral("°"); }

/******************************************************************************
* Converts the given string to a value.
******************************************************************************/
FloatType PercentParameterUnit::parseString(const QString& valueString)
{
    return FloatParameterUnit::parseString(QString(valueString).remove(QChar('%')));
}

/******************************************************************************
* Converts a numeric value to a string.
******************************************************************************/
QString PercentParameterUnit::formatValue(FloatType value)
{
    return FloatParameterUnit::formatValue(value) + QStringLiteral("%");
}

/******************************************************************************
* Converts the given string to a time value.
******************************************************************************/
FloatType TimeParameterUnit::parseString(const QString& valueString)
{
    bool ok;
    int frame = valueString.toInt(&ok);
    if(!ok)
        throw Exception(tr("Invalid frame number format: %1").arg(valueString));
    return AnimationTime::fromFrame(frame).ticks();
}

/******************************************************************************
* Converts a time value to a string.
******************************************************************************/
QString TimeParameterUnit::formatValue(FloatType value)
{
    return QString::number(AnimationTime(static_cast<AnimationTime::value_type>(value)).frame());
}

/******************************************************************************
* Returns the (positive) step size used by spinner widgets for this
* parameter unit type.
******************************************************************************/
FloatType TimeParameterUnit::stepSize(FloatType currentValue, bool upDirection)
{
    if(upDirection)
        return std::ceil((currentValue + FloatType(1)) / AnimationTime::TicksPerFrame) * AnimationTime::TicksPerFrame - currentValue;
    else
        return currentValue - std::floor((currentValue - FloatType(1)) / AnimationTime::TicksPerFrame) * AnimationTime::TicksPerFrame;
}

/******************************************************************************
* Given an arbitrary value, which is potentially invalid, rounds it to the
* closest valid value.
******************************************************************************/
FloatType TimeParameterUnit::roundValue(FloatType value)
{
    return std::round(value / AnimationTime::TicksPerFrame) * AnimationTime::TicksPerFrame;
}

}   // End of namespace
