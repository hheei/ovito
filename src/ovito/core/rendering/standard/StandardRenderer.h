// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/rendering/SceneRenderer.h>

namespace Ovito {

/**
 * \brief The standard GPU-accelerated scene renderer of OVITO.
 */
class OVITO_CORE_EXPORT StandardRenderer : public SceneRenderer
{
    /// Defines a metaclass specialization for this renderer class.
    class OVITO_CORE_EXPORT OOMetaClass : public SceneRenderer::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using SceneRenderer::OOMetaClass::OOMetaClass;

        /// Is called by OVITO to query the class for any information that should be included in the application's system report.
        virtual void querySystemInformation(QTextStream& stream, UserInterface& userInterface) const override;
    };
    OVITO_CLASS_META(StandardRenderer, OOMetaClass)

public:

    /// Renderer-specific configuration data structure, which is passed from the GUI thread to the render thread.
    class OVITO_CORE_EXPORT Configuration : public SceneRenderer::Configuration
    {
    public:
        /// Constructor.
        Configuration(bool orderIndependentTransparency) :
            _orderIndependentTransparency(orderIndependentTransparency)
        {}

		/// Creates an implementation object that can be used by the RenderThread to render an image.
		virtual std::unique_ptr<Implementation> createImplementationForVisual(RendererService* service, std::unique_ptr<Implementation> existingImpl) const override;

		/// Creates an implementation object that can be used by the RenderThread to render object picking images.
		virtual std::unique_ptr<Implementation> createImplementationForPicking(RendererService* service, std::unique_ptr<Implementation> existingImpl) const override;

    private:
        bool _orderIndependentTransparency;
    };

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

	/// Creates a new configuration object encapsulating the current renderer configuration.
	virtual std::unique_ptr<SceneRenderer::Configuration> createConfiguration(const FrameGraph& frameGraph) const override;

	/// Supersampling factor to apply to the rendered image size.
	/// This value is used by the RenderThread to determine the size of the offscreen render target to create for this renderer.
	virtual int supersamplingFactor() const override { return std::max(1, antialiasingLevel()); }

private:

    /// Controls the supersampling factor.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{3}, antialiasingLevel, setAntialiasingLevel, PROPERTY_FIELD_RESETTABLE);

    /// Activates the order-independent rendering method for semi-transparent objects.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, orderIndependentTransparency, setOrderIndependentTransparency, PROPERTY_FIELD_RESETTABLE);
};

}   // End of namespace
