// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/Particles.h>
#include "Dihedrals.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(Dihedrals);
OVITO_CLASSINFO(Dihedrals, "DisplayName", "Dihedrals");
OVITO_CLASSINFO(Dihedrals, "ClassNameAlias", "DihedralsObject");  // For backward compatibility with OVITO 3.9.2

/******************************************************************************
* Constructor.
******************************************************************************/
void Dihedrals::initializeObject(ObjectInitializationFlags flags)
{
    PropertyContainer::initializeObject(flags);

    // Assign the default data object identifier.
    setIdentifier(OOClass().pythonName());
}

/******************************************************************************
* Creates a storage object for standard properties.
******************************************************************************/
PropertyPtr Dihedrals::OOMetaClass::createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type, const ConstDataObjectPath& containerPath) const
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
        OVITO_ASSERT_MSG(false, "Dihedrals::createStandardPropertyInternal", "Invalid standard property type");
        throw Exception(tr("This is not a valid dihedral standard property type: %1").arg(type));
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
void Dihedrals::OOMetaClass::initialize()
{
    PropertyContainerClass::initialize();

    setPropertyClassDisplayName(tr("Dihedrals"));
    setElementDescriptionName(QStringLiteral("dihedrals"));
    setPythonName(QStringLiteral("dihedrals"));

    const QStringList emptyList;
    const QStringList abcdList = QStringList() << "A" << "B" << "C" << "D";
    const QStringList onetwothreefourList = QStringList() << "1" << "2" << "3" << "4";

    registerStandardProperty(TypeProperty, QStringLiteral("Dihedral Type"), Property::Int32, emptyList, &ElementType::OOClass(), tr("Dihedral types"));
    registerStandardProperty(TopologyProperty, QStringLiteral("Topology"), Property::Int64, abcdList);
    registerStandardProperty(ParticleIdentifiersProperty, QStringLiteral("Particle Identifiers"), Property::Int64, onetwothreefourList);
}

}   // End of namespace
