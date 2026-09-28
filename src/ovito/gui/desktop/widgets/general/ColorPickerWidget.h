// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

/**
 * \file ColorPickerWidget.h
 * \brief Contains the definition of the Ovito::ColorPickerWidget class.
 */

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * \brief A UI control lets the user choose a color.
 */
class OVITO_GUI_EXPORT ColorPickerWidget : public QAbstractButton
{
    Q_OBJECT

public:

    /// \brief Constructs the color picker control.
    /// \param parent The parent widget for the widget.
    ColorPickerWidget(QWidget* parent = 0);

    /// \brief Gets the current value of the color picker.
    /// \return The current color.
    /// \sa setColor()
    const Color& color() const { return _color; }

    /// \brief Sets the current value of the color picker.
    /// \param newVal The new color value.
    /// \param emitChangeSignal Controls whether the control should emit
    ///                         a colorChanged() signal when \a newVal is
    ///                         not equal to the old color.
    /// \sa color()
    void setColor(const Color& newVal, bool emitChangeSignal = false);

    /// Returns the preferred size of the widget.
    virtual QSize sizeHint() const override;

    /// Indicates whether the widget is in read-only mode.
    bool isReadOnly() const { return _isReadOnly; }

    /// Sets the read-only mode of the widget.
    /// In read-only mode, the user cannot change the color value.
    void setReadOnly(bool readOnly) {
        if(readOnly != _isReadOnly) {
            _isReadOnly = readOnly;
            update();
        }
    }

Q_SIGNALS:

    /// \brief This signal is emitted by the color picker after its value has been changed by the user.
    void colorChanged();

protected Q_SLOTS:

    /// \brief Is called when the user has clicked on the color picker control.
    ///
    /// This will open the color selection dialog.
    void activateColorPicker();

protected:

    /// Paints the widget.
    virtual void paintEvent(QPaintEvent* event) override;

    /// The currently selected color.
    Color _color;

    /// Indicates whether the widget is in read-only mode.
    bool _isReadOnly = false;
};

}   // End of namespace


