// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/******************************************************************************
* A push button that allows the user to open a modifier's output in the data
* inspector panel.
******************************************************************************/
class OVITO_GUI_EXPORT OpenDataInspectorButton : public QPushButton
{
    Q_OBJECT

public:

    /// Constructor.
    OpenDataInspectorButton(PropertiesEditor* editor, const QString& buttonTitle, const QStringView objectNameHint = {}, const QVariant& modeHint = {});

    /// Returns the properties editor hosting this button.
    PropertiesEditor* editor() const { return _editor; }

private:

    /// The properties editor hosting this button.
    PropertiesEditor* _editor;

    /// Data object identifier hint to be passed to the data inspector when the button is clicked.
    const QString _objectIdentifierHint;

    /// Mode hint to be passed to the data inspector when the button is clicked.
    const QVariant _modeHint;
};

}   // End of namespace
