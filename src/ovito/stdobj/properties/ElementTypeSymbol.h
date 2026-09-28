// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdobj/StdObj.h>
#include <ovito/core/dataset/data/mesh/TriangleMesh.h>

namespace Ovito {

/**
 * \brief Describes a 3d shape that visually represents an ElementType, for example in a color legend.
 *
 * The descriptor is renderer-agnostic and value-comparable, which allows consumers to use it both as
 * a source of geometry and as a cache key. A default-constructed instance means that the element type
 * has no 3d representation and should be displayed as a flat color box instead.
 */
struct ElementTypeSymbol
{
    /// The kinds of shapes an element type can be represented by.
    enum class Shape
    {
        None,
        Sphere,
        Box,
        Circle,
        Square,
        Cylinder,
        Spherocylinder,
        Mesh
    };

    /// The kind of shape representing the element type.
    Shape shape = Shape::None;

    /// The half-extent of the shape in world units. All scaling factors of the visual element are already applied.
    /// For Mesh shapes this is the uniform scaling factor applied to the polyhedral shape, not its actual extent.
    FloatType radius = 0;

    /// The extent of the shape along its axis (Cylinder and Spherocylinder shapes only).
    FloatType length = 0;

    /// The color of the shape.
    Color color{1,1,1};

    /// The polyhedral geometry of the shape (Mesh shapes only).
    DataOORef<const TriangleMesh> mesh;

    /// Controls the rendering of sharp edges of the polyhedral shape (Mesh shapes only).
    bool emphasizeEdges = false;

    /// Controls the culling of back-facing polygons of the polyhedral shape (Mesh shapes only).
    bool cullFaces = true;

    /// Controls whether the mesh's own face colors are used instead of the color field (Mesh shapes only).
    bool useMeshColor = false;

    /// Returns whether this descriptor represents a renderable 3d shape.
    explicit operator bool() const { return shape != Shape::None && radius > 0; }

    /// Returns the radius of a sphere centered at the shape's origin which encloses the entire shape.
    /// It is used to compute a common scaling factor for a group of symbols that are rendered with a shared camera.
    FloatType boundingRadius() const {
        switch(shape) {
        case Shape::None:
            return 0;
        case Shape::Box:
        case Shape::Square:
            // Half the space diagonal of a cube with edge length 2*radius.
            return radius * FloatType(1.7320508075688772);
        case Shape::Cylinder:
            // Distance from the center of the cylinder to the rim of one of its end caps.
            return std::sqrt(radius * radius + FloatType(0.25) * length * length);
        case Shape::Spherocylinder:
            return FloatType(0.5) * length + radius;
        case Shape::Mesh: {
            if(!mesh || mesh->faceCount() == 0)
                return 0;
            const Box3& bb = mesh->boundingBox();
            if(bb.isEmpty())
                return 0;
            return radius * std::max((bb.maxc - Point3::Origin()).length(), (bb.minc - Point3::Origin()).length());
        }
        default:
            return radius;
        }
    }

    /// Compares two symbol descriptors for equality.
    /// Note that meshes are compared by identity, which is sufficient, because data objects in OVITO are
    /// copy-on-write, i.e., two references to the same object always have the same contents.
    bool operator==(const ElementTypeSymbol& other) const = default;
};

}   // End of namespace
