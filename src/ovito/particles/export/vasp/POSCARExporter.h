// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include "../ParticleExporter.h"

namespace Ovito {

/**
 * \brief Exporter that writes the particles to a POSCAR file.
 */
class OVITO_PARTICLES_EXPORT POSCARExporter : public ParticleExporter
{
    /// Defines a metaclass specialization for this exporter type.
    class OOMetaClass : public ParticleExporter::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using ParticleExporter::OOMetaClass::OOMetaClass;

        /// Returns the file filter that specifies the extension of files written by this service.
        virtual QString fileFilter() const override { return QStringLiteral("*"); }

        /// Returns the filter description that is displayed in the drop-down box of the file dialog.
        virtual QString fileFilterDescription() const override { return tr("POSCAR"); }
    };

    OVITO_CLASS_META(POSCARExporter, OOMetaClass)

protected:

    /// Creates a worker performing the actual data export.
    virtual OORef<FileExportJob> createExportJob(const QString& filePath, int numberOfFrames) override;

private:

    /// Controls whether atomic coordinates are written in reduced form to the POSCAR file.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, writeReducedCoordinates, setWriteReducedCoordinates, PROPERTY_FIELD_MEMORIZE);
};

}   // End of namespace
