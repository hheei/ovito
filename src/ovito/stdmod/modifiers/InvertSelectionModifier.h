// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdmod/StdMod.h>
#include <ovito/stdobj/properties/GenericPropertyModifier.h>

namespace Ovito {

/**
 * \brief This modifier inverts the selection status of each element.
 */
class OVITO_STDMOD_EXPORT InvertSelectionModifier : public GenericPropertyModifier
{
    OVITO_CLASS(InvertSelectionModifier)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

    /// Indicates whether the interactive viewports should be updated after a parameter of the modifier has
    /// been changed and before the entire pipeline is recomputed.
    virtual bool shouldRefreshViewportsAfterChange() override { return true; }
};

}   // End of namespace
