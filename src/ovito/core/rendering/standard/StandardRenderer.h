////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

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
		virtual std::unique_ptr<Implementation> createImplementationForVisual(RenderThread* rt, std::unique_ptr<Implementation> existingImpl) const override;

		/// Creates an implementation object that can be used by the RenderThread to render object picking images.
		virtual std::unique_ptr<Implementation> createImplementationForPicking(RenderThread* rt, std::unique_ptr<Implementation> existingImpl) const override;

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
