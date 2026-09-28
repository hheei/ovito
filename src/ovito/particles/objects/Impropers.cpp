// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/Particles.h>
#include "Impropers.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(Impropers);
OVITO_CLASSINFO(Impropers, "DisplayName", "Impropers");
OVITO_CLASSINFO(Impropers, "ClassNameAlias", "ImpropersObject");  // For backward compatibility with OVITO 3.9.2

/******************************************************************************
* Constructor.
******************************************************************************/
void Impropers::initializeObject(ObjectInitializationFlags flags)
{
    PropertyContainer::initializeObject(flags);

    // Assign the default data object identifier.
    setIdentifier(OOClass().pythonName());
}

/******************************************************************************
* Creates a storage object for standard properties.
******************************************************************************/
PropertyPtr Impropers::OOMetaClass::createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type, const ConstDataObjectPath& containerPath) const
{
    int dataType;
    size_t componentCount;

    switch(type) {
    case TypeProperty:
        dataType = Property::Int32;
        componentCount = 1;
        break;
    case TopologyProperty:
    case ParticleIdentifiersProperty:
        dataType = Property::Int64;
        componentCount = 4;
        break;
    default:
        OVITO_ASSERT_MSG(false, "Impropers::createStandardPropertyInternal", "Invalid standard property type");
        throw Exception(tr("This is not a valid improper standard property type: %1").arg(type));
    }
    const QStringList& componentNames = standardPropertyComponentNames(type);
    const QString& propertyName = standardPropertyName(type);

    OVITO_ASSERT(componentCount == standardPropertyComponentCount(type));

    PropertyPtr property = PropertyPtr::create(DataBuffer::Uninitialized, elementCount, dataType, componentCount, propertyName, type, componentNames);

    if(init == DataBuffer::Initialized) {
        // Default-initialize property values with zeros.
        property->fillZero();
    }

    return property;
}

/******************************************************************************
* Registers all standard properties with the property traits class.
******************************************************************************/
void Impropers::OOMetaClass::initialize()
{
    PropertyContainerClass::initialize();

    setPropertyClassDisplayName(tr("Impropers"));
    setElementDescriptionName(QStringLiteral("impropers"));
    setPythonName(QStringLiteral("impropers"));

    const QStringList emptyList;
    const QStringList abcdList = QStringList() << "A" << "B" << "C" << "D";
    const QStringList onetwothreefourList = QStringList() << "1" << "2" << "3" << "4";

    registerStandardProperty(TypeProperty, QStringLiteral("Improper Type"), Property::Int32, emptyList, &ElementType::OOClass(), tr("Improper types"));
    registerStandardProperty(TopologyProperty, QStringLiteral("Topology"), Property::Int64, abcdList);
    registerStandardProperty(ParticleIdentifiersProperty, QStringLiteral("Particle Identifiers"), Property::Int64, onetwothreefourList);
}

}   // End of namespace
