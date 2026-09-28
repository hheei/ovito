// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/particles/import/ParticleImporter.h>
#include <ovito/core/app/Application.h>

namespace Ovito {

/**
 * \brief File parser for data files of the oxDNA code.
 *
 * File format documentation:
 *
 * https://dna.physics.ox.ac.uk/index.php/Documentation#Visualisation_of_structures
 */
class OVITO_OXDNA_EXPORT OXDNAImporter : public ParticleImporter
{
    /// Defines a metaclass specialization for this importer type.
    class OOMetaClass : public ParticleImporter::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using ParticleImporter::OOMetaClass::OOMetaClass;

        /// Returns the list of file formats that can be read by this importer class.
        virtual std::span<const SupportedFormat> supportedFormats() const override {
            static const SupportedFormat formats[] = {{ QStringLiteral("*"), tr("oxDNA Configuration Files") }};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(OXDNAImporter, OOMetaClass)

public:

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override {
        return std::make_unique<FrameLoader>(request, topologyFileUrl());
    }

    /// Scans the data file and builds a list of source frames.
    virtual void discoverFramesInFile(const FileHandle& fileHandle, QVector<FileSourceImporter::Frame>& frames) const override;

private:

    /// The format-specific task object that is responsible for reading an input file in a separate thread.
    class FrameLoader : public ParticleImporter::FrameLoader
    {
    public:

        /// Constructor.
        FrameLoader(const LoadOperationRequest& request, const QUrl& userSpecifiedTopologyUrl) :
            ParticleImporter::FrameLoader(request),
            _userSpecifiedTopologyUrl(userSpecifiedTopologyUrl) {}

    protected:

        /// Reads the frame data from the external file.
        virtual void loadFile() override;

        /// URL of the topology file if explicitly specified by the user.
        QUrl _userSpecifiedTopologyUrl;
    };

private:

    /// oxDNA files come in pairs: a topology file and a configuration file.
    /// The configuration file is the primary file passed to the file importer by the system.
    /// This extra field stores the URL of the oxDNA topology file belonging to the configuration file
    /// if explicitly specified by the user.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QUrl{}, topologyFileUrl, setTopologyFileUrl);
};

}   // End of namespace
