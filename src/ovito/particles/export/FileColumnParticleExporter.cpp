// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/Particles.h>
#include <ovito/core/dataset/DataSet.h>
#include "FileColumnParticleExporter.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(FileColumnParticleExporter);

/******************************************************************************
* Constructor.
*****************************************************************************/
void FileColumnParticleExporter::initializeObject(ObjectInitializationFlags flags)
{
    ParticleExporter::initializeObject(flags);

#ifndef OVITO_DISABLE_QSETTINGS
    if(this_task::isInteractive()) {
        // Restore last output column mapping.
        QSettings settings;
        settings.beginGroup("exporter/particles/");
        if(settings.contains("columnmapping")) {
            try {
                _columnMapping.fromByteArray(settings.value("columnmapping").toByteArray());
            }
            catch(Exception& ex) {
                ex.prependGeneralMessage(tr("Failed to load previous output column mapping from application settings store."));
                ex.logError();
            }
        }
        settings.endGroup();
    }
#endif
}

}   // End of namespace
