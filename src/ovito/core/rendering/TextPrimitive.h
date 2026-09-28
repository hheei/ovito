// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include "RenderingPrimitive.h"

namespace Ovito {

/**
 * \brief A text string to be rendered by a SceneRenderer implementation.
 */
class OVITO_CORE_EXPORT TextPrimitive final : public RenderingPrimitive
{
    Q_GADGET

#ifndef OVITO_BUILD_MONOLITHIC
    // Give this exported c++ class a "key function" to work around dynamic_cast problems (observed on macOS platform).
    // This function is not actually used but ensures that the class' vtable ends up in the core module.
    // See also http://itanium-cxx-abi.github.io/cxx-abi/abi.html#vague-vtable
    virtual void __key_function() override;
#endif

public:

    /// Sets the text to be rendered.
    void setText(const QString& text) { _text = text; }

    /// Returns the number of vertices stored in the buffer.
    [[nodiscard]] const QString& text() const { return _text; }

    /// Sets the text color.
    void setColor(const ColorA& color) { _color = color; }

    /// Returns the text color.
    [[nodiscard]] const ColorA& color() const { return _color; }

    /// Sets the text outline color.
    void setOutlineColor(const ColorA& color) { _outlineColor = color; }

    /// Returns the text outline color.
    [[nodiscard]] const ColorA& outlineColor() const { return _outlineColor; }

    /// Sets the fill color of the rectangle painted behind the text (nothing is painted if alpha=0).
    /// Note that the background is baked into the rasterized text image, so it does not cost an
    /// additional rendering command per text primitive.
    void setBackgroundColor(const ColorA& color) { _backgroundColor = color; }

    /// Returns the fill color of the rectangle painted behind the text.
    [[nodiscard]] const ColorA& backgroundColor() const { return _backgroundColor; }

    /// Sets the padding between the text and the edge of the background rectangle, in device pixels.
    void setBackgroundMargin(FloatType margin) { _backgroundMargin = std::max(margin, FloatType(0)); }

    /// Returns the padding between the text and the edge of the background rectangle, in device pixels.
    [[nodiscard]] FloatType backgroundMargin() const { return _backgroundMargin; }

    /// Returns the background padding - or 0 if no background color has been set.
    [[nodiscard]] qreal effectiveBackgroundMargin() const
    {
        return backgroundColor().a() > 0.0 ? (qreal)backgroundMargin() : 0.0;
    }

    /// Sets the width of the text outline.
    void setOutlineWidth(FloatType width) { _outlineWidth = std::max(width, FloatType(0)); }

    /// Returns the width of the text outline.
    [[nodiscard]] FloatType outlineWidth() const { return _outlineWidth; }

    /// Returns the width of the text outline multiplied with the device pixel ratio - or 0 if no outline color has been set.
    [[nodiscard]] qreal effectiveOutlineWidth(qreal devicePixelRatio = 1) const
    {
        return outlineColor().a() > 0.0 ? (qreal)outlineWidth() * devicePixelRatio : 0.0;
    }

    /// Sets the text font.
    void setFont(const QFont& font) { _font = font; }

    /// Returns the text font.
    [[nodiscard]] const QFont& font() const { return _font; }

    /// Returns the alignment of the text.
    [[nodiscard]] int alignment() const { return _alignment; }

    /// Sets the alignment of the text.
    void setAlignment(int alignment) { _alignment = alignment; }

    /// Sets the text position in window coordinates.
    void setPositionWindow(const Point2& pos) { _position = pos; }

    /// Sets the text position in window coordinates.
    void setPositionWindow(const QPointF& pos) { _position = Point2(pos.x(), pos.y()); }

    /// Returns the text position in window coordinates.
    [[nodiscard]] const Point2& position() const { return _position; }

    /// Returns whether the tight bounding box of the text is used for alignment.
    bool useTightBox() const { return _tightBox; }

    /// Sets whether the tight bounding box of the text is used for alignment.
    void setUseTightBox(bool use) { _tightBox = use; }

    /// Returns the type of text string (plain or rich text).
    [[nodiscard]] Qt::TextFormat textFormat() const { return _textFormat; }

    /// Determines whether the text primitive uses rich text formatting or not.
    [[nodiscard]] Qt::TextFormat resolvedTextFormat() const;

    /// Sets the type of text string (plain or rich text).
    void setTextFormat(Qt::TextFormat format) { _textFormat = format; }

    // Sets the rotation of the text (angle in radian).
    void setRotation(FloatType angle) { _rotation = angle; }

    // Returns the current rotation angle (in radian).
    FloatType rotation() const { return _rotation; }

