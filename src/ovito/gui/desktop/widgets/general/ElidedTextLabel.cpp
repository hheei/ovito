// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include "ElidedTextLabel.h"

namespace Ovito {

/******************************************************************************
* Returns the rect that is available for us to draw the document.
******************************************************************************/
QRect ElidedTextLabel::documentRect() const
{
    // The following implementation has been adopted from QLabelPrivate::documentRect().
    QRect cr = contentsRect();
    int m = margin();
    cr.adjust(m, m, -m, -m);
    m = indent();
    if(m < 0 && frameWidth()) // no indent, but we do have a frame
        m = fontMetrics().horizontalAdvance(QLatin1Char('x')) / 2 - margin();
    int align = QStyle::visualAlignment(layoutDirection(), alignment());
    if(m > 0) {
        if (align & Qt::AlignLeft)
            cr.setLeft(cr.left() + m);
        if (align & Qt::AlignRight)
            cr.setRight(cr.right() - m);
        if (align & Qt::AlignTop)
            cr.setTop(cr.top() + m);
        if (align & Qt::AlignBottom)
            cr.setBottom(cr.bottom() - m);
    }
    return cr;
}

/******************************************************************************
* Paints the widget.
******************************************************************************/
void ElidedTextLabel::paintEvent(QPaintEvent *)
{
    QStyle* style = QWidget::style();
    QPainter painter(this);
    QRect cr = documentRect();
    int flags = QStyle::visualAlignment(layoutDirection(), alignment());
    QString elidedText = painter.fontMetrics().elidedText(text(), _elideMode, cr.width(), flags);
    style->drawItemText(&painter, cr, flags, palette(), isEnabled(), elidedText, foregroundRole());

    // Use the label's full text as tool tip.
    if(toolTip() != text())
        setToolTip(text());
}

}   // End of namespace
