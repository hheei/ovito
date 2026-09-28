// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/mesh/Mesh.h>
#include <ovito/mesh/surface/SurfaceMesh.h>
#include <ovito/stdmod/modifiers/ColorCodingModifier.h>

namespace Ovito {

/**
 * \brief Delegate function for the ColorCodingModifier that operates on surface mesh vertices.
 */
class OVITO_MESHMOD_EXPORT SurfaceMeshVerticesColorCodingModifierDelegate : public ColorCodingModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class OOMetaClass : public ColorCodingModifierDelegate::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using ColorCodingModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// Indicates which class of data objects the modifier delegate is able to operate on.
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return SurfaceMeshVertices::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("surface_vertices"); }
    };

    OVITO_CLASS_META(SurfaceMeshVerticesColorCodingModifierDelegate, OOMetaClass)

protected:

    /// \brief returns the ID of the standard property that will receive the computed colors.
    virtual int outputColorPropertyId() const override { return SurfaceMeshVertices::ColorProperty; }
};

/**
 * \brief Delegate function for the ColorCodingModifier that operates on surface mesh faces.
 */
class OVITO_MESHMOD_EXPORT SurfaceMeshFacesColorCodingModifierDelegate : public ColorCodingModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class OOMetaClass : public ColorCodingModifierDelegate::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using ColorCodingModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// Indicates which class of data objects the modifier delegate is able to operate on.
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return SurfaceMeshFaces::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("surface_faces"); }
    };

    OVITO_CLASS_META(SurfaceMeshFacesColorCodingModifierDelegate, OOMetaClass)

protected:

    /// \brief returns the ID of the standard property that will receive the computed colors.
    virtual int outputColorPropertyId() const override { return SurfaceMeshFaces::ColorProperty; }
};

/**
 * \brief Delegate function for the ColorCodingModifier that operates on surface mesh regions.
 */
class OVITO_MESHMOD_EXPORT SurfaceMeshRegionsColorCodingModifierDelegate : public ColorCodingModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class OOMetaClass : public ColorCodingModifierDelegate::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using ColorCodingModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// Indicates which class of data objects the modifier delegate is able to operate on.
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return SurfaceMeshRegions::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("surface_regions"); }
    };

    OVITO_CLASS_META(SurfaceMeshRegionsColorCodingModifierDelegate, OOMetaClass)

protected:

    /// \brief returns the ID of the standard property that will receive the computed colors.
    virtual int outputColorPropertyId() const override { return SurfaceMeshRegions::ColorProperty; }
};

}   // End of namespace
