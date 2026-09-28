// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

/**
 * \file ElidedTextLabel.h
 * \brief Contains the definition of the Ovito::ElidedTextLabel class.
 */

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * \brief A QLabel-like widget that display a line of text, which is shortened if necessary to fit the available space.
 */
class OVITO_GUI_EXPORT ElidedTextLabel : public QLabel
{
    Q_OBJECT

public:

    /// \brief Constructs an empty label.
    /// \param elideMode Controls where the text gets shortened if necessary.
    /// \param parent The parent widget for the new widget.
    /// \param f Flags to be passed to the QLabel constructor.
    ElidedTextLabel(Qt::TextElideMode elideMode, QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags()) : QLabel(parent, f), _elideMode(elideMode) {}

    /// \brief Constructs a label with text.
    /// \param elideMode Controls where the text gets shortened if necessary.
    /// \param text The text string to display.
    /// \param parent The parent widget for the new widget.
    /// \param f Flags to be passed to the QLabel constructor.
    ElidedTextLabel(Qt::TextElideMode elideMode, const QString& string, QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags()) : QLabel(string, parent, f), _elideMode(elideMode) {}

protected:

    /// Returns the area that is available for us to draw the document.
    QRect documentRect() const;

    /// Paints the widget.
    void paintEvent(QPaintEvent *) override;

    /// The elide mode.
    Qt::TextElideMode _elideMode;
};

}   // End of namespace
