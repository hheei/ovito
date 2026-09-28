// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include "RendererResourceCache.h"

namespace Ovito {

/**
 * Abstract base class for all rendering primitives in OVITO:
 *
 *   - ParticlePrimitive
 *   - CylinderPrimitive
 *   - LinePrimitive
 *   - MeshPrimitive
 *   - TextPrimitive
 *   - ImagePrimitive
 *   - MarkerPrimitive
 *   - VolumePrimitive
 */
class OVITO_CORE_EXPORT RenderingPrimitive
{
public:

	/// Virtual destructor.
	virtual ~RenderingPrimitive() = default;

	/// Computes the 3d bounding box of the primitive in local coordinate space.
	virtual Box3 computeBoundingBox(const RendererResourceCache::ResourceFrame& visCache) const { return Box3(); }

protected:

#ifndef OVITO_BUILD_MONOLITHIC
    // Give this exported c++ class a "key function" to work around dynamic_cast problems (observed on macOS platform).
    // This function is not actually used but ensures that the class' vtable ends up in the core module.
    // See also http://itanium-cxx-abi.github.io/cxx-abi/abi.html#vague-vtable
    virtual void __key_function();
#endif
};

}   // End of namespace
