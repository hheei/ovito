////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ovito/particles/Particles.h>
#include <ovito/particles/import/ParticleImporter.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/core/dataset/DataSetContainer.h>

namespace Ovito {

/**
 * \brief File parser for the Tripos MOL2 molecular file format.
 */
class OVITO_PARTICLES_EXPORT MOL2Importer : public ParticleImporter
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
            static const std::array<SupportedFormat, 1> formats{{QStringLiteral("*.mol2"), tr("Tripos MOL2 Files")}};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        [[nodiscard]] virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(MOL2Importer, OOMetaClass)

public:

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override
    {
        return std::make_unique<FrameLoader>(request, recenterCell(), generateBoundingBox(), sortParticles());
    }

private:
    /// The format-specific task object that is responsible for reading an input file in the background.
    class FrameLoader : public ParticleImporter::FrameLoader
    {
    public:
        /// Constructor.
        FrameLoader(const LoadOperationRequest& request, bool recenterCell, bool generateBoundingBox, bool sortParticles)
            : ParticleImporter::FrameLoader(request, recenterCell, generateBoundingBox), _sortParticles(sortParticles) {}

    protected:
        /// Reads the frame data from the external file.
        virtual void loadFile() override;

    private:
        bool _sortParticles;
    };

protected:
    /// Scans the data file and builds a list of source frames.
    virtual void discoverFramesInFile(const FileHandle& fileHandle, QVector<FileSourceImporter::Frame>& frames) const override;
};

}  // namespace Ovito
