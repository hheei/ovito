////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

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

    /// Returns the render thread instance.
    RenderThread* rt() const;

    /// Returns the QRhi instance from the render thread.
    QRhi* rhi() const;

    /// Returns the owner StandardRendererImplementation instance.
    StandardRendererImplementation* impl() const { return _impl; }

private:

    /// The owner of this sub-renderer component.
    StandardRendererImplementation* _impl;
};

}   // End of namespace
