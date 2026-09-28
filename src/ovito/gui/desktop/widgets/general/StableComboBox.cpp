// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include "StableComboBox.h"

namespace Ovito {

/******************************************************************************
* Returns the standard warning icon.
******************************************************************************/
const QIcon& StableComboBox::warningIcon()
{
    static QIcon warningIcon(QStringLiteral(":/guibase/mainwin/status/status_warning.svg"));
    return warningIcon;
}

/******************************************************************************
* Replaces the list of items.
******************************************************************************/
void StableComboBox::setItems(const QList<std::pair<QString, QVariant>>& itemsWithData)
{
    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(this->model());
    OVITO_ASSERT(model);

    // Overwrite existing items.
    int oldCount = model->rowCount();
    for(int i = 0; i < itemsWithData.size() && i < oldCount; ++i) {
        QStandardItem* item = model->item(i);
        item->setText(itemsWithData[i].first);
        item->setData(itemsWithData[i].second, Qt::UserRole);
    }

    // Add new items.
    for(int i = oldCount; i < itemsWithData.size(); ++i) {
        QStandardItem* item = new QStandardItem(itemsWithData[i].first);
        item->setData(itemsWithData[i].second, Qt::UserRole);
        model->insertRow(i, item);
    }

    // Remove excess items from model.
    for(int i = oldCount - 1; i >= itemsWithData.size(); --i)
        model->removeRow(i);
}

/******************************************************************************
* Replaces the list of items.
******************************************************************************/
void StableComboBox::setItems(std::vector<std::unique_ptr<QStandardItem>> items)
{
    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(this->model());
    OVITO_ASSERT(model);

    // Overwrite existing items.
    int oldCount = model->rowCount();
    for(int i = 0; i < (int)items.size() && i < oldCount; ++i) {
        model->setItem(i, items[i].release());
    }

    // Add new items.
    for(int i = oldCount; i < (int)items.size(); ++i) {
        model->insertRow(i, items[i].release());
    }

    // Remove excess items from model.
    for(int i = oldCount - 1; i >= (int)items.size(); --i)
        model->removeRow(i);
}

}   // End of namespace
