// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include "InfoItemDelegate.h"

namespace Ovito {

/******************************************************************************
* Renders the list items of the pipeline editor and other list views.
******************************************************************************/
void InfoItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    // Render the item exactly like QStyledItemDelegate::paint().
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    QStyle* style = option.widget ? option.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, option.widget);

    if(!(opt.state & QStyle::State_Editing)) {

        // Obtain the value of the info value from the list model.
        if(QVariant info = index.data(infoRole()); info.isValid() && !info.isNull()) {
            painter->save();
            painter->setClipRegion(opt.rect);

            if(info.typeId() == QMetaType::QColor) {
                // Display a QColor as a small filled rectangle.
                QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, option.widget);
                const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, option.widget) + 1;
                const int titleWidth = opt.fontMetrics.horizontalAdvance(opt.text + QStringLiteral("   "));
                QRect rect = textRect.adjusted(textMargin + titleWidth, 6, -textMargin, -6); // remove width padding
                rect.setWidth(rect.height());
                painter->fillRect(rect, info.value<QColor>());
            }
            else if(info.canConvert<QString>()) {
                // Render textual information as a text label with dimmed coloring.
                opt.font = option.widget->font();
                painter->setFont(opt.font);

                // The following is adopted from QCommonStyle::drawControl().
                QPalette::ColorGroup cg = (opt.state & QStyle::State_Enabled) ? QPalette::Normal : QPalette::Disabled;
                if(cg == QPalette::Normal && !(opt.state & QStyle::State_Active))
                    cg = QPalette::Inactive;
                QPalette::ColorRole textRole = (opt.state & QStyle::State_Selected) ? QPalette::HighlightedText : QPalette::Text;
                QPalette::ColorRole backgroundRole = (opt.state & QStyle::State_Selected) ? QPalette::Highlight : QPalette::Window;
                painter->setPen(blendColors(
                    opt.palette.color(cg, textRole),
                    opt.palette.color(cg, backgroundRole),
                    0.75));

                QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, option.widget);
                const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, option.widget) + 1;
                const int titleWidth = opt.fontMetrics.horizontalAdvance(opt.text + QStringLiteral("   "));
                textRect.adjust(textMargin + titleWidth, 0, -textMargin, 0); // remove width padding

                QString text = opt.fontMetrics.elidedText(info.toString(), Qt::ElideRight, textRect.width());
                painter->drawText(textRect, opt.displayAlignment, text);
            }

            painter->restore();
        }
    }
}


}   // End of namespace
