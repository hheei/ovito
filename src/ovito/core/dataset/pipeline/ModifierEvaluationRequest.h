// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>

namespace Ovito {

/**
 * Data structure representing the evaluation of a modification pipeline node.
 */
class ModifierEvaluationRequest : public PipelineEvaluationRequest
{
public:

    /// Constructor.
    ModifierEvaluationRequest(const PipelineEvaluationRequest& pipelineRequest, const ModificationNode* node) :
        PipelineEvaluationRequest(pipelineRequest), _modificationNode(const_cast<ModificationNode*>(node)) {}

    /// Constructor.
    ModifierEvaluationRequest(AnimationTime time, bool throwOnError, bool interactiveMode, const ModificationNode* node) :
        PipelineEvaluationRequest(time, throwOnError, interactiveMode), _modificationNode(const_cast<ModificationNode*>(node)) {}

    /// Returns the modification node being evaluated.
    ModificationNode* modificationNode() const { return _modificationNode; }

    /// Returns a weak reference to the modification node being evaluated.
    OOWeakRef<const PipelineNode> modificationNodeWeak() const;

    /// Returns the modifier being evaluated.
    Modifier* modifier() const;

private:

    /// The modification pipeline node being evaluated.
    OORef<ModificationNode> _modificationNode;
};

// Data structure passed to Modifier::initializeModifier():
using ModifierInitializationRequest = ModifierEvaluationRequest;

}   // End of namespace

#include "ModificationNode.h"

namespace Ovito {

/// Returns a weak reference to the modification node being evaluated.
inline OOWeakRef<const PipelineNode> ModifierEvaluationRequest::modificationNodeWeak() const {
    return _modificationNode;
}

/// Returns the modifier being evaluated.
inline Modifier* ModifierEvaluationRequest::modifier() const {
    return modificationNode()->modifier();
}

}   // End of namespace
