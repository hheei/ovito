// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/ParticlesVis.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/rendering/FrameBuffer.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/standard/StandardRenderer.h>
#include <ovito/core/rendering/RenderThread.h>
#include "AmbientOcclusionModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(AmbientOcclusionModifier);
OVITO_CLASSINFO(AmbientOcclusionModifier, "DisplayName", "Ambient occlusion");
OVITO_CLASSINFO(AmbientOcclusionModifier, "Description", "Perform an ambient occlusion calculation to shade particles.");
OVITO_CLASSINFO(AmbientOcclusionModifier, "ModifierCategory", "Coloring");
DEFINE_PROPERTY_FIELD(AmbientOcclusionModifier, intensity);
DEFINE_PROPERTY_FIELD(AmbientOcclusionModifier, samplingCount);
DEFINE_PROPERTY_FIELD(AmbientOcclusionModifier, bufferResolution);
SET_PROPERTY_FIELD_LABEL(AmbientOcclusionModifier, intensity, "Shading intensity");
SET_PROPERTY_FIELD_LABEL(AmbientOcclusionModifier, samplingCount, "Number of exposure samples");
SET_PROPERTY_FIELD_LABEL(AmbientOcclusionModifier, bufferResolution, "Render buffer resolution");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(AmbientOcclusionModifier, intensity, PercentParameterUnit, 0, 1);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(AmbientOcclusionModifier, samplingCount, IntegerParameterUnit, 3, 2000);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(AmbientOcclusionModifier, bufferResolution, IntegerParameterUnit, 1, AmbientOcclusionModifier::MAX_AO_RENDER_BUFFER_RESOLUTION);

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool AmbientOcclusionModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
 * Is called by the pipeline system before a new modifier evaluation begins.
 ******************************************************************************/
void AmbientOcclusionModifier::preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const
{
    // Indicate that we will do different computations depending on whether the pipeline is evaluated in interactive mode or not.
    if(request.interactiveMode())
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Interactive;
    else
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Noninteractive;
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> AmbientOcclusionModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    // In interactive mode, do not perform a real computation. Instead, reuse an old result from the cached state if available.
    if(request.interactiveMode()) {
        if(PipelineFlowState cachedState = request.modificationNode()->getCachedPipelineNodeOutput(request.time(), true)) {
            if(DataOORef<const Particles> cachedParticles = cachedState.getObject<Particles>()) {
                Particles* particles = state.expectMutableObject<Particles>();
                particles->verifyIntegrity();
                const Property* cachedColors = cachedParticles->getProperty(Particles::ColorProperty);
                return asyncLaunch([state = std::move(state), particles, cachedColors, cachedParticles = std::move(cachedParticles)]() mutable {
                    particles->tryToAdoptProperties(cachedParticles, {cachedColors}, {particles});
                    return std::move(state);
                });
            }
        }
        return std::move(state);
    }

    // Special case handling: there are no particles to shade.
    const Particles* particles = state.expectObject<Particles>();
    particles->verifyIntegrity();
    if(particles->elementCount() == 0) {
        state.makeMutable(particles)->createProperty(DataBuffer::Initialized, Particles::ColorProperty, {particles});
        return std::move(state);
    }

    // Phase I: Perform the particle occlusion calculation. The results are cached in the node's partial cache.
    auto brightnessFuture = request.modificationNode()->partialResultsCache().getOrCompute(state.data(), std::bind_front(&AmbientOcclusionModifier::computeAmbientOcclusion, this, particles));

    // Phase II: Modulate input particle colors with the computed brightness values.
    return brightnessFuture.then(ObjectExecutor(this), [this, state = std::move(state)](ConstDataBufferPtr brightness) {

        // Perform work in a separate thread.
        return asyncLaunch([state = std::move(state), brightness = std::move(brightness), intensity = intensity()]() mutable {

            Particles* particles = state.expectMutableObject<Particles>();
            OVITO_ASSERT(brightness && particles->elementCount() == brightness->size());

            GraphicsFloatType effIntensity = qBound(GraphicsFloatType(0), static_cast<GraphicsFloatType>(intensity), GraphicsFloatType(1));

            BufferReadAccess<GraphicsFloatType> brightnessAcc(brightness);
            BufferWriteAccess<ColorG, access_mode::read_write> colorAcc = particles->createProperty(DataBuffer::Initialized, Particles::ColorProperty, {particles});

            const GraphicsFloatType* __restrict b = brightnessAcc.cbegin();
            for(ColorG& c : colorAcc) {
                GraphicsFloatType factor = std::min(GraphicsFloatType(1) - effIntensity + *b, GraphicsFloatType(1));
                c = c * factor;
                ++b;
            }

            return std::move(state);
        });
    });
}

