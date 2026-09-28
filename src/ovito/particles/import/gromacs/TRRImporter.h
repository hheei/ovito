// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/particles/Particles.h>
#include <ovito/particles/import/ParticleImporter.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/core/dataset/DataSetContainer.h>

namespace Ovito {

/**
 * File parser for GROMACS TRR trajectory files.
 */
class OVITO_PARTICLES_EXPORT TRRImporter : public ParticleImporter
{
    /// Defines a metaclass specialization for this importer type.
    class OOMetaClass : public ParticleImporter::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using ParticleImporter::OOMetaClass::OOMetaClass;

        /// Returns the list of file formats that can be read by this importer class.
        virtual std::span<const SupportedFormat> supportedFormats() const override {
            static const SupportedFormat formats[] = {{QStringLiteral("*.trr"), tr("Gromacs Trajectory Files")}};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(TRRImporter, OOMetaClass)

public:
    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags)
    {
        ParticleImporter::initializeObject(flags);
        setMultiTimestepFile(true);
        setRecenterCell(true);
    }

    /// Indicates whether this file importer type loads particle trajectories.
    virtual bool isTrajectoryFormat() const override { return true; }

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override
    {
        return std::make_unique<FrameLoader>(request, recenterCell());
    }

    /// Scans the data file and builds a list of source frames.
    virtual void discoverFramesInFile(const FileHandle& fileHandle, QVector<FileSourceImporter::Frame>& frames) const override;

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
};

}  // namespace Ovito
