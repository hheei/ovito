// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/dataset/animation/TimeInterval.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include <ovito/core/utilities/concurrent/SharedFuture.h>

namespace Ovito {

/**
 * \brief This class holds the results of an pipeline evaluation request.
 */
class OVITO_CORE_EXPORT PipelineEvaluationResult : public SharedFuture<PipelineFlowState>
{
public:

    enum EvaluationType {
        Noninteractive = (1 << 0),
        Interactive = (1 << 1),
        Both = Noninteractive | Interactive
    };
    Q_DECLARE_FLAGS(EvaluationTypes, EvaluationType);

    /// Null constructor.
    PipelineEvaluationResult() = default;

    /// Constructs an immediate result.
    PipelineEvaluationResult(const PipelineFlowState& state, EvaluationTypes evaluationTypes = EvaluationType::Both) : SharedFuture<PipelineFlowState>(Future<PipelineFlowState>::createImmediate(state)), _evaluationTypes(evaluationTypes), _validityInterval(state.stateValidity()) {}

    /// Constructs a future result.
    PipelineEvaluationResult(SharedFuture<PipelineFlowState> future, EvaluationTypes evaluationTypes, const TimeInterval& validityInterval) : SharedFuture<PipelineFlowState>(std::move(future)), _evaluationTypes(evaluationTypes), _validityInterval(validityInterval) {}

    /// Constructs a failed state.
    PipelineEvaluationResult(std::exception_ptr exception) : SharedFuture<PipelineFlowState>(Future<PipelineFlowState>::createFailed(std::move(exception))) {}

    /// Constructs a failed state.
    PipelineEvaluationResult(Exception&& exception) : SharedFuture<PipelineFlowState>(Future<PipelineFlowState>::createFailed(std::move(exception))) {}

    /// Returns the animation time interval over which the pipeline output will remain valid (constant) once it has been computed.
    const TimeInterval& validityInterval() const { return _validityInterval; }

    /// Reduces the validity interval of this pipeline result to include only the given animation time interval.
    void intersectValidityInterval(const TimeInterval& intersectionInterval) { _validityInterval.intersect(intersectionInterval); }

    /// Returns the type(s) of pipeline evaluation this result is from.
    EvaluationTypes evaluationTypes() const { return _evaluationTypes; }

    /// Sets the type(s) of pipeline evaluation this result is from.
    void setEvaluationTypes(EvaluationTypes evaluationTypes) { _evaluationTypes = evaluationTypes; }

    /// Turns this result object into a regular future.
    SharedFuture<PipelineFlowState>&& asFuture() && { return std::move(*this); }

private:

    /// The type(s) of pipeline evaluation this result is from.
    EvaluationTypes _evaluationTypes = EvaluationType::Both;

    /// The animation time interval over which the pipeline result remains valid (during which it doesn't change).
    TimeInterval _validityInterval = TimeInterval::infinite();
};

}   // End of namespace
