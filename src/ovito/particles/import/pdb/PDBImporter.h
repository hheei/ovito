// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/particles/import/ParticleImporter.h>
#include <ovito/core/dataset/DataSetContainer.h>

namespace Ovito {

/**
 * \brief File parser for Protein Data Bank (PDB) files.
 */
class OVITO_PARTICLES_EXPORT PDBImporter : public ParticleImporter
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
            static const SupportedFormat formats[] = {{ QStringLiteral("*.pdb *.pdb.gz"), tr("PDB Files") }};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        [[nodiscard]] virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(PDBImporter, OOMetaClass)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags) {
        ParticleImporter::initializeObject(flags);
        setGenerateBonds(true);
    }

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override {
        return std::make_unique<FrameLoader>(request, recenterCell(), generateBonds(), generateBoundingBox(), sortParticles());
    }

    /// Scans the data file and builds a list of source frames.
    virtual void discoverFramesInFile(const FileHandle& fileHandle, QVector<FileSourceImporter::Frame>& frames) const override;

private:

    /// The format-specific task object that is responsible for reading an input file in the background.
    class FrameLoader : public ParticleImporter::FrameLoader
    {
    public:

        /// Constructor.
        FrameLoader(const LoadOperationRequest& request, bool recenterCell, bool generateBonds, bool generateBoundingBox, bool sortParticles)
            : ParticleImporter::FrameLoader::FrameLoader(request, recenterCell, generateBoundingBox), _generateBonds(generateBonds), _sortParticles(sortParticles)
        {
        }

    protected:

        /// Reads the frame data from the external file.
        virtual void loadFile() override;

    private:

        /// Controls the generation of ad-hoc bonds during data import.
        bool _generateBonds;

        /// Controls sorting of particles by ID after loading.
        bool _sortParticles;
    };
};

}   // End of namespace
