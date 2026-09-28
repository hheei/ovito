// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ModifierGroup);
OVITO_CLASSINFO(ModifierGroup, "DisplayName", "Modifier group");
DEFINE_PROPERTY_FIELD(ModifierGroup, isCollapsed);
SET_PROPERTY_FIELD_LABEL(ModifierGroup, isCollapsed, "Collapsed");

/******************************************************************************
* This is called from a ModificationNode whenever it becomes a member of this group.
******************************************************************************/
void ModifierGroup::registerNode(ModificationNode* node)
{
    updateCombinedStatus();
    notifyDependents(ModifierGroup::ModifierAddedOrRemoved);
}

/******************************************************************************
* This is called from a ModificationNode whenever it is removed from this group.
******************************************************************************/
void ModifierGroup::unregisterNode(ModificationNode* node)
{
    updateCombinedStatus();
    notifyDependents(ModifierGroup::ModifierAddedOrRemoved);
}

/******************************************************************************
* Is called when one of the group's nodes has generated an event.
******************************************************************************/
void ModifierGroup::modificationNodeStatusChanged()
{
    // Update the group's status whenever the status of one of its members changes.
    updateCombinedStatus();
}

/******************************************************************************
* This is called whenever one of the group's member nodes change.
* It computes the combined status of the entire group.
******************************************************************************/
void ModifierGroup::updateCombinedStatus()
{
    bool isActive = false;
    PipelineStatus combinedStatus(PipelineStatus::Success);
    if(isEnabled()) {
        visitDependents([&](RefMaker* dependent) {
            if(ModificationNode* node = dynamic_object_cast<ModificationNode>(dependent)) {
                OVITO_ASSERT(node->modifierGroup() == this);
                if(node->isObjectActive())
                    isActive = true;

                if(node->modifier() && node->modifier()->isEnabled()) {
                    const PipelineStatus& nodeStatus = node->status();
                    if(combinedStatus.type() == PipelineStatus::Success || nodeStatus.type() == PipelineStatus::Error)
                        combinedStatus.setType(nodeStatus.type());
                    if(!nodeStatus.text().isEmpty()) {
                        if(!combinedStatus.text().isEmpty())
                            combinedStatus.setText(combinedStatus.text() + QStringLiteral("\n") + nodeStatus.text());
                        else
                            combinedStatus.setText(nodeStatus.text());
                    }
                }
            }
        });
    }

    if(!isObjectActive() && isActive)
        incrementNumberOfActiveTasks();
    else if(isObjectActive() && !isActive)
        decrementNumberOfActiveTasks();
    setStatus(std::move(combinedStatus));
}

/******************************************************************************
* Returns the list of pipeline nodes that are part of this group.
******************************************************************************/
QVector<ModificationNode*> ModifierGroup::nodes() const
{
    QVector<ModificationNode*> nodes;
    visitDependents([&](RefMaker* dependent) {
        if(ModificationNode* node = dynamic_object_cast<ModificationNode>(dependent))
            nodes.push_back(node);
    });
    if(!nodes.empty()) {
        // Order the nodes according to their sequence in the data pipeline.
        std::ranges::sort(nodes, [](ModificationNode* a, ModificationNode* b) {
            return b->isReferencedBy(a);
        });
#ifdef OVITO_DEBUG
        // The input (successor) of the last node (the group's tail) should not be part of the modifier group.
        ModificationNode* successor = !nodes.empty() ? dynamic_object_cast<ModificationNode>(nodes.back()->input()) : nullptr;
        OVITO_ASSERT(!successor || successor->modifierGroup() != this);
        // All others should be referenced by the group's head node. This ensures that the node are all from the same pipeline branch.
        for(ModificationNode* node : nodes) {
            OVITO_ASSERT(node->modifierGroup() == this);
            OVITO_ASSERT(node->isReferencedBy(nodes.front()));
        }
#endif
    }

    return nodes;
}

/******************************************************************************
* Returns the list of pipelines that contain this modifier group.
******************************************************************************/
QSet<Pipeline*> ModifierGroup::pipelines(bool onlyScenePipelines) const
{
    QSet<Pipeline*> pipelinesList;
    visitDependents([&](RefMaker* dependent) {
        if(ModificationNode* node = dynamic_object_cast<ModificationNode>(dependent))
            pipelinesList.unite(node->pipelines(onlyScenePipelines));
    });
    return pipelinesList;
}

}   // End of namespace
