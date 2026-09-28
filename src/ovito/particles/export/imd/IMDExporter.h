// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include "../FileColumnParticleExporter.h"

namespace Ovito {

/**
 * \brief Exporter that writes the particles to an IMD file.
 */
class OVITO_PARTICLES_EXPORT IMDExporter : public FileColumnParticleExporter
{
    /// Defines a metaclass specialization for this exporter type.
    class OOMetaClass : public FileColumnParticleExporter::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using FileColumnParticleExporter::OOMetaClass::OOMetaClass;

        /// Returns the file filter that specifies the extension of files written by this service.
        virtual QString fileFilter() const override { return QStringLiteral("*"); }

        /// Returns the filter description that is displayed in the drop-down box of the file dialog.
        virtual QString fileFilterDescription() const override { return tr("IMD"); }
    };

    OVITO_CLASS_META(IMDExporter, OOMetaClass)

protected:

    /// Creates a worker performing the actual data export.
    virtual OORef<FileExportJob> createExportJob(const QString& filePath, int numberOfFrames) override;
};

}   // End of namespace
