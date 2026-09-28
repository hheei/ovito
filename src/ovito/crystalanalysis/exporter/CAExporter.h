// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/crystalanalysis/objects/DislocationNetwork.h>
#include <ovito/core/dataset/io/FileExporter.h>

namespace Ovito {

/**
 * \brief Exporter that exports dislocation lines to a Crystal Analysis Tool (CA) file.
 */
class OVITO_CRYSTALANALYSIS_EXPORT CAExporter : public FileExporter
{
    /// Defines a metaclass specialization for this exporter type.
    class OOMetaClass : public FileExporter::OOMetaClass
    {
    public:

        /// Inherit standard constructor from base meta class.
        using FileExporter::OOMetaClass::OOMetaClass;

        /// Returns the file filter that specifies the extension of files written by this service.
        virtual QString fileFilter() const override { return QStringLiteral("*.ca *.ca.gz"); }

        /// Returns the filter description that is displayed in the drop-down box of the file dialog.
        virtual QString fileFilterDescription() const override { return tr("Crystal Analysis"); }
    };

    OVITO_CLASS_META(CAExporter, OOMetaClass)

public:

    /// Constructor.
    using FileExporter::FileExporter;

    /// Returns whether the DXA defect mesh is exported (in addition to the dislocation lines).
    bool meshExportEnabled() const { return _meshExportEnabled; }

    /// Sets whether the DXA defect mesh is exported (in addition to the dislocation lines).
    void setMeshExportEnabled(bool enable) { _meshExportEnabled = enable; }

    /// \brief Returns the type(s) of data objects that this exporter service can export.
    virtual std::vector<DataObjectClassPtr> exportableDataObjectClass() override {
        return { &DislocationNetwork::OOClass() };
    }

protected:

    /// Creates a worker performing the actual data export.
    virtual OORef<FileExportJob> createExportJob(const QString& filePath, int numberOfFrames) override;

private:

    /// Controls whether the DXA defect mesh is exported (in addition to the dislocation lines).
    bool _meshExportEnabled = true;
};

}   // End of namespace
