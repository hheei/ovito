// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/crystalanalysis/objects/DislocationNetwork.h>
#include <ovito/core/dataset/io/FileExporter.h>

namespace Ovito {

/**
 * \brief Exporter that exports dislocation networks to a VTK (ParaView) file.
 */
class OVITO_CRYSTALANALYSIS_EXPORT VTKDislocationsExporter : public FileExporter
{
    /// Defines a metaclass specialization for this exporter type.
    class OOMetaClass : public FileExporter::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using FileExporter::OOMetaClass::OOMetaClass;

        /// Returns the file filter that specifies the files that can be exported by this service.
        virtual QString fileFilter() const override { return QStringLiteral("*.vtk"); }

        /// Returns the filter description that is displayed in the drop-down box of the file dialog.
        virtual QString fileFilterDescription() const override { return tr("VTK Dislocation Lines"); }
    };

    OVITO_CLASS_META(VTKDislocationsExporter, OOMetaClass)

public:

    /// Constructor.
    using FileExporter::FileExporter;

    /// \brief Returns the type(s) of data objects that this exporter service can export.
    virtual std::vector<DataObjectClassPtr> exportableDataObjectClass() override {
        return { &DislocationNetwork::OOClass() };
    }

protected:

    /// Creates a worker performing the actual data export.
    virtual OORef<FileExportJob> createExportJob(const QString& filePath, int numberOfFrames) override;
};

}   // End of namespace
