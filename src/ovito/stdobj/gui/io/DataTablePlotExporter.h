// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/core/dataset/io/FileExporter.h>

namespace Ovito {

/**
 * \brief Exporter that writes the graphical plot of a data table to an image file.
 */
class OVITO_STDOBJGUI_EXPORT DataTablePlotExporter : public FileExporter
{
    /// Defines a metaclass specialization for this exporter type.
    class OOMetaClass : public FileExporter::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using FileExporter::OOMetaClass::OOMetaClass;

        /// Returns the file filter that specifies the files that can be exported by this service.
        virtual QString fileFilter() const override { return QStringLiteral("*.pdf *.png"); }

        /// Returns the filter description that is displayed in the drop-down box of the file dialog.
        virtual QString fileFilterDescription() const override { return tr("Data Plot"); }
    };

    OVITO_CLASS_META(DataTablePlotExporter, OOMetaClass)

public:

    /// Returns the type(s) of data objects that this exporter service can export.
    virtual std::vector<DataObjectClassPtr> exportableDataObjectClass() override {
        return { &DataTable::OOClass() };
    }

protected:

    /// Creates a worker performing the actual data export.
    virtual OORef<FileExportJob> createExportJob(const QString& filePath, int numberOfFrames) override;

private:

    /// The width of the plot in millimeters.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{150}, plotWidth, setPlotWidth, PROPERTY_FIELD_MEMORIZE);

    /// The height of the plot in millimeters.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{100}, plotHeight, setPlotHeight, PROPERTY_FIELD_MEMORIZE);

    /// The resolution of the plot in DPI.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{200}, plotDPI, setPlotDPI, PROPERTY_FIELD_MEMORIZE);
};

}   // End of namespace
