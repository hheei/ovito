// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/grid/Grid.h>
#include <ovito/stdobj/io/StandardFrameLoader.h>
#include <ovito/core/dataset/io/FileSourceImporter.h>

#include <QXmlStreamReader>

namespace Ovito {

/**
 * \brief File parser for reading a VoxelGrid from a ParaView VTI (ImageData) file.
 */
class OVITO_GRID_EXPORT ParaViewVTIGridImporter : public FileSourceImporter
{
    /// Defines a metaclass specialization for this importer type.
    class OOMetaClass : public FileSourceImporter::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using FileSourceImporter::OOMetaClass::OOMetaClass;

        /// Returns the list of file formats that can be read by this importer class.
        virtual std::span<const SupportedFormat> supportedFormats() const override {
            static const SupportedFormat formats[] = {{ QStringLiteral("*.vti"), tr("ParaView VTI ImageData Files") }};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(ParaViewVTIGridImporter, OOMetaClass)

public:

    /// Returns the title of this object.
    virtual QString objectTitle() const override { return tr("VTI"); }

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

    private:

        /// Creates the right kind of OVITO property object that will receive the data read from a <DataArray> element.
        Property* createGridPropertyForDataArray(VoxelGrid* gridObj, QXmlStreamReader& xml);
    };
};

}   // End of namespace
