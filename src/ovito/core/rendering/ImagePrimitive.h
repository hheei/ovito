// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include "RenderingPrimitive.h"

namespace Ovito {

/**
 * \brief A 2d image to be rendered by a SceneRenderer implementation.
 */
class OVITO_CORE_EXPORT ImagePrimitive final : public RenderingPrimitive
{
    Q_GADGET

#ifndef OVITO_BUILD_MONOLITHIC
    // Give this exported c++ class a "key function" to work around dynamic_cast problems (observed on macOS platform).
    // This function is not actually used but ensures that the class' vtable ends up in the core module.
    // See also http://itanium-cxx-abi.github.io/cxx-abi/abi.html#vague-vtable
    virtual void __key_function() override;
#endif

public:

    /// Default constructor.
    ImagePrimitive() = default;

    /// Constructor taking an image and a window rectangle.
    ImagePrimitive(const QImage& image, const Box2& windowRect) : _image(image), _windowRect(windowRect) {}

    /// Constructor taking an image and a window rectangle.
    ImagePrimitive(const QImage& image, const QRectF& windowRect) : _image(image) { setRectWindow(windowRect); }

    /// Sets the mage to be rendered.
    void setImage(const QImage& image) { _image = image; }

    /// Sets the mage to be rendered.
    void setImage(QImage&& image) { _image = std::move(image); }

    /// Returns the image stored in the buffer.
    const QImage& image() const { return _image; }

    /// Sets the destination rectangle for rendering the image in window coordinates.
    void setRectWindow(const Box2& rect) { _windowRect = rect; }

    /// Sets the destination rectangle for rendering the image in window coordinates.
    void setRectWindow(const QRectF& rect) { _windowRect.minc = Point2(rect.left(), rect.top()); _windowRect.maxc = Point2(rect.right(), rect.bottom()); }

    /// Returns the destination rectangle in window coordinates.
    const Box2& windowRect() const { return _windowRect; }

private:

    /// The image to be rendered.
    QImage _image;

    /// The destination rectangle in window coordinates.
    Box2 _windowRect;
};

}   // End of namespace
