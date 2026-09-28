// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/app/Application.h>
#include "TextPrimitive.h"

#include <QTextDocument>
#include <QTextFrame>
#include <QTextFrameFormat>
#include <QAbstractTextDocumentLayout>

namespace Ovito {

/******************************************************************************
* Ensures that the current system has the capability to render text by
* initializing a global Qt application object.
******************************************************************************/
void TextPrimitive::ensureFontRenderingCapability()
{
    try {
        Application::instance()->createQtApplication(false);
    }
    catch(Exception& ex) {
        throw ex.prependGeneralMessage(QStringLiteral("Qt font rendering capability is not available in this system environment."));
    }
}

/******************************************************************************
* Sets the size of a text font to an absolute size given in device-independent pixels.
*
* All font sizes OVITO renders are specified as a fraction of the output image height, i.e. they are
* pixel quantities. They must therefore be assigned to a QFont as a pixel size and never as a point
* size: Qt converts a point size to pixels using the DPI reported by the primary screen, which is 72
* on macOS but typically 96 on Windows and Linux -- and 100 in a session that has no screen at all,
* where Qt falls back to a hardcoded value (see qt_defaultDpis() in Qt's qfont.cpp). Going through a
* point size would therefore render the same scene with differently sized text depending on the
* platform and on whether the image is rendered by the GUI application or by a Python script.
******************************************************************************/
void TextPrimitive::setFontPixelSize(QFont& font, qreal pixelSize)
{
    // Note: QFont::setPixelSize() accepts integer sizes only, and rejects sizes of zero or less.
    font.setPixelSize(std::max(1, qRound(pixelSize)));
}

/******************************************************************************
* Determines whether the text primitive uses rich text formatting or not.
******************************************************************************/
Qt::TextFormat TextPrimitive::resolvedTextFormat() const
{
    Qt::TextFormat format = textFormat();
    if(format == Qt::AutoText)
        format = Qt::mightBeRichText(text()) ? Qt::RichText : Qt::PlainText;
    return format;
}

/******************************************************************************
* Computes the bounds of the text in local coordinates, i.e., in a
* coordinate system that is aligned with the text. The bounds are computed as if
* the text was drawn at (0,0).
* Does NOT take into account the offset position, the rotation or the outline width. The alignment
* flags do enter the measurement of rich text, where they select the paragraph alignment.
******************************************************************************/
QRectF TextPrimitive::queryLocalBounds(qreal devicePixelRatio, Qt::TextFormat textFormatHint) const
{
    ensureFontRenderingCapability();

    QRectF textBounds;
    Qt::TextFormat resolvedTextFormat = textFormat();
    if(resolvedTextFormat == Qt::AutoText) {
        if(textFormatHint != Qt::AutoText) resolvedTextFormat = textFormatHint;
        else resolvedTextFormat = Qt::mightBeRichText(text()) ? Qt::RichText : Qt::PlainText;
    }
#ifndef Q_OS_MACOS
    if(resolvedTextFormat != Qt::RichText && effectiveOutlineWidth(devicePixelRatio) == 0) {
#else
    // Workaround for macOS: When rendering text using the regular QPainter::drawText() method, the font size changes between GUI/CLI mode (for unknown reasons).
    // To avoid this problem, always use the rich-text rendering method in console mode.
    if(resolvedTextFormat != Qt::RichText && effectiveOutlineWidth(devicePixelRatio) == 0 && Application::guiEnabled()) {
#endif
        if(!useTightBox()) {
            textBounds = QFontMetricsF(font()).boundingRect(text());
        }
        else {
            QPainterPath textPath;
            textPath.addText(0, 0, font(), text());
            textBounds = textPath.boundingRect();
        }
        textBounds.moveTo(devicePixelRatio * textBounds.x(), devicePixelRatio * textBounds.y());
        textBounds.setWidth(devicePixelRatio * textBounds.width());
        textBounds.setHeight(devicePixelRatio * textBounds.height());
        // Add 1 pixel of horizontal padding as text bounds to not account for anti aliasing
        // From testing the vertical text bounds seem to be sufficient
        textBounds.adjust(-1, 0, 0, 1);
    }
    else {
        QTextDocument doc;
        doc.setUndoRedoEnabled(false);
        if(resolvedTextFormat == Qt::RichText)
            doc.setHtml(text());
        else
            doc.setPlainText(text());
        doc.setDefaultFont(font());
        doc.setDocumentMargin(0);
        QTextOption opt = doc.defaultTextOption();
        opt.setAlignment(Qt::Alignment(alignment()));
        doc.setDefaultTextOption(opt);
        textBounds = QRectF(QPointF(0,0), devicePixelRatio * doc.size());
    }

    return textBounds;
}

/******************************************************************************
* Cached variant of queryLocalBounds().
*
* The cache key lists every input the measurement actually depends on. Note that the position,
* the rotation and the two colors are deliberately not part of it: they do not influence the local
* text bounds, and leaving them out lets a cached measurement survive a camera movement or a color
* change. The outline width only enters as a boolean, because all it does is select which of the
* two measurement code paths below is taken.
******************************************************************************/
QRectF TextPrimitive::queryLocalBounds(const RendererResourceCache::ResourceFrame& visCache, qreal devicePixelRatio, Qt::TextFormat textFormatHint) const
{
    using TextLocalBoundsCacheKey = RendererResourceKey<struct TextLocalBoundsCache,
        QString,            // The text string
        QString,            // The font key
        qreal,              // The device pixel ratio
        bool,               // Whether the tight bounding box is used
        int,                // The alignment flags
        Qt::TextFormat,     // The text format
        Qt::TextFormat,     // The text format hint
        bool                // Whether an outline is rendered
    >;

    return visCache.lookup<QRectF>(
        TextLocalBoundsCacheKey{text(), font().key(), devicePixelRatio, useTightBox(), alignment(), textFormat(), textFormatHint,
                                effectiveOutlineWidth(devicePixelRatio) != 0},
        [&](QRectF& bounds) {
            bounds = queryLocalBounds(devicePixelRatio, textFormatHint);
        });
}

/******************************************************************************
* Computes the axis-aligned bounding rectangle of the text in the canvas coordinate system.
* This method takes into account text alignment, offset position, rotation, and outline width.
* This overload uses the pre-computed size of the text in the local coordinate system.
******************************************************************************/
QRectF TextPrimitive::computeBounds(const QSizeF textSize, qreal devicePixelRatio) const
{
    QRectF boundingRect(QPointF(0,0), textSize);

    // Apply horizontal alignment.
    if(alignment() & Qt::AlignRight)
        boundingRect.moveLeft(-textSize.width());
    else if(alignment() & Qt::AlignHCenter)
        boundingRect.moveLeft(-textSize.width() / 2);

    // Apply vertical alignment.
    if(alignment() & Qt::AlignBottom)
        boundingRect.moveTop(-textSize.height());
    else if(alignment() & Qt::AlignVCenter)
        boundingRect.moveTop(-textSize.height() / 2);

    // Apply rotation.
    if(rotation() != 0.0) {
        boundingRect = QTransform().rotateRadians(rotation()).mapRect(boundingRect);
    }

    // Apply translation.
    boundingRect.translate(position().x(), position().y());

    // Apply the outline and background margins. The background rectangle is painted into the same
    // image as the text itself, so its padding has to be part of the bounds as well.
    const qreal margin = std::max(effectiveOutlineWidth(devicePixelRatio), effectiveBackgroundMargin());
    boundingRect.adjust(-margin, -margin, margin, margin);

    return boundingRect;
}

/******************************************************************************
* Draws the text (and optional outline) using a QPainter.
******************************************************************************/
void TextPrimitive::draw(QPainter& painter, Qt::TextFormat resolvedTextFormat, qreal textWidth) const
{
    ensureFontRenderingCapability();

#ifndef Q_OS_MACOS
    if(resolvedTextFormat != Qt::RichText && effectiveOutlineWidth() == 0) {
#else
    // Workaround for macOS: When rendering text using the regular QPainter::drawText() method, the font size changes between GUI/CLI mode (for unknown reasons).
    // To avoid this problem, always use the rich-text rendering method in console mode.
    if(resolvedTextFormat != Qt::RichText && effectiveOutlineWidth() == 0 && Application::guiEnabled()) {
#endif
        drawPlainText(painter);
    }
    else {
        drawRichText(painter, resolvedTextFormat, textWidth);
    }
}

/******************************************************************************
* Draws the unformatted text (and optional outline) using a QPainter.
******************************************************************************/
void TextPrimitive::drawPlainText(QPainter& painter) const
{
    OVITO_ASSERT_MSG(this->effectiveOutlineWidth() == 0, "TextPrimitive::drawPlainText()", "Outline rendering is only supported by the drawRichText routine.");

    painter.setFont(font());
    painter.setPen((QColor)color());
    painter.drawText(QPointF(0,0), text());
}

/******************************************************************************
* Draws the formatted text (and optional outline) using a QPainter.
******************************************************************************/
void TextPrimitive::drawRichText(QPainter& painter, Qt::TextFormat resolvedTextFormat, qreal textWidth) const
{
    QTextDocument doc;
    doc.setUndoRedoEnabled(false);
    doc.setDefaultFont(font());
    if(resolvedTextFormat == Qt::RichText)
        doc.setHtml(text());
    else
        doc.setPlainText(text());
    // Remove document margin.
    doc.setDocumentMargin(0);
    // Specify document alignment.
    QTextOption opt = doc.defaultTextOption();
    opt.setAlignment(Qt::Alignment(alignment()));
    doc.setDefaultTextOption(opt);
    doc.setTextWidth(textWidth);
    // When rendering outlined text is requested, apply the outlined text style to the entire document.
    qreal effectiveOutlineWidth = this->effectiveOutlineWidth();
    if(effectiveOutlineWidth != 0) {
        QTextCursor cursor(&doc);
        cursor.select(QTextCursor::Document);
        QTextCharFormat charFormat;
        charFormat.setTextOutline(QPen(QBrush(outlineColor()), 2 * effectiveOutlineWidth));
        doc.setUndoRedoEnabled(true);
        cursor.mergeCharFormat(charFormat);
    }
    QAbstractTextDocumentLayout::PaintContext ctx;
    // Specify default text color:
    ctx.palette.setColor(QPalette::Text, (QColor)color());
    doc.documentLayout()->draw(&painter, ctx);
    // When rendering outlined text, paint the text again on top without the outline
    // in order to make the outline only go outward, not inward into the letters.
    if(effectiveOutlineWidth != 0) {
        doc.undo();
        doc.documentLayout()->draw(&painter, ctx);
    }
}

}   // End of namespace
