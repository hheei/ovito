// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include "CopyableTableView.h"

namespace Ovito {

/******************************************************************************
* Handles key press events for the table widget.
******************************************************************************/
void CopyableTableView::keyPressEvent(QKeyEvent* event)
{
    if(event->matches(QKeySequence::Copy)) {

        QItemSelectionModel* selection = selectionModel();
        QModelIndexList indices = selection->selectedIndexes();

        if(indices.isEmpty())
            return;

        // QModelIndex::operator < sorts first by row, then by column. This is what we need.
        std::sort(indices.begin(), indices.end());

        int lastRow = indices.first().row();
        int lastColumn = indices.first().column();

        QString selectedText;
        for(const QModelIndex& current : indices) {

            if(current.row() != lastRow) {
                selectedText += QLatin1Char('\n');
                lastColumn = indices.first().column();
                lastRow = current.row();
            }

            if(current.column() != lastColumn) {
                for(int i = 0; i < current.column() - lastColumn; ++i)
                    selectedText += QLatin1Char('\t');
                lastColumn = current.column();
            }

            selectedText += model()->data(current).toString();
        }

        selectedText += QLatin1Char('\n');

        QApplication::clipboard()->setText(selectedText);
        event->accept();
    }
    else {
        QTableView::keyPressEvent(event);
    }
}

/******************************************************************************
* Handles key press events for the table widget.
******************************************************************************/
void CopyableTableWidget::keyPressEvent(QKeyEvent* event)
{
    if(event->matches(QKeySequence::Copy)) {

        QItemSelectionModel* selection = selectionModel();
        QModelIndexList indices = selection->selectedIndexes();

        if(indices.isEmpty())
            return;

        // QModelIndex::operator < sorts first by row, then by column. This is what we need.
        std::sort(indices.begin(), indices.end());

        int lastRow = indices.first().row();
        int lastColumn = indices.first().column();

        QString selectedText;
        for(const QModelIndex& current : indices) {

            if(current.row() != lastRow) {
                selectedText += QLatin1Char('\n');
                lastColumn = indices.first().column();
                lastRow = current.row();
            }

            if(current.column() != lastColumn) {
                for(int i = 0; i < current.column() - lastColumn; ++i)
                    selectedText += QLatin1Char('\t');
                lastColumn = current.column();
            }

            selectedText += model()->data(current).toString();
        }

        selectedText += QLatin1Char('\n');

        QApplication::clipboard()->setText(selectedText);
        event->accept();
    }
    else {
        QTableWidget::keyPressEvent(event);
    }
}

}   // End of namespace
