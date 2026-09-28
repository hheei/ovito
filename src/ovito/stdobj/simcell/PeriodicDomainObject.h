// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdobj/StdObj.h>
#include <ovito/core/dataset/data/DataObject.h>
#include <ovito/stdobj/simcell/SimulationCell.h>

namespace Ovito {

/**
 * \brief Base class for geometry objects that are embedded in a spatial domain that may be periodic.
 */
class OVITO_STDOBJ_EXPORT PeriodicDomainObject : public DataObject
{
    OVITO_CLASS(PeriodicDomainObject)

public:

    /// Returns the spatial domain this geometry is embedded in after making sure it
    /// can safely be modified.
    SimulationCell* mutableDomain() {
        return makeMutable(domain());
    }

    /// Returns the display title of this object.
    virtual QString objectTitle() const override;

    /// Tests whether the given spatial point is culled by the cutting planes set for this object.
    bool isPointCulled(const Point3& p) const {
        for(const Plane3& plane : cuttingPlanes()) {
            if(plane.classifyPoint(p) > 0)
                return true;
        }
        return false;
    }

protected:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags, const QString& title = {});

private:

    /// The spatial domain (possibly periodic) this geometry object is embedded in.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(DataOORef<const SimulationCell>, domain, setDomain, PROPERTY_FIELD_NO_SUB_ANIM);

    /// The planar cuts to be applied to geometry after its has been transformed into a non-periodic representation.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QVector<Plane3>{}, cuttingPlanes, setCuttingPlanes);

    /// The assigned title of the data object, which is displayed in the user interface.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{}, title, setTitle);
};

}   // End of namespace
