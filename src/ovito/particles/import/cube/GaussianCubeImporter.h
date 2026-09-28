// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/particles/import/ParticleImporter.h>
#include <ovito/grid/objects/VoxelGrid.h>
#include <ovito/core/dataset/DataSetContainer.h>

namespace Ovito {

/**
 * \brief File parser for Gaussian Cube file containing atomic coordinates and volumetric voxel data.
 */
class OVITO_PARTICLES_EXPORT GaussianCubeImporter : public ParticleImporter
{
    /// Defines a metaclass specialization for this importer type.
    class OOMetaClass : public ParticleImporter::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using ParticleImporter::OOMetaClass::OOMetaClass;

        /// Returns the list of file formats that can be read by this importer class.
        virtual std::span<const SupportedFormat> supportedFormats() const override {
            static const SupportedFormat formats[] = {{ QStringLiteral("*"), tr("Gaussian Cube Files") }};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(GaussianCubeImporter, OOMetaClass)

public:

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override {
        return std::make_unique<FrameLoader>(request, recenterCell(), generateBonds(), gridType(), convertFieldBohrToAngstrom());
    }

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:

    /// The format-specific task object that is responsible for reading an input file in the background.
    class FrameLoader : public ParticleImporter::FrameLoader
    {
    public:

        /// Constructor.
        FrameLoader(const LoadOperationRequest& request, bool recenterCell, bool generateBonds, VoxelGrid::GridType gridType, bool convertFieldBohrToAngstrom)
            : ParticleImporter::FrameLoader::FrameLoader(request, recenterCell)
            , _generateBonds(generateBonds)
            , _gridType(gridType)
            , _convertFieldBohrToAngstrom(convertFieldBohrToAngstrom) {}

    protected:

        /// Reads the frame data from the external file.
        virtual void loadFile() override;

    private:

        /// Controls the generation of ad-hoc bonds during data import.
        bool _generateBonds;

        /// The type of grid to be imported.
        VoxelGrid::GridType _gridType;

        /// Controls whether field values are assumed to be density values given in Bohr units (atomic units) and require conversion to Angstroms.
        bool _convertFieldBohrToAngstrom;
    };

private:

    /// Controls whether the grid's sampling points are point-based or cell-based.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(VoxelGrid::GridType{VoxelGrid::GridType::PointData}, gridType, setGridType);

    /// Controls whether field values are assumed to be density values given in Bohr units (atomic units) and require conversion to Angstroms.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{true}, convertFieldBohrToAngstrom, setConvertFieldBohrToAngstrom);
};

}   // End of namespace