/******************************************************************************
* Calculates the ambient occlusion values for the given particles and returns them in a data buffer.
******************************************************************************/
Future<ConstDataBufferPtr> AmbientOcclusionModifier::computeAmbientOcclusion(DataOORef<const Particles> particles) const
{
    // Capture local copies of the input parameters in the coroutine state.
    const auto resolution = 128 << qBound(0, this->bufferResolution(), (int)MAX_AO_RENDER_BUFFER_RESOLUTION);
    const auto samplingCount = std::max(1, this->samplingCount());
    const auto self = OORef<AmbientOcclusionModifier>(this);

    TaskProgress progress(this_task::ui());
    progress.setText(tr("Ambient occlusion"));

    // Perform the following in a worker thread.
    co_await ExecutorAwaiter(ThreadPoolExecutor());

    // Get particle radii.
    ConstPropertyPtr radii = particles->inputParticleRadii();
    this_task::throwIfCanceled();

    // Compute bounding box of input particles (and include particle radii).
    const Property* positions = particles->expectProperty(Particles::PositionProperty);
    Box3 boundingBox = positions->boundingBox3();
    OVITO_ASSERT(!boundingBox.isEmpty());
    boundingBox = boundingBox.padBox(std::max(FloatType(0), radii->minMax().second));
    this_task::throwIfCanceled();

    // Create output array.
    DataBufferPtr brightness = DataBufferPtr::create(DataBuffer::Initialized, particles->elementCount(), DataBuffer::FloatGraphics, 1);

    // Continue execution in the main thread in order to access the global RenderThread.
    co_await ExecutorAwaiter(ObjectExecutor(this));

    // Request an AO sampling frame buffer of the right size from the RenderThread.
    RenderTarget renderTarget = this_task::ui()->renderThread()->createOffscreenTarget(QSize(resolution, resolution), true);

    // Create a frame graph that can be submitted to the RenderThread for offscreen rendering.
    OORef<FrameGraph> frameGraph = OORef<FrameGraph>::create(
        this_task::ui()->datasetContainer().visCache()->acquireResourceFrame(),
        AnimationTime(0), ViewProjectionParameters{}, QSize(resolution, resolution), false, false, false, 1.0);
    frameGraph->setClearColor(ColorA(0,0,0,0));
    this_task::throwIfCanceled();

    // Add the particles to the frame graph.
    std::unique_ptr<ParticlePrimitive> particleBuffer = std::make_unique<ParticlePrimitive>();
    particleBuffer->setShadingMode(ParticlePrimitive::FlatShading);
    particleBuffer->setRenderingQuality(ParticlePrimitive::LowQuality);
    particleBuffer->setPositions(positions);
    particleBuffer->setRadii(radii);
    frameGraph->addCommandGroup(FrameGraph::SceneLayer).addPrimitive(std::move(particleBuffer), AffineTransformation::Identity(), boundingBox, OORef<const SceneNode>{});
    OVITO_ASSERT(frameGraph->commandGroups().size() == 1);
    OVITO_ASSERT(frameGraph->commandGroups().front().commands().size() == 1);
    OVITO_ASSERT(frameGraph->commandGroups().front().commands().front().skipInPickingPass() == false);
    this_task::throwIfCanceled();

    // Release data that is no longer needed to reduce memory footprint.
    particles.reset();

    // Switch back to a worker thread.
    co_await ExecutorAwaiter(ThreadPoolExecutor());

    // AO sampling loop.
    progress.setMaximum(samplingCount);
    for(int sample = 0; sample < samplingCount; sample++) {
        progress.setValue(sample);
        this_task::throwIfCanceled();

        // Generate lighting direction on unit sphere using "Fibonacci sphere algorithm".
        // https://stackoverflow.com/a/26127012
        FloatType y = FloatType(1) - (sample / FloatType(samplingCount - 1)) * 2; // y goes from 1 to -1
        FloatType r = std::sqrt(FloatType(1) - y * y); // radius at y
        FloatType phi = (FloatType)sample * Ovito::pi * (FloatType(3) - std::sqrt(FloatType(5)));
        Vector3 dir(std::cos(phi)*r, y, std::sin(phi)*r);
        OVITO_ASSERT(std::abs(dir.length() - 1.0) < Ovito::epsilon);

        // Set up view projection.
        ViewProjectionParameters projParams;
        projParams.viewMatrix = AffineTransformation::lookAlong(boundingBox.center(), dir, Vector3(0,0,1));

        // Transform bounding box to camera space.
        Box3 bb = boundingBox.transformed(projParams.viewMatrix).centerScale(FloatType(1.01));

        // Complete projection parameters.
        projParams.aspectRatio = 1;
        projParams.isPerspective = false;
        projParams.inverseViewMatrix = projParams.viewMatrix.inverse();
        projParams.fieldOfView = FloatType(0.5) * boundingBox.size().length();
        projParams.znear = -bb.maxc.z();
        projParams.zfar  = std::max(-bb.minc.z(), projParams.znear + FloatType(1));
        projParams.projectionMatrix = Matrix4::ortho(-projParams.fieldOfView, projParams.fieldOfView,
                            -projParams.fieldOfView, projParams.fieldOfView,
                            projParams.znear, projParams.zfar);
        projParams.inverseProjectionMatrix = projParams.projectionMatrix.inverse();
        projParams.validityInterval = TimeInterval::infinite();
        frameGraph->setProjectionParams(projParams);

        // Render the current view to the frame buffer.
        auto [objectIdBuffer, primitiveIdBuffer] = co_await FutureAwaiter(ThreadPoolExecutor(), renderTarget.renderAOFrame(frameGraph));

        // Extract brightness values from rendered image.
        OVITO_ASSERT(objectIdBuffer.size() == resolution * resolution * 4);
        OVITO_ASSERT(primitiveIdBuffer.size() == resolution * resolution * 4);
        BufferWriteAccess<GraphicsFloatType, access_mode::read_write> brightnessValues(brightness);
        const uint32_t* objectId = reinterpret_cast<const uint32_t*>(objectIdBuffer.constData());
        const uint32_t* primitiveId = reinterpret_cast<const uint32_t*>(primitiveIdBuffer.constData());
        for(int y = 0; y < resolution; y++) {
            for(int x = 0; x < resolution; x++, ++objectId, ++primitiveId) {
                if(*objectId == 0)
                    continue; // Background pixel, no particle here.
                uint32_t particleIndex = *primitiveId;
                OVITO_ASSERT(particleIndex < brightnessValues.size());
                brightnessValues[particleIndex] += 1;
            }
        }
    }
    progress.setValue(samplingCount);

    // Normalize brightness values by particle area.
    BufferReadAccess<GraphicsFloatType> radiusArray(radii);
    BufferWriteAccess<GraphicsFloatType, access_mode::read_write> brightnessValues(brightness);
    auto r = radiusArray.cbegin();
    GraphicsFloatType maxBrightness = 0;
    for(GraphicsFloatType& b : brightnessValues) {
        if(*r != 0)
            b /= (*r) * (*r);
        maxBrightness = std::max(maxBrightness, b);
        ++r;
    }
    this_task::throwIfCanceled();

    // Normalize brightness values by global maximum.
    if(maxBrightness != 0) {
        for(GraphicsFloatType& b : brightnessValues) {
            b /= maxBrightness;
        }
    }
    brightnessValues.reset();

    co_return brightness;
}


}   // End of namespace