    /// Computes the bounds of the text in local coordinates, i.e., in a
    /// coordinate system that is aligned with the text. The bounds are computed as if
    /// the text was drawn at (0,0).
    /// Does NOT take into account the offset position, the rotation or the outline width. Note that the
    /// alignment flags do enter the measurement of rich text, where they select the paragraph alignment.
    [[nodiscard]] QRectF queryLocalBounds(qreal devicePixelRatio, Qt::TextFormat textFormatHint = Qt::AutoText) const;

    /// Cached variant of queryLocalBounds(). Measuring a text string is expensive, because it may
    /// involve laying out a QTextDocument. The result depends only on the text itself, the font and
    /// the layout flags - never on the primitive's position or its colors - which means the
    /// measurement can be reused across frames while the camera is being moved.
    [[nodiscard]] QRectF queryLocalBounds(const RendererResourceCache::ResourceFrame& visCache,
                                          qreal devicePixelRatio,
                                          Qt::TextFormat textFormatHint = Qt::AutoText) const;

    /// Computes the axis-aligned bounding rectangle of the text in the canvas coordinate system.
    /// This method takes into account text alignment, offset position, rotation, and outline width.
    /// This overload uses the pre-computed size of the text in the local coordinate system.
    [[nodiscard]] QRectF computeBounds(const QSizeF textSize, qreal devicePixelRatio) const;

    /// Computes the axis-aligned bounding rectangle of the text in the canvas coordinate system.
    /// This method takes into account text alignment, offset position, rotation, and outline width.
    [[nodiscard]] QRectF computeBounds(qreal devicePixelRatio) const
    {
        return computeBounds(queryLocalBounds(devicePixelRatio).size(), devicePixelRatio);
    }

    /// Computes the axis-aligned bounding rectangle of the text in the canvas coordinate system,
    /// reusing a previously cached measurement of the text if one is available.
    [[nodiscard]] QRectF computeBounds(const RendererResourceCache::ResourceFrame& visCache, qreal devicePixelRatio) const
    {
        return computeBounds(queryLocalBounds(visCache, devicePixelRatio).size(), devicePixelRatio);
    }

    /// Draws the text (and the optional outline) using a QPainter.
    void draw(QPainter& painter, Qt::TextFormat resolvedTextFormat, qreal textWidth) const;

    /// \brief Sets the size of a text font to an absolute size given in device-independent pixels.
    ///
    /// All font sizes OVITO renders are specified as a fraction of the output image height, i.e. they are
    /// pixel quantities. They must therefore be assigned to a QFont as a pixel size and never as a point
    /// size: Qt converts a point size to pixels using the DPI reported by the primary screen, which is 72
    /// on macOS but typically 96 on Windows and Linux -- and 100 in a session that has no screen at all,
    /// where Qt falls back to a hardcoded value (see qt_defaultDpis() in Qt's qfont.cpp). Going through a
    /// point size would therefore render the same scene with differently sized text depending on the
    /// platform and on whether the image is rendered by the GUI application or by a Python script.
    static void setFontPixelSize(QFont& font, qreal pixelSize);

    /// \brief Ensures that the current system has the capability to render text by initializing a global Qt application object.
    ///
    /// This method must be called before any text rendering is attempted. It will throw an exception
    /// if the system does not have the capability to render text.
    static void ensureFontRenderingCapability();

private:

    /// Draws the unformatted text (and optional outline) using a QPainter.
    void drawPlainText(QPainter& painter) const;

    /// Draws the formatted text (and optional outline) using a QPainter.
    void drawRichText(QPainter& painter, Qt::TextFormat resolvedTextFormat, qreal textWidth) const;

    /// The text to be rendered.
    QString _text;

    /// The text color.
    ColorA _color{1,1,1,1};

    /// The text outline color (no outline is rendered if alpha=0).
    ColorA _outlineColor{0,0,0,0};

    /// The width of the text outline.
    FloatType _outlineWidth{2.0};

    /// The fill color of the rectangle painted behind the text (nothing is painted if alpha=0).
    ColorA _backgroundColor{0,0,0,0};

    /// The padding between the text and the edge of the background rectangle, in device pixels.
    FloatType _backgroundMargin{0.0};

    /// The text font.
    QFont _font;

    /// The rendering location in window coordinates.
    Point2 _position = Point2::Origin();

    /// The alignment of the text.
    int _alignment = Qt::AlignLeft | Qt::AlignTop;

    /// Use the tight bounding box of the text for alignment.
    bool _tightBox = false;

    /// The type of text string (plain or rich text).
    Qt::TextFormat _textFormat = Qt::PlainText;

    // Rotation angle (rad units).
    FloatType _rotation{0.0};
};

}   // End of namespace
