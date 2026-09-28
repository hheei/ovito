// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * A combo-box widget that displays the current scene node selection
 * and allows to select scene nodes.
 */
class SceneNodeSelectionBox : public QComboBox, public UserInterfaceComponent<MainWindowUI>
{
    Q_OBJECT

public:

    /// Constructs the widget.
    SceneNodeSelectionBox(MainWindowUI& ui, QWidget* parent = nullptr);
};

}   // End of namespace
