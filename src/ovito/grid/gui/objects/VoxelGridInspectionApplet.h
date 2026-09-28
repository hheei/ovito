// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/stdobj/gui/properties/PropertyInspectionApplet.h>
#include <ovito/grid/objects/VoxelGrid.h>

namespace Ovito {

/**
 * \brief Data inspector page for voxel grid objects.
 */
class VoxelGridInspectionApplet : public PropertyInspectionApplet
{
    OVITO_CLASS(VoxelGridInspectionApplet)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject() { PropertyInspectionApplet::initializeObject(VoxelGrid::OOClass()); }

    /// Returns the key value for this applet that is used for ordering the applet tabs.
    virtual int orderingKey() const override { return 210; }

    /// Lets the applet create the UI widget that is to be placed into the data inspector panel.
    virtual QWidget* createWidget() override;

    /// Returns the help topic ID for the documentation page of this applet.
    virtual QString helpTopicId() const override { return QStringLiteral("manual:data_inspector.voxel_grids"); }

protected:

    /// Determines the text shown in cells of the vertical header column.
    virtual QVariant headerColumnText(int section) override;

private Q_SLOTS:

    /// Is called when the user selects a different property container object in the list.
    void onCurrentContainerChanged(const DataObject* dataObject);

private:

    QLabel* _gridInfoLabel;
};

}   // End of namespace
