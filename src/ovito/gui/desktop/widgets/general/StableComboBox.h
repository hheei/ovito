// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * \brief A QComboBox that provides more stable behavior when the list of items is changed while
 *        the drop-down list is open. The standard QComboBox widget does not handle this situation
 *        well and won't let the user select an item, because the selection is reset when the list
 *        of items is changed.
 */
class OVITO_GUI_EXPORT StableComboBox : public QComboBox
{
    Q_OBJECT

public:

    /// Constructor.
    using QComboBox::QComboBox;

    /// Replaces the list of items.
    void setItems(const QList<std::pair<QString, QVariant>>& itemsWithData);

    /// Replaces the list of items.
    void setItems(std::vector<std::unique_ptr<QStandardItem>> items);

    /// Returns the standard warning icon.
    static const QIcon& warningIcon();
};

}   // End of namespace
