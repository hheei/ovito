// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/core/dataset/io/FileSourceImporter.h>

#include <QXmlStreamReader>

namespace Ovito {

/**
 * \brief File reader for particle-wall contact network data in ParaView VTP (vtkPolyData) files written by the Aspherix simulation code.
 */
class OVITO_PARTICLES_EXPORT ParaViewVTPParticleWallContactsImporter : public FileSourceImporter
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

    OVITO_CLASS_META(ParaViewVTPParticleWallContactsImporter, OOMetaClass)

public:

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override {
        return std::make_unique<FrameLoader>(request);
    }

private:

    /// The format-specific task object that is responsible for reading an input file in a separate thread.
    class FrameLoader : public FileSourceImporter::FrameLoader
    {
    public:

        /// Constructor.
        using FileSourceImporter::FrameLoader::FrameLoader;

    protected:

        /// Reads the frame data from the external file.
        virtual void loadFile() override;

        /// Creates the right kind of OVITO property object that will receive the data read from a <DataArray> element.
        Property* createLinesPropertyForDataArray(QXmlStreamReader& xml, int& vectorComponent, Lines* lines, DataBuffer::BufferInitialization propertyAccessMode);
    };
};

}   // End of namespace
