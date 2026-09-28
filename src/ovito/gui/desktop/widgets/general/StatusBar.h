// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

/**
 * \file StatusBar.h
 * \brief Contains the definition of the Ovito::StatusBar class.
 */

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * \brief A status bar widget.
 */
class StatusBar : public QLabel
{
    Q_OBJECT

public:

    /// Constructor.
    StatusBar(QWidget* parent = nullptr);

    QLabel* overflowWidget() { return _overflowLabel; }

    virtual QSize sizeHint() const override;
    virtual QSize minimumSizeHint() const override { return sizeHint(); }

    QString currentMessage() const { return text(); }

public Q_SLOTS:

    /// Displays the given message for the specified number of milli-seconds
    void showMessage(const QString& message, int timeout = 0);

    /// Removes any message being shown.
    void clearMessage();

protected:

    virtual void resizeEvent(QResizeEvent* event) override;

private:

    QTimer* _timer = nullptr;
    QLabel* _overflowLabel = nullptr;
    mutable int _preferredHeight = 0;
};

}   // End of namespace
