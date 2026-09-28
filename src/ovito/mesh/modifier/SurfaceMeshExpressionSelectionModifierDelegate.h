// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/stdmod/modifiers/ExpressionSelectionModifier.h>

namespace Ovito {

/**
 * \brief Delegate for the ExpressionSelectionModifier that operates surface mesh regions.
 */
class SurfaceMeshRegionsExpressionSelectionModifierDelegate : public ExpressionSelectionModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class OOMetaClass : public ExpressionSelectionModifierDelegate::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using ExpressionSelectionModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// Indicates which class of data objects the modifier delegate is able to operate on.
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return SurfaceMeshRegions::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("surface_regions"); }
    };

    OVITO_CLASS_META(SurfaceMeshRegionsExpressionSelectionModifierDelegate, OOMetaClass)
};

}   // End of namespace
