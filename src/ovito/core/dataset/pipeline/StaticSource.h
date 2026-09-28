// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/dataset/data/DataCollection.h>
#include "PipelineNode.h"

namespace Ovito {

/**
 * \brief A source pipeline node returning a static data collection.
 */
class OVITO_CORE_EXPORT StaticSource : public PipelineNode
{
    OVITO_CLASS(StaticSource)

public:

    /// Returns the list of data objects that are managed by this data source.
    /// The returned data objects will be displayed as sub-objects of the data source in the pipeline editor.
    virtual const DataCollection* getSourceDataCollection() const override { return dataCollection(); }

    /// Replaces all references to the given visual element in the pipeline with new compatible objects.
    virtual void replaceVisualElement(DataVis* visElement, const std::function<OORef<DataVis>(const QString&)>& getReplacement) override;

protected:

    /// Called by the pipeline system before a new evaluation begins to query the validity interval and evaluation result type of this pipeline stage.
    virtual void preevaluateInternal(const PipelineEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) override {}

    /// Asks the object for the result of the data pipeline.
    virtual SharedFuture<PipelineFlowState> evaluateInternal(const PipelineEvaluationRequest& request) override;

    /// Handles reference events sent by reference targets of this object.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// Is called when a RefTarget referenced by this object has been replaced with a different one.
    virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

private:

    /// The data collection owned by this source.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(DataOORef<const DataCollection>, dataCollection, setDataCollection);
};

}   // End of namespace
