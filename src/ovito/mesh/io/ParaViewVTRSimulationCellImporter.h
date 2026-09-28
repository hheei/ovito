// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/mesh/Mesh.h>
#include <ovito/stdobj/io/StandardFrameLoader.h>
#include <ovito/core/dataset/io/FileSourceImporter.h>

#include <QXmlStreamReader>

namespace Ovito {

/**
 * \brief File parser for reading the simulation cell geometry from a ParaView VTR (RectilinearGrid) file as written by the Aspherix simulation code.
 */
class OVITO_MESH_EXPORT ParaViewVTRSimulationCellImporter : public FileSourceImporter
{
    /// Defines a metaclass specialization for this importer type.
    class OOMetaClass : public FileSourceImporter::OOMetaClass
    {
    public:

        /// Inherit standard constructor from base meta class.
        using FileSourceImporter::OOMetaClass::OOMetaClass;

        /// Checks if the given file has format that can be read by this importer.
        virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(ParaViewVTRSimulationCellImporter, OOMetaClass)

public:

    /// Constructor.
    using FileSourceImporter::FileSourceImporter;

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override {
        return std::make_unique<FrameLoader>(request);
    }

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
    };
};

}   // End of namespace
