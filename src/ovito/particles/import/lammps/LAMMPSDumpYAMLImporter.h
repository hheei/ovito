// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include "LAMMPSTextDumpImporter.h"

namespace Ovito {

/**
 * \brief File parser for LAMMPS dump files in YAML format.
 */
class OVITO_PARTICLES_EXPORT LAMMPSDumpYAMLImporter : public LAMMPSTextDumpImporter
{
    /// Defines a metaclass specialization for this importer type.
    class OOMetaClass : public LAMMPSTextDumpImporter::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using LAMMPSTextDumpImporter::OOMetaClass::OOMetaClass;

        /// Returns the list of file formats that can be read by this importer class.
        virtual std::span<const SupportedFormat> supportedFormats() const override {
            static const SupportedFormat formats[] = {{ QStringLiteral("*"), tr("LAMMPS Dump YAML Files") }};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(LAMMPSDumpYAMLImporter, OOMetaClass)

public:

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override {
        return std::make_unique<FrameLoader>(request, sortParticles(), useCustomColumnMapping(), customColumnMapping());
    }

    /// Inspects the header of the given file and returns the number of file columns.
    virtual Future<ParticleInputColumnMapping> inspectFileHeader(const Frame& frame) override;

    /// Scans the data file and builds a list of source frames.
    virtual void discoverFramesInFile(const FileHandle& fileHandle, QVector<FileSourceImporter::Frame>& frames) const override;

private:

    /// The format-specific task object that is responsible for reading an input file.
    class FrameLoader : public LAMMPSTextDumpImporter::FrameLoader
    {
    public:

        /// Inherit constructor from base class.
        using LAMMPSTextDumpImporter::FrameLoader::FrameLoader;

    protected:

        /// Reads the frame data from the external file.
        virtual void loadFile() override;
    };
};

}   // End of namespace
