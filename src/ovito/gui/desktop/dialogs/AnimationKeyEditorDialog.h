// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/oo/RefTargetListener.h>
#include <ovito/gui/desktop/properties/ParameterUI.h>

namespace Ovito {

/**
 * This dialog box allows to edit the animation keys of an animatable parameter.
 */
class AnimationKeyEditorDialog : public QDialog, public UserInterfaceComponent<MainWindowUI>, private UndoableTransaction, private UndoSuspender
{
    Q_OBJECT

public:

    /// Constructor.
    AnimationKeyEditorDialog(KeyframeController* ctrl, const PropertyFieldDescriptor* propertyField, QWidget* parent, MainWindowUI& ui);

    /// Returns the animation controller being edited.
    KeyframeController* ctrl() const { return _ctrl.target(); }

private Q_SLOTS:

    /// Event handler for the Ok button.
    void onOk();

    /// Handles the 'Add key' button.
    void onAddKey();

    /// Handles the 'Delete key' button.
    void onDeleteKey();

private:

    QTableView* _tableWidget;
    QAbstractTableModel* _model;
    QAction* _addKeyAction;
    QAction* _deleteKeyAction;
    RefTargetListener<KeyframeController> _ctrl;
    PropertiesPanel* _keyPropPanel;

    friend class AnimationKeyModel;
};

}   // End of namespace
