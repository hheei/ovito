// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include "HtmlListWidget.h"

namespace Ovito {

/******************************************************************************
* Constructs a list widget.
******************************************************************************/
HtmlListWidget::HtmlListWidget(QWidget* parent) : QListWidget(parent)
{
    class HtmlItemDelegate : public QStyledItemDelegate {
    protected:
        virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
            QStyleOptionViewItem options = option;
            initStyleOption(&options, index);
            painter->save();
            QTextDocument doc;
            doc.setHtml(options.text);
            options.text.clear();
            options.widget->style()->drawControl(QStyle::CE_ItemViewItem, &options, painter, options.widget);
            // Shift text right to make icon visible
            painter->translate(options.rect.left(), options.rect.top());
            QRect clip(0, 0, options.rect.width(), options.rect.height());
            doc.setTextWidth(clip.width());
            QAbstractTextDocumentLayout::PaintContext ctx;
#ifndef Q_OS_WIN
            // Set text color for highlighted item
            if(option.state & QStyle::State_Selected)
                ctx.palette.setColor(QPalette::Text, options.palette.color(QPalette::Active, QPalette::HighlightedText));
#endif
            ctx.clip = clip;
            doc.documentLayout()->draw(painter, ctx);
            painter->restore();
        }

        virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
            QStyleOptionViewItem options = option;
            initStyleOption(&options, index);
            QTextDocument doc;
            doc.setHtml(options.text);
            doc.setTextWidth(options.rect.width());
            return QSize(doc.idealWidth(), doc.size().height());
        }
    };
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setWordWrap(true);
    setItemDelegate(new HtmlItemDelegate());
}

}   // End of namespace
