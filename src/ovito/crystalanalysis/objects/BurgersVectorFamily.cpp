// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/crystalanalysis/objects/ClusterVector.h>
#include "BurgersVectorFamily.h"
#include "MicrostructurePhase.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(BurgersVectorFamily);
DEFINE_PROPERTY_FIELD(BurgersVectorFamily, burgersVector);
DEFINE_SNAPSHOT_PROPERTY_FIELD(BurgersVectorFamily, burgersVector);
SET_PROPERTY_FIELD_LABEL(BurgersVectorFamily, burgersVector, "Burgers vector");

/******************************************************************************
* Constructor.
******************************************************************************/
void BurgersVectorFamily::initializeObject(ObjectInitializationFlags flags, int id, const QString& name)
{
    ElementType::initializeObject(flags);

    setNumericId(id);
    setName(name);
}

/******************************************************************************
* Checks if the given Burgers vector is a member of this family.
******************************************************************************/
bool BurgersVectorFamily::isMember(const Cluster::VecType& v, const MicrostructurePhase* latticeStructure) const
{
    if(burgersVector() == Vector3::Zero())
        return false;

    if(latticeStructure->crystalSymmetryClass() == MicrostructurePhase::CrystalSymmetryClass::CubicSymmetry) {

        // Bring prototype vector into canonical form.
        Vector3 sc1(std::fabs(burgersVector().x()), std::fabs(burgersVector().y()), std::fabs(burgersVector().z()));
        std::sort(sc1.data(), sc1.data() + 3);

        // Bring candidate vector into canonical form.
        Vector3 sc2(std::fabs(v.x()), std::fabs(v.y()), std::fabs(v.z()));
        std::sort(sc2.data(), sc2.data() + 3);

        return sc2.equals(sc1, CA_LATTICE_VECTOR_EPSILON);
    }
    else if(latticeStructure->crystalSymmetryClass() == MicrostructurePhase::CrystalSymmetryClass::HexagonalSymmetry) {

        // Bring prototype vector into canonical form.
        Vector3 sc1a(std::fabs(burgersVector().x()), std::fabs(burgersVector().y()), std::fabs(burgersVector().z()));
        Vector3 sc1b(
                std::fabs(0.5f*burgersVector().x()+sqrt(3.0f)/2*burgersVector().y()),
                std::fabs(0.5f*burgersVector().y()-sqrt(3.0f)/2*burgersVector().x()),
                std::fabs(burgersVector().z()));

        // Bring candidate vector into canonical form.
        Vector3 sc2(std::fabs(v.x()), std::fabs(v.y()), std::fabs(v.z()));

        return sc2.equals(sc1a, CA_LATTICE_VECTOR_EPSILON) || sc2.equals(sc1b, CA_LATTICE_VECTOR_EPSILON);
    }
    return false;
}

}   // End of namespace
