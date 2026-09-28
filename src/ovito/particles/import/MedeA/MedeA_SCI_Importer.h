// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/particles/Particles.h>
#include <ovito/particles/import/ParticleImporter.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/core/dataset/DataSetContainer.h>

namespace Ovito {

/**
 * \brief File parser for the MedeA SCI molecular/crystalline file format.
 */
class OVITO_PARTICLES_EXPORT MedeA_SCI_Importer : public ParticleImporter
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
            static const std::array<SupportedFormat, 1> formats{{QStringLiteral("*.sci"), tr("MedeA SCI Files")}};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        [[nodiscard]] virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(MedeA_SCI_Importer, OOMetaClass)

public:

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override
    {
        return std::make_unique<FrameLoader>(request, recenterCell(), generateBoundingBox(), sortParticles());
    }

    /// The format-specific task object that is responsible for reading an input file in the background.
    /// Declared public so that MedeA_SLI_Importer can delegate to it for parsing single frames.
    class FrameLoader : public ParticleImporter::FrameLoader
    {
    public:
        /// Constructor.
        FrameLoader(const LoadOperationRequest& request, bool recenterCell, bool generateBoundingBox, bool sortParticles)
            : ParticleImporter::FrameLoader(request, recenterCell, generateBoundingBox), _sortParticles(sortParticles) {}

        /// Reads the frame data from the external file.
        virtual void loadFile() override;

        /// Parses SCI body data from a raw memory buffer, bypassing the file-format header.
        /// Called by loadFile() after header validation, and directly by MedeA_SLI_Importer.
        void loadFileBody(const char* bufStart, const char* bufEnd, TaskProgress& progress);

    private:
        bool _sortParticles;
    };
};

}  // namespace Ovito
