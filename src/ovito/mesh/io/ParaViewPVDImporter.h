// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/mesh/Mesh.h>
#include <ovito/core/dataset/io/FileSourceImporter.h>

#include <QXmlStreamReader>

namespace Ovito {

/**
 * \brief File parser for ParaView data files (PVD).
 *
 * See https://www.paraview.org/Wiki/ParaView/Data_formats
 */
class OVITO_MESH_EXPORT ParaViewPVDImporter : public FileSourceImporter
{
    /// Defines a metaclass specialization for this importer type.
    class OOMetaClass : public FileSourceImporter::OOMetaClass
    {
    public:

        /// Inherit standard constructor from base meta class.
        using FileSourceImporter::OOMetaClass::OOMetaClass;

        /// Returns the list of file formats that can be read by this importer class.
        virtual std::span<const SupportedFormat> supportedFormats() const override {
            static const SupportedFormat formats[] = {{ QStringLiteral("*.pvd"), tr("ParaView PVD Files") }};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(ParaViewPVDImporter, OOMetaClass)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags) {
        FileSourceImporter::initializeObject(flags);
        setMultiTimestepFile(true);
    }

    /// Loads the data for the given frame from the external file.
    virtual Future<PipelineFlowState> loadFrame(const LoadOperationRequest& request) override;

    /// Scans the data file and builds a list of source frames.
    virtual void discoverFramesInFile(const FileHandle& fileHandle, QVector<FileSourceImporter::Frame>& frames) const override;

private:

    /// The delegate importer responsible for parsing the datasets referenced in the PVD file.
    DECLARE_REFERENCE_FIELD_FLAGS(OORef<FileSourceImporter>, childImporter, PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_OPEN_SUBEDITOR);
};

}   // End of namespace
