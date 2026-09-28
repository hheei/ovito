// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * \brief A properties editor for the DislocationNetworkObject class.
 */
class DislocationNetworkEditor : public PropertiesEditor
{
    OVITO_CLASS(DislocationNetworkEditor)
    Q_OBJECT

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

protected Q_SLOTS:

    /// Is called when the user has double-clicked on one of the entries in the list widget.
    void onDoubleClickPattern(const QModelIndex& index);

private:

    RefTargetListParameterUI* typesListUI;
};

}   // End of namespace
