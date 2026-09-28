// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/particles/import/ParticleImporter.h>
#include <ovito/core/dataset/DataSetContainer.h>

namespace Ovito {

/**
 * \brief File parser for the HDF5 output files (vaspout.h5, vaspwave.h5) written by the VASP DFT code.
 *
 * The reader extracts the atomic structure, the simulation cell and the full ion-dynamics
 * trajectory (MD/relaxation frames), as well as per-atom velocities/forces, per-frame
 * energies, and charge density fields (as voxel grids).
 */
class OVITO_PARTICLES_EXPORT VASPHDF5Importer : public ParticleImporter
{
    /// Defines a metaclass specialization for this importer type.
    class OOMetaClass : public ParticleImporter::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using ParticleImporter::OOMetaClass::OOMetaClass;

        /// Returns the list of file formats that can be read by this importer class.
        virtual std::span<const SupportedFormat> supportedFormats() const override {
            static const SupportedFormat formats[] = {{ QStringLiteral("*.h5"), tr("VASP HDF5 Files") }};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(VASPHDF5Importer, OOMetaClass)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags) {
        ParticleImporter::initializeObject(flags);
        setMultiTimestepFile(true);
    }

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override {
        return std::make_unique<FrameLoader>(request, recenterCell(), generateBonds());
    }

protected:

    /// Scans the data file and builds a list of source frames.
    virtual void discoverFramesInFile(const FileHandle& fileHandle, QVector<FileSourceImporter::Frame>& frames) const override;

    /// Called when the pipeline is first created; auto-inserts a CreateIsosurfaceModifier (GUI mode only).
    virtual Future<void> setupPipeline(OORef<Pipeline> pipeline, OORef<FileSource> importObj) override;

private:

    /// The format-specific task object that is responsible for reading an input file in the background.
    class FrameLoader : public ParticleImporter::FrameLoader
    {
    public:

        /// Constructor.
        FrameLoader(const LoadOperationRequest& request, bool recenterCell, bool generateBonds)
            : ParticleImporter::FrameLoader::FrameLoader(request, recenterCell), _generateBonds(generateBonds) {}

    protected:

        /// Reads the frame data from the external file.
        virtual void loadFile() override;

    private:

        /// Stores charge density and optional magnetization density in a VoxelGrid.
        QString readChargeDensity(size_t nspins, size_t nx, size_t ny, size_t nz,
                                  const std::vector<double>& chargeData, TaskProgress& progress);

        /// Controls the generation of ad-hoc bonds during data import.
        bool _generateBonds;
    };
};

}   // End of namespace
