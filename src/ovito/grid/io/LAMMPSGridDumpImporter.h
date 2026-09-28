// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/grid/Grid.h>
#include <ovito/grid/objects/VoxelGrid.h>
#include <ovito/stdobj/io/StandardFrameLoader.h>
#include <ovito/core/dataset/io/FileSourceImporter.h>

namespace Ovito {

/**
 * \brief File parser for text-based LAMMPS dump files that store voxel grid data.
 */
class OVITO_GRID_EXPORT LAMMPSGridDumpImporter : public FileSourceImporter
{
    /// Defines a metaclass specialization for this importer type.
    class OOMetaClass : public FileSourceImporter::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using FileSourceImporter::OOMetaClass::OOMetaClass;

        /// Returns the list of file formats that can be read by this importer class.
        virtual std::span<const SupportedFormat> supportedFormats() const override {
            static const SupportedFormat formats[] = {{ QStringLiteral("*"), tr("LAMMPS Grid Dump Files") }};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(LAMMPSGridDumpImporter, OOMetaClass)

public:

    /// Returns the title of this object.
    virtual QString objectTitle() const override { return tr("LAMMPS Grid Dump"); }

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override {
        return std::make_unique<FrameLoader>(request);
    }

    /// Scans the data file and builds a list of source frames.
    virtual void discoverFramesInFile(const FileHandle& fileHandle, QVector<FileSourceImporter::Frame>& frames) const override;

private:

    /// The format-specific task object that is responsible for reading an input file.
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
