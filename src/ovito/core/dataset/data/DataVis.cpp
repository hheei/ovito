// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/DataSet.h>
#include "DataVis.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(DataVis);

/******************************************************************************
* Returns all pipelines that produced this visualization element.
******************************************************************************/
QSet<Pipeline*> DataVis::pipelines(bool onlyScenePipelines) const
{
    OVITO_ASSERT_MSG(this_task::isMainThread(), "DataVis::pipelines", "This function may only be called from the main thread.");

    QSet<Pipeline*> pipelinesList;
    visitDependents([&](RefMaker* dependent) {
        if(Pipeline* pipeline = dynamic_object_cast<Pipeline>(dependent)) {
            if(pipeline->visElements().contains(const_cast<DataVis*>(this))) {
                if(!onlyScenePipelines || pipeline->isInScene())
                    pipelinesList.insert(pipeline);
            }
        }
    });
    return pipelinesList;
}

/******************************************************************************
* Replaces this visual element with a shared visual element
* by telling all dependents to update their references.
******************************************************************************/
void DataVis::replaceWithSharedElement(DataVis* sharedVis) const
{
    // Must be a compatible visual element.
    OVITO_ASSERT(&sharedVis->getOOMetaClass() == &getOOMetaClass());

    for(Pipeline* pipeline : pipelines(true)) {
        // Ask the pipeline nodes to update references they have to this visual element with the shared one.
        pipeline->replaceVisualElement(const_cast<DataVis*>(this), [sharedVis](const QString&) -> OORef<DataVis> { return sharedVis; });
    }
}

}   // End of namespace
