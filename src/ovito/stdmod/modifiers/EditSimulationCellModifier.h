// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdmod/StdMod.h>
#include <ovito/core/dataset/pipeline/Modifier.h>

namespace Ovito {

/**
 * \brief Lets the user edit the simulation cell parameters and boundary conditions.
 */
class OVITO_STDMOD_EXPORT EditSimulationCellModifier : public Modifier
{
    OVITO_CLASS(EditSimulationCellModifier)

public:

    /// This method is called by the system after the modifier has been inserted into a data pipeline.
    virtual void initializeModifier(const ModifierInitializationRequest& request) override;

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

    /// Indicates whether the interactive viewports should be updated after a parameter of the modifier has
    /// been changed and before the entire pipeline is recomputed.
    virtual bool shouldRefreshViewportsAfterChange() override { return true; }

    /// Setter variants used by the Python bindings. In addition to assigning the value, they unconditionally record
    /// that the parameter has been specified explicitly. Relying on propertyChanged() alone is not sufficient here,
    /// because assigning a parameter its current value (e.g. 'EditSimulationCellModifier(pbc_z=False)') does not
    /// generate a change notification, and the parameter would then still be overwritten by initializeModifier().
    void setPbcXPYTHON(bool on) { _uninitializedPbcX = false; setPbcX(on); }
    void setPbcYPYTHON(bool on) { _uninitializedPbcY = false; setPbcY(on); }
    void setPbcZPYTHON(bool on) { _uninitializedPbcZ = false; setPbcZ(on); }
    void setIs2DPYTHON(bool on) { _uninitializedDimensionality = false; setIs2D(on); }

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:

    /// The three cell vectors and the cell origin.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(AffineTransformation{AffineTransformation::Zero()}, cellMatrix, setCellMatrix);

    /// Specifies whether the modifier will override the cell geometry.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, replaceCell, setReplaceCell);

    /// Specifies periodic boundary condition in the X direction.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, pbcX, setPbcX);
    /// Specifies periodic boundary condition in the Y direction.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, pbcY, setPbcY);
    /// Specifies periodic boundary condition in the Z direction.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, pbcZ, setPbcZ);

    /// The dimensionality of the system.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, is2D, setIs2D);

    /// Flags indicating that the PBC or dimensionality parameters have not been set by the user (or Python API)
    /// before the modifier is inserted into a pipeline. In this case, they are initialized from the upstream input cell
    /// in the initializeModifier() method. Note that the three PBC directions are tracked individually, because a
    /// script may set just one of them when constructing the modifier and still expect the other two to be inherited
    /// from the input cell.
    bool _uninitializedPbcX = true;
    bool _uninitializedPbcY = true;
    bool _uninitializedPbcZ = true;
    bool _uninitializedDimensionality = true;
};

}   // End of namespace
