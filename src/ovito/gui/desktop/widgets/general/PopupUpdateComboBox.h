// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * \brief A QComboBox widget that emits a signal just before the drop-down popup list is shown.
 */
class OVITO_GUI_EXPORT PopupUpdateComboBox : public QComboBox
{
    Q_OBJECT

public:

    /// Initializes the widget.
    using QComboBox::QComboBox;

    /// Is called just before the drop-down box is activated.
    virtual void showPopup() override {
        Q_EMIT dropDownActivated();
        QComboBox::showPopup();
    }

Q_SIGNALS:

    /// This signal is emited right before the drop-down list of the combobox is displayed.
    void dropDownActivated();
};

}   // End of namespace
