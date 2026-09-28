// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/particles/Particles.h>
#include <ovito/particles/import/ParticleImporter.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/core/dataset/DataSetContainer.h>

namespace Ovito {

/**
 * \brief File parser for the text-based XYZ file format.
 */
class OVITO_PARTICLES_EXPORT SDFImporter : public ParticleImporter
{
    /// Defines a metaclass specialization for this importer type.
    class OOMetaClass : public ParticleImporter::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using ParticleImporter::OOMetaClass::OOMetaClass;

        /// Returns the list of file formats that can be read by this importer class.
        [[nodiscard]] virtual std::span<const SupportedFormat> supportedFormats() const override
        {
            static const std::array<SupportedFormat, 1> formats{{QStringLiteral("*"), tr("MOL/SDF Files")}};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        [[nodiscard]] virtual bool checkFileFormat(const FileHandle& file) const override;

        /// Returns a numeric value that is used to sort the list of file readers.
        /// File readers with higher priority get to check a file first during format auto-detection.
        /// SDF file reader has to run before the xyz file reader therefore it gets an increased priority.
        [[nodiscard]] virtual int autodetectionPriority() const override
        {
            return ParticleImporter::OOMetaClass::autodetectionPriority() + 10;
        }
    };

    OVITO_CLASS_META(SDFImporter, OOMetaClass)

public:

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override
    {
        return std::make_unique<FrameLoader>(request, recenterCell(), generateBoundingBox());
    }

private:
    /// The format-specific task object that is responsible for reading an input file in the background.
    class FrameLoader : public ParticleImporter::FrameLoader
    {
    public:
        /// Constructor.
        using ParticleImporter::FrameLoader::FrameLoader;

    protected:
        /// Reads the frame data from the external file.
        virtual void loadFile() override;
    };

protected:
    /// Scans the data file and builds a list of source frames.
    virtual void discoverFramesInFile(const FileHandle& fileHandle, QVector<FileSourceImporter::Frame>& frames) const override;
};

}  // namespace Ovito
