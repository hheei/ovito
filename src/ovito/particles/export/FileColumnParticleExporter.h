// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/stdobj/io/PropertyOutputWriter.h>
#include "ParticleExporter.h"

namespace Ovito {

/**
 * \brief Abstract base class for export services that can export an arbitrary list of particle properties.
 */
class OVITO_PARTICLES_EXPORT FileColumnParticleExporter : public ParticleExporter
{
    OVITO_CLASS(FileColumnParticleExporter)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// \brief Returns the mapping of particle properties to output file columns.
    const OutputColumnMapping& columnMapping() const { return _columnMapping; }

    /// \brief Sets the mapping of particle properties to output file columns.
    void setColumnMapping(const OutputColumnMapping& mapping) { _columnMapping = mapping; }

public:

    Q_PROPERTY(Ovito::OutputColumnMapping columnMapping READ columnMapping WRITE setColumnMapping)

private:

    /// The mapping of particle properties to output file columns.
    OutputColumnMapping _columnMapping;
};

}   // End of namespace
