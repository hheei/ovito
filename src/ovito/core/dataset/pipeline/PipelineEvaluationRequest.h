// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/dataset/animation/TimeInterval.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include <ovito/core/utilities/concurrent/SharedFuture.h>

namespace Ovito {

/**
 * \brief This class holds the parameters for an evaluation request of a data pipeline.
 */
class OVITO_CORE_EXPORT PipelineEvaluationRequest
{
public:

    /// Constructs a request object for evaluating the pipeline at a certain animation time.
    PipelineEvaluationRequest(AnimationTime time = AnimationTime::fromFrame(0), bool throwOnError = false, bool interactiveMode = false) :
        _time(time),
        _throwOnError(throwOnError),
        _interactiveMode(interactiveMode),
        _cachingIntervals(time) {}

    /// Constructs a request object for evaluating the pipeline using a prescribed caching pattern.
    explicit PipelineEvaluationRequest(const TimeIntervalUnion& cachingIntervals) : _cachingIntervals(cachingIntervals) {}

    /// Returns the animation time at which the pipeline is being evaluated.
    AnimationTime time() const { return _time; }

    /// Sets a new animation time at which the pipeline should be evaluated.
    void setTime(AnimationTime time) { _time = time; }

    /// Indicates whether the pipeline system should abort the evaluation by throwing an exception as soon as a first error occurs in one of the pipeline stages.
    bool throwOnError() const { return _throwOnError; }

    /// Sets whether the pipeline system should abort the evaluation by throwing an exception as soon as a first error occurs in one of the pipeline stages.
    void setThrowOnError(bool enable = true) { _throwOnError = enable; }

    /// Returns whether long-running pipeline steps should be skipped. In interactive mode,
    /// the pipeline system may choose to skip the evaluation of certain pipeline stages in order to
    /// generate a preliminary result faster. This is useful for interactive rendering in the viewports.
    bool interactiveMode() const { return _interactiveMode; }

    /// Sets whether long-running pipeline steps should be skipped.
    void setInteractiveMode(bool interactive) { _interactiveMode = interactive; }

    /// Returns the animation time intervals over which the pipeline should pre-cache the state.
    const TimeIntervalUnion& cachingIntervals() const { return _cachingIntervals; }

    /// Returns a non-const reference to the animation time intervals over which the pipeline should pre-cache the state.
    TimeIntervalUnion& mutableCachingIntervals() { return _cachingIntervals; }

private:

    /// The animation time at which the pipeline is being evaluated.
    AnimationTime _time = AnimationTime::fromFrame(0);

    /// Controls whether the pipeline system should abort the evaluation by throwing an exception as soon as a first error occurs in one of the modifiers.
    bool _throwOnError = false;

    /// Controls whether long-running pipeline steps should be skipped. In interactive mode,
    /// the pipeline system may choose to skip the evaluation of certain pipeline stages in order to
    /// generate a preliminary result faster. This is useful for interactive rendering in the viewports.
    bool _interactiveMode = false;

    /// Indicates to the upstream pipeline stages which animation frames they should keep in the cache.
    TimeIntervalUnion _cachingIntervals;
};

}   // End of namespace
