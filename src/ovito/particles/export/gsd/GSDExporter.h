// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/particles/export/ParticleExporter.h>

namespace Ovito {

class GSDFile;  // Defined in GSDFile.h

/**
 * \brief Exporter that writes GSD (General Simulation Data) files as used by the HOOMD simulation code.
 */
class OVITO_PARTICLES_EXPORT GSDExporter : public ParticleExporter
{
    /// Defines a metaclass specialization for this exporter type.
    class OOMetaClass : public ParticleExporter::OOMetaClass
    {
    public:

        /// Inherit standard constructor from base meta class.
        using ParticleExporter::OOMetaClass::OOMetaClass;

        /// Returns the file filter that specifies the extension of files written by this service.
        virtual QString fileFilter() const override { return QStringLiteral("*.gsd"); }

        /// Returns the filter description that is displayed in the drop-down box of the file dialog.
        virtual QString fileFilterDescription() const override { return tr("GSD/HOOMD"); }
    };

    OVITO_CLASS_META(GSDExporter, OOMetaClass)

public:
    enum DataType
    {
        Float32 = QMetaType::Float,
        Float64 = QMetaType::Double,
    };

    /// Indicates whether this file exporter can write more than one animation frame into a single output file.
    virtual bool supportsMultiFrameFiles() override { return true; }

protected:

    /// Creates a worker performing the actual data export.
    virtual OORef<FileExportJob> createExportJob(const QString& filePath, int numberOfFrames) override;

private:
    /// Controls whether atomic coordinates are written in reduced form to the POSCAR file.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(DataType{Float64}, dataType, setDataType, PROPERTY_FIELD_MEMORIZE);
};

}   // End of namespace
