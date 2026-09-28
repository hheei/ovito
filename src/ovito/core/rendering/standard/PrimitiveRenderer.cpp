// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include "PrimitiveRenderer.h"
#include "StandardRendererImplementation.h"

namespace Ovito {

/*******************************************************************************
* Returns the renderer service that supplies this renderer's GPU resources.
*******************************************************************************/
RendererService* PrimitiveRenderer::service() const
{
    return _impl->service();
}

/*******************************************************************************
* Returns the QRhi instance from the render thread.
*******************************************************************************/
QRhi* PrimitiveRenderer::rhi() const
{
    return _impl->rhi();
}

}   // End of namespace
