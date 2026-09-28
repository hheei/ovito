// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/particles/import/ParticleImporter.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/properties/InputColumnMapping.h>
#include <ovito/core/dataset/DataSetContainer.h>

namespace Ovito {

/**
 * \brief File parser for text-based LAMMPS dump simulation files.
 */
class OVITO_PARTICLES_EXPORT LAMMPSTextDumpImporter : public ParticleImporter
{
protected:

    /// Defines a metaclass specialization for this importer type.
    class OOMetaClass : public ParticleImporter::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using ParticleImporter::OOMetaClass::OOMetaClass;

        /// Returns the list of file formats that can be read by this importer class.
        virtual std::span<const SupportedFormat> supportedFormats() const override {
            static const SupportedFormat formats[] = {{ QStringLiteral("*"), tr("LAMMPS Text Dump Files") }};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(LAMMPSTextDumpImporter, OOMetaClass)

public:

    /// Indicates whether this file importer type loads particle trajectories.
    virtual bool isTrajectoryFormat() const override { return true; }

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override {
        return std::make_unique<FrameLoader>(request, sortParticles(), useCustomColumnMapping(), customColumnMapping());
    }

    /// Inspects the header of the given file and returns the number of file columns.
    virtual Future<ParticleInputColumnMapping> inspectFileHeader(const Frame& frame);

    /// Guesses the mapping of input file columns to particle properties.
    static ParticleInputColumnMapping generateAutomaticColumnMapping(const QStringList& columnNames);

protected:

    /// The format-specific task object that is responsible for reading an input file.
    class FrameLoader : public ParticleImporter::FrameLoader
    {
    public:

        /// Constructor.
        FrameLoader(const LoadOperationRequest& request,
                bool sortParticles, bool useCustomColumnMapping,
                const ParticleInputColumnMapping& customColumnMapping)
            : ParticleImporter::FrameLoader(request),
                _sortParticles(sortParticles),
                _useCustomColumnMapping(useCustomColumnMapping),
                _customColumnMapping(customColumnMapping) {}

        /// Returns the file column mapping used to load the file.
        const ParticleInputColumnMapping& columnMapping() const { return _customColumnMapping; }

    protected:

        /// Reads the frame data from the external file.
        virtual void loadFile() override;

        /// After parsing the particle data, this method post-processes the particle properties.
        void postprocessParticleProperties(const QStringList& fileColumnNames, const ParticleInputColumnMapping& columnMapping);

    protected:

        bool _sortParticles;
        bool _useCustomColumnMapping;
        ParticleInputColumnMapping _customColumnMapping;
    };

protected:

    /// Scans the data file and builds a list of source frames.
    virtual void discoverFramesInFile(const FileHandle& fileHandle, QVector<FileSourceImporter::Frame>& frames) const override;

    /// Is called when the value of a non-animatable property field of this RefMaker has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

    /// Loads the class' contents from the given stream.
    virtual void loadFromStream(ObjectLoadStream& stream) override;

private:

    /// Controls whether the mapping between input file columns and particle
    /// properties is done automatically or by the user.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, useCustomColumnMapping, setUseCustomColumnMapping);

    /// The user-defined mapping of input file columns to OVITO's particle properties.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(ParticleInputColumnMapping{}, customColumnMapping, setCustomColumnMapping);
};

}   // End of namespace
