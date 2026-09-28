// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/gui/desktop/widgets/general/RolloutContainer.h>

namespace Ovito {

/******************************************************************************
* This panel lets the user edit the properties of some RefTarget derived object.
******************************************************************************/
class OVITO_GUI_EXPORT PropertiesPanel : public RolloutContainer
{
    Q_OBJECT

public:

    /// Constructs the panel.
    explicit PropertiesPanel(MainWindowUI& ui, QWidget* parent = nullptr);

    /// Destructs the panel.
    virtual ~PropertiesPanel();

    /// Returns the target object being edited in the panel.
    RefTarget* editObject() const;

    /// Sets the target object being edited in the panel.
    void setEditObject(RefTarget* editObject, OORef<PropertiesEditor> newEditor = {});

    /// Returns the editor that is responsible for the object being edited.
    PropertiesEditor* editor() const { return _editor; }

public Q_SLOTS:

    /// Close the editor that is currently open.
    void close() { setEditObject(nullptr); }

protected:

    /// The editor for the current object.
    OORef<PropertiesEditor> _editor;
};

}   // End of namespace
