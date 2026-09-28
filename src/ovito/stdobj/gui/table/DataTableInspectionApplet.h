// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/stdobj/gui/widgets/DataTablePlotWidget.h>
#include <ovito/stdobj/gui/properties/PropertyInspectionApplet.h>

namespace Ovito {

/**
 * \brief Data inspector page for data tables and 2d data plots.
 */
class OVITO_STDOBJGUI_EXPORT DataTableInspectionApplet : public PropertyInspectionApplet
{
    OVITO_CLASS(DataTableInspectionApplet)

public:

    /// Constructor.
    void initializeObject() { PropertyInspectionApplet::initializeObject(DataTable::OOClass()); }

    /// Returns the key value for this applet that is used for ordering the applet tabs.
    virtual int orderingKey() const override { return 200; }

    /// Lets the applet create the UI widget that is to be placed into the data inspector panel.
    virtual QWidget* createWidget() override;

    /// Returns the plotting widget.
    DataTablePlotWidget* plotWidget() const { return _plotWidget; }

    /// Selects a specific data object in this applet.
    virtual bool selectDataObject(const PipelineNode* createdByNode, const QStringView objectIdentifierHint, const QVariant& modeHint) override;

    /// Determines whether the given property represents a color.
    virtual bool isColorProperty(const Property* property) const override {
        return (property->dataType() == Property::Float32 || property->dataType() == Property::Float64) && property->componentCount() == 3 && property->name().contains(QStringLiteral("Color"));
    }

    /// Creates an optional ad-hoc property that serves as header column for the table.
    virtual ConstPropertyPtr createHeaderColumnProperty(const PropertyContainer* container) override;

    /// Returns the UI display name of the elements contained in the selected property container, e.g. "particles" or "bonds".
    virtual QString elementDescriptionName() const override { return tr("rows"); }

    /// Returns the help topic ID for the documentation page of this applet.
    virtual QString helpTopicId() const override { return QStringLiteral("manual:data_inspector.data_tables"); }

private Q_SLOTS:

    /// Is called when the user selects a different container object from the list.
    void onCurrentContainerChanged(const DataObject* dataObject);

private:

    /// The plotting widget.
    DataTablePlotWidget* _plotWidget;

    QStackedWidget* _stackedWidget;
    QAction* _switchToPlotAction;
    QAction* _switchToTableAction;
};

}   // End of namespace
