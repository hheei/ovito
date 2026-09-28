// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "OpenDataInspectorButton.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
OpenDataInspectorButton::OpenDataInspectorButton(PropertiesEditor* editor, const QString& buttonTitle, const QStringView objectIdentifierHint, const QVariant& modeHint)
    : QPushButton(buttonTitle), _editor(editor), _objectIdentifierHint(objectIdentifierHint.toString()), _modeHint(modeHint)
{
    connect(this, &QPushButton::clicked, [this]() {
        if(!this->editor()->modificationNode() || !this->editor()->modificationNode()->modifier() || !this->editor()->modificationNode()->modifier()->isEnabled()) {
            QToolTip::showText(mapToGlobal(QPoint(0, height()/2)), tr("No results available, because modifier is turned off."),
                this->editor()->container(), this->editor()->container()->rect(), 3000);
        }
        else if(!this->editor()->ui().mainWindow()->openDataInspector(this->editor()->modificationNode(), _objectIdentifierHint, _modeHint)) {
            QToolTip::showText(mapToGlobal(QPoint(0, height()/2)), tr("Results not available yet. Try again later."),
                this->editor()->container(), this->editor()->container()->rect(), 3000);
        }
        else {
            QToolTip::hideText();
        }
    });
}

}   // End of namespace
