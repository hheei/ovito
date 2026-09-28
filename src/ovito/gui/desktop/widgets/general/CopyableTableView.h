// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * \brief A specialized QTableView widget that supports copying the selected contents of the table to the clipboard.
 */
class OVITO_GUI_EXPORT CopyableTableView : public QTableView
{
public:

    /// Constructor.
    CopyableTableView(QWidget* parent = nullptr) : QTableView(parent) {
        setWordWrap(false);
    }

protected:

    /// Handles key press events for this widget.
    virtual void keyPressEvent(QKeyEvent* event) override;
};

/**
 * \brief A specialized QTableWidget widget that supports copying the selected contents of the table to the clipboard.
 */
class OVITO_GUI_EXPORT CopyableTableWidget : public QTableWidget
{
public:

    /// Constructor.
    CopyableTableWidget(QWidget* parent = nullptr) : QTableWidget(parent) {
        setWordWrap(false);
    }

    /// Constructor.
    CopyableTableWidget(int rows, int columns, QWidget* parent = nullptr) : QTableWidget(rows, columns, parent) {
        setWordWrap(false);
    }

protected:

    /// Handles key press events for this widget.
    virtual void keyPressEvent(QKeyEvent* event) override;
};

}   // End of namespace
