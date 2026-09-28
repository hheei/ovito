// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/mesh/Mesh.h>
#include <ovito/stdobj/io/StandardFrameLoader.h>
#include <ovito/core/dataset/io/FileSourceImporter.h>
#include <ovito/mesh/io/ParaViewVTMImporter.h>

#include <QXmlStreamReader>

namespace Ovito {

/**
 * \brief File parser for reading a SurfaceMesh from a ParaView VTP (PolyData) file.
 */
class OVITO_MESH_EXPORT ParaViewVTPMeshImporter : public FileSourceImporter
{
    /// Defines a metaclass specialization for this importer type.
    class OOMetaClass : public FileSourceImporter::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using FileSourceImporter::OOMetaClass::OOMetaClass;

        /// Returns the list of file formats that can be read by this importer class.
        virtual std::span<const SupportedFormat> supportedFormats() const override {
            static const SupportedFormat formats[] = {{ QStringLiteral("*.vtp"), tr("ParaView PolyData Mesh Files") }};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(ParaViewVTPMeshImporter, OOMetaClass)

public:

    /// Constructor.
    using FileSourceImporter::FileSourceImporter;

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override {
        return std::make_unique<FrameLoader>(request);
    }

    /// Reads a <DataArray> element from a VTK file and stores it in the given OVITO data buffer.
    static bool parseVTKDataArray(DataBuffer* buffer, int vtkHeaderType, QXmlStreamReader& xml, int vectorComponent = -1, size_t destBaseIndex = 0, size_t replication = 1);

private:

    /// The format-specific task object that is responsible for reading an input file in a separate thread.
    class FrameLoader : public StandardFrameLoader
    {
    public:

        /// Inherit constructor from base class.
        using StandardFrameLoader::StandardFrameLoader;

    protected:

        /// Reads the frame data from the external file.
        virtual void loadFile() override;

        /// Reads a <DataArray> element and returns it as an OVITO property.
        PropertyPtr parseDataArray(QXmlStreamReader& xml, int vtkHeaderType, int convertToDataType = 0);
    };
};

/**
 * \brief Plugin filter used to customize the loading of VTM files referencing one or more ParaView VTP mesh files.
 *        This filter is needed to correctly load VTM/VTP file combinations written by the Aspherix simulation code.
 */
class OVITO_MESH_EXPORT MeshParaViewVTMFileFilter : public ParaViewVTMFileFilter
{
    OVITO_CLASS(MeshParaViewVTMFileFilter)

public:

    /// Constructor.
    explicit MeshParaViewVTMFileFilter() = default;

    /// \brief Is called once before the datasets referenced in a multi-block VTM file will be loaded.
    virtual void preprocessDatasets(std::vector<ParaViewVTMBlockInfo>& blockDatasets, FileSourceImporter::LoadOperationRequest& request, const ParaViewVTMImporter& vtmImporter) override;
};

}   // End of namespace
