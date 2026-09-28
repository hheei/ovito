// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>

namespace Ovito {

class StandardRendererImplementation; // Forward declaration to avoid circular include

/**
 * Base class for sub-renderers for different primitive types.
 * Used by StandardRendererImplementation.
 */
class OVITO_CORE_EXPORT PrimitiveRenderer
{
public:

    /// Constructor.
    explicit PrimitiveRenderer(StandardRendererImplementation* impl) : _impl(impl) {}

    /// Returns the renderer service that supplies this renderer's GPU resources.
    RendererService* service() const;

    /// Returns the QRhi instance from the render thread.
    QRhi* rhi() const;

    /// Returns the owner StandardRendererImplementation instance.
    StandardRendererImplementation* impl() const { return _impl; }

private:

    /// The owner of this sub-renderer component.
    StandardRendererImplementation* _impl;
};

}   // End of namespace
