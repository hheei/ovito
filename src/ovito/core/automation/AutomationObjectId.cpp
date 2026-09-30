// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/automation/AutomationObjectId.h>

namespace Ovito {

namespace {

/// Returns the kind prefix of an object kind, for example "pipeline".
QString kindPrefix(AutomationObjectId::Kind kind)
{
    switch(kind) {
        case AutomationObjectId::Kind::SceneNode: return QStringLiteral("scenenode");
        case AutomationObjectId::Kind::Pipeline: return QStringLiteral("pipeline");
        case AutomationObjectId::Kind::Modifier: return QStringLiteral("modifier");
        case AutomationObjectId::Kind::Viewport: return QStringLiteral("viewport");
        case AutomationObjectId::Kind::Property: return QStringLiteral("property");
    }
    OVITO_ASSERT(false);
    return {};
}

/// Returns the single letter that stands for an object kind inside a local name.
QChar kindLetter(AutomationObjectId::Kind kind)
{
    switch(kind) {
        case AutomationObjectId::Kind::SceneNode: return QLatin1Char('s');
        case AutomationObjectId::Kind::Pipeline: return QLatin1Char('p');
        case AutomationObjectId::Kind::Modifier: return QLatin1Char('m');
        case AutomationObjectId::Kind::Viewport: return QLatin1Char('v');
        case AutomationObjectId::Kind::Property: break;
    }
    OVITO_ASSERT_MSG(false, "kindLetter()", "Only object kinds have a local name.");
    return {};
}

/// Parses the number of a local name ("p42" with the letter p) and rejects a leading zero, which would give one
/// object two textual forms.
std::optional<quint64> parseLocalNumber(QStringView text, QChar letter)
{
    if(text.size() < 2 || text.front() != letter)
        return std::nullopt;
    const QStringView digits = text.sliced(1);
    if(digits.front() == QLatin1Char('0'))
        return std::nullopt;
    bool ok = false;
    const quint64 number = digits.toULongLong(&ok);
    if(!ok || number == 0)
        return std::nullopt;
    return number;
}

/// Parses the local name of an object ("p42", "m108", "v2").
std::optional<std::pair<AutomationObjectId::Kind, quint64>> parseLocalName(QStringView text)
{
    if(auto number = parseLocalNumber(text, kindLetter(AutomationObjectId::Kind::SceneNode)))
        return std::make_pair(AutomationObjectId::Kind::SceneNode, *number);
    if(auto number = parseLocalNumber(text, kindLetter(AutomationObjectId::Kind::Pipeline)))
        return std::make_pair(AutomationObjectId::Kind::Pipeline, *number);
    if(auto number = parseLocalNumber(text, kindLetter(AutomationObjectId::Kind::Modifier)))
        return std::make_pair(AutomationObjectId::Kind::Modifier, *number);
    if(auto number = parseLocalNumber(text, kindLetter(AutomationObjectId::Kind::Viewport)))
        return std::make_pair(AutomationObjectId::Kind::Viewport, *number);
    return std::nullopt;
}

}   // End of anonymous namespace

/******************************************************************************
* Constructs an ID of an object kind.
******************************************************************************/
AutomationObjectId AutomationObjectId::forSceneNode(quint64 number)
{
    AutomationObjectId id;
    id._kind = Kind::SceneNode;
    id._number = number;
    return id;
}

AutomationObjectId AutomationObjectId::forPipeline(quint64 number)
{
    AutomationObjectId id;
    id._kind = Kind::Pipeline;
    id._number = number;
    return id;
}

AutomationObjectId AutomationObjectId::forModifier(quint64 number)
{
    AutomationObjectId id;
    id._kind = Kind::Modifier;
    id._number = number;
    return id;
}

AutomationObjectId AutomationObjectId::forViewport(quint64 number)
{
    AutomationObjectId id;
    id._kind = Kind::Viewport;
    id._number = number;
    return id;
}

/******************************************************************************
* Constructs the ID of a property field of an object.
******************************************************************************/
AutomationObjectId AutomationObjectId::forProperty(Kind ownerKind, quint64 ownerNumber, QString fieldName)
{
    OVITO_ASSERT_MSG(ownerKind != Kind::Property, "AutomationObjectId::forProperty()", "A property has no properties.");
    OVITO_ASSERT(!fieldName.isEmpty() && !fieldName.contains(QLatin1Char(':')) && !fieldName.contains(QLatin1Char('/')));
    AutomationObjectId id;
    id._kind = Kind::Property;
    id._ownerKind = ownerKind;
    id._number = ownerNumber;
    id._fieldName = std::move(fieldName);
    return id;
}

/******************************************************************************
* Parses the textual form.
******************************************************************************/
std::optional<AutomationObjectId> AutomationObjectId::parse(const QString& text)
{
    const qsizetype separator = text.indexOf(QLatin1Char(':'));
    if(separator <= 0)
        return std::nullopt;
    const QStringView prefix = QStringView(text).left(separator);
    const QStringView rest = QStringView(text).sliced(separator + 1);

    for(Kind kind : { Kind::SceneNode, Kind::Pipeline, Kind::Modifier, Kind::Viewport }) {
        if(prefix != kindPrefix(kind))
            continue;
        const std::optional<quint64> number = parseLocalNumber(rest, kindLetter(kind));
        if(!number)
            return std::nullopt;
        AutomationObjectId id;
        id._kind = kind;
        id._number = *number;
        return id;
    }

    if(prefix == kindPrefix(Kind::Property)) {
        const qsizetype slash = rest.lastIndexOf(QLatin1Char('/'));
        if(slash <= 0 || slash == rest.size() - 1)
            return std::nullopt;
        const QStringView ownerLocalName = rest.left(slash);
        const QStringView fieldName = rest.sliced(slash + 1);
        if(fieldName.contains(QLatin1Char(':')) || fieldName.contains(QLatin1Char('/')))
            return std::nullopt;
        const std::optional<std::pair<Kind, quint64>> owner = parseLocalName(ownerLocalName);
        if(!owner)
            return std::nullopt;
        return forProperty(owner->first, owner->second, fieldName.toString());
    }
    return std::nullopt;
}

/******************************************************************************
* Returns the textual form.
******************************************************************************/
QString AutomationObjectId::toString() const
{
    if(!isValid())
        return {};
    if(_kind == Kind::Property)
        return QStringLiteral("property:%1/%2").arg(localName(_ownerKind, _number), _fieldName);
    return QStringLiteral("%1:%2").arg(kindPrefix(_kind), localName(_kind, _number));
}

/******************************************************************************
* Returns the local part of an object ID.
******************************************************************************/
QString AutomationObjectId::localName(Kind kind, quint64 number)
{
    OVITO_ASSERT_MSG(kind != Kind::Property, "AutomationObjectId::localName()", "A property is named by its owner.");
    if(number == 0)
        return {};
    return QStringLiteral("%1%2").arg(kindLetter(kind)).arg(number);
}

}   // End of namespace
