// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/particles/Particles.h>
#include <ovito/particles/import/ParticleImporter.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/core/dataset/DataSetContainer.h>

namespace Ovito {

/**
 * \brief File parser for the MedeA SLI (Structure List) SQLite database format.
 */
class OVITO_PARTICLES_EXPORT MedeA_SLI_Importer : public ParticleImporter
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
            static const std::array<SupportedFormat, 1> formats{{QStringLiteral("*.sli *.Trajectory.data"), tr("MedeA SLI Files")}};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        [[nodiscard]] virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(MedeA_SLI_Importer, OOMetaClass)

public:

    /// Constructor — always treat SLI files as multi-frame.
    void initializeObject(ObjectInitializationFlags flags) {
        ParticleImporter::initializeObject(flags);
        setMultiTimestepFile(true);
    }

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override
    {
        return std::make_unique<FrameLoader>(request, recenterCell(), generateBoundingBox());
    }

protected:

    /// Scans the SLI database to discover all animation frames.
    virtual void discoverFramesInFile(const FileHandle& fileHandle, QVector<FileSourceImporter::Frame>& frames) const override;

private:
    /// The format-specific task object that is responsible for reading an input file in the background.
    class FrameLoader : public ParticleImporter::FrameLoader
    {
    public:
        explicit FrameLoader(const LoadOperationRequest& request, bool recenterCell, bool generateBoundingBox)
            : ParticleImporter::FrameLoader(request, recenterCell, generateBoundingBox)
            , _recenterCell(recenterCell), _generateBoundingBox(generateBoundingBox) {}

    protected:
        virtual void loadFile() override;

    private:
        bool _recenterCell;
        bool _generateBoundingBox;
    };
};

}  // namespace Ovito
