// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/particles/import/ParticleImporter.h>
#include <ovito/stdobj/properties/InputColumnMapping.h>
#include <ovito/core/dataset/DataSetContainer.h>

namespace Ovito {

/**
 * \brief File parser for LAMMPS dump local files, which can contain bond/angle/dihedral/improper attributes.
 */
class OVITO_PARTICLES_EXPORT LAMMPSDumpLocalImporter : public ParticleImporter
{
    /// Defines a metaclass specialization for this importer type.
    class OOMetaClass : public ParticleImporter::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using ParticleImporter::OOMetaClass::OOMetaClass;

        /// Returns the list of file formats that can be read by this importer class.
        virtual std::span<const SupportedFormat> supportedFormats() const override {
            static const SupportedFormat formats[] = {{ QStringLiteral("*"), tr("LAMMPS Dump Local Files") }};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(LAMMPSDumpLocalImporter, OOMetaClass)

public:

    /// Indicates whether this file importer type loads particle trajectories.
    virtual bool isTrajectoryFormat() const override { return true; }

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override {
        return std::make_unique<FrameLoader>(request, columnMapping());
    }

    /// Inspects the header of the given file and returns the list of file columns.
    [[nodiscard]] Future<InputColumnMapping> inspectFileHeader(const Frame& frame);

protected:

    /// Scans the data file and builds a list of source frames.
    virtual void discoverFramesInFile(const FileHandle& fileHandle, QVector<FileSourceImporter::Frame>& frames) const override;

    /// Is called when the value of a non-animatable property field of this RefMaker has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

    /// Guesses the mapping of input file columns to OVITO properties.
    static InputColumnMapping generateAutomaticColumnMapping(PropertyContainerClassPtr containerClass, const QStringList& columnNames);

private:

    /// The format-specific task object that is responsible for reading an input file in the background.
    class FrameLoader : public ParticleImporter::FrameLoader
    {
    public:

        /// Constructor.
        FrameLoader(const LoadOperationRequest& request, const InputColumnMapping& columnMapping)
            : ParticleImporter::FrameLoader(request), _columnMapping(columnMapping) {}

        /// Returns the file column mapping used to load the file.
        const InputColumnMapping& columnMapping() const { return _columnMapping; }

    protected:

        /// Reads the frame data from the external file.
        virtual void loadFile() override;

    private:

        InputColumnMapping _columnMapping;
    };

private:

    /// The user-defined mapping of input file columns to OVITO properties.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(InputColumnMapping{}, columnMapping, setColumnMapping);
};

}   // End of namespace
