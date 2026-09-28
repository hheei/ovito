// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * \brief A QListWidget that supports HTML text items.
 */
class OVITO_GUI_EXPORT HtmlListWidget : public QListWidget
{
public:

    /// \brief Constructs a list widget.
    /// \param parent The parent widget for the new widget.
    HtmlListWidget(QWidget* parent = nullptr);

    /// Returns the recommended size for the widget.
    virtual QSize sizeHint() const override {
        return QSize(320, 280);
    }
};

}   // End of namespace


