// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include "RolloutContainerLayout.h"

namespace Ovito {

RolloutContainerLayout::~RolloutContainerLayout()
{
    while(!list.isEmpty())
        delete list.takeFirst();
}

void RolloutContainerLayout::addItem(QLayoutItem* item)
{
    list.push_back(item);
}

void RolloutContainerLayout::insertWidget(int index, QWidget* widget)
{
    addChildWidget(widget);
    list.insert(index, new QWidgetItem(widget));
}

void RolloutContainerLayout::setGeometry(const QRect& r)
{
    int y = 0;
    for(QLayoutItem* item : list) {
        QSize itemSize = item->sizeHint();
        item->setGeometry(QRect(r.left(), r.top() + y, r.width(), itemSize.height()));
        y += itemSize.height() + spacing();
    }
}

QSize RolloutContainerLayout::sizeHint() const
{
    QSize size(0,0);
    for(QLayoutItem* item : list) {
        QSize itemSize = item->sizeHint();
        size.rwidth() =qMax(size.width(), itemSize.width());
        size.rheight() += itemSize.height();
    }
    size.rheight() += spacing() * qMax(0, list.size() - 1);
    return size;
}

QSize RolloutContainerLayout::minimumSize() const
{
    QSize size(0,0);
    for(QLayoutItem* item : list) {
        QSize itemSize = item->minimumSize();
        size.rwidth() = qMax(size.width(), itemSize.width());
        size.rheight() += itemSize.height();
    }
    size.rheight() += spacing() * qMax(0, list.size() - 1);
    return size;
}

}   // End of namespace
