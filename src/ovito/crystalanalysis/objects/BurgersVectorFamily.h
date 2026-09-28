// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/stdobj/properties/ElementType.h>
#include <ovito/crystalanalysis/objects/Cluster.h>

namespace Ovito {

/**
 * \brief represents a dislocation type.
 */
class OVITO_CRYSTALANALYSIS_EXPORT BurgersVectorFamily : public ElementType
{
    OVITO_CLASS(BurgersVectorFamily)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags, int id = 0, const QString& name = tr("Other"));

    /// Checks if the given Burgers vector is a member of this family.
    bool isMember(const Cluster::VecType& v, const MicrostructurePhase* latticeStructure) const;

private:

    /// This prototype Burgers vector of this family.
    DECLARE_MODIFIABLE_PROPERTY_FIELD((Vector3{0,0,0}), burgersVector, setBurgersVector);
    DECLARE_SNAPSHOT_PROPERTY_FIELD(burgersVector);
};

}   // End of namespace
