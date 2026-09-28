// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/RenderThread.h>
#include <ovito/core/utilities/concurrent/NoninteractiveContext.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(SceneRenderer);

/******************************************************************************
* Creates a renderer instance with standardized settings for rendering small
* preview images of individual objects.
******************************************************************************/
OORef<SceneRenderer> SceneRenderer::createPreviewRenderer() const
{
    OVITO_ASSERT(this_task::isMainThread());
    if(!getOOClass().isInstantiable())
        return {};
    // Establish a non-interactive context, so that the parameters of the new instance are initialized to
    // the factory default values instead of the values memorized in the user's settings store.
    NoninteractiveContext noninteractiveContext;
    return static_object_cast<SceneRenderer>(getOOClass().createInstance());
}

/******************************************************************************
* Provides a custom function that takes care of the deserialization of a
* serialized property field whose data layout has changed.
*
* Up to OVITO 3.15, the parameters of the depth-aware outline post-processing effect
* were property fields of the individual renderer classes. They are now stored in the
* RenderSettings object instead, so that they are preserved when the user switches to a
* different rendering backend. The handlers below consume the values from the file and
* park them on the renderer instance, from where RenderSettings::loadFromStreamComplete()
* picks them up once the entire object graph has been restored.
*
* This is implemented here on the common base class rather than on the individual renderer
* classes, so that a single implementation covers all rendering backends.
******************************************************************************/
RefTarget::SerializedPropertyField::CustomDeserializationFunctionPtr SceneRenderer::OOMetaClass::overrideFieldDeserialization(LoadStream& stream, const SerializedPropertyField& field) const
{
    // Only claim fields that were defined by one of the renderer classes. Note that the check cannot
    // compare against SceneRenderer::OOClass() itself, because in the file the fields are attributed to
    // whichever concrete renderer class declared them.
    if(field.definingClass == nullptr || !field.definingClass->isDerivedFrom(SceneRenderer::OOClass()))
        return RefTarget::OOMetaClass::overrideFieldDeserialization(stream, field);

    if(field.identifier == "outlinesEnabled") {
        return [](const SerializedPropertyField& field, ObjectLoadStream& stream, RefMaker& owner) {
            bool enabled;
            stream >> enabled;
            static_cast<SceneRenderer&>(owner).legacyOutlineSettings().enabled = enabled;
        };
    }
    if(field.identifier == "minDepthDiff") {
        return [](const SerializedPropertyField& field, ObjectLoadStream& stream, RefMaker& owner) {
            FloatType value;
            stream >> value;
            static_cast<SceneRenderer&>(owner).legacyOutlineSettings().minDepthDiff = value;
        };
    }
    if(field.identifier == "maxDepthDiff") {
        return [](const SerializedPropertyField& field, ObjectLoadStream& stream, RefMaker& owner) {
            FloatType value;
            stream >> value;
            static_cast<SceneRenderer&>(owner).legacyOutlineSettings().maxDepthDiff = value;
        };
    }
    if(field.identifier == "useCustomOutlineColor") {
        return [](const SerializedPropertyField& field, ObjectLoadStream& stream, RefMaker& owner) {
            bool useCustomColor;
            stream >> useCustomColor;
            static_cast<SceneRenderer&>(owner).legacyOutlineSettings().useCustomColor = useCustomColor;
        };
    }
    if(field.identifier == "outlineColor") {
        return [](const SerializedPropertyField& field, ObjectLoadStream& stream, RefMaker& owner) {
            Color color;
            stream >> color;
            static_cast<SceneRenderer&>(owner).legacyOutlineSettings().customColor = color;
        };
    }

    // The two outline width fields used to be stored as integers in the file, but became
    // floating-point values in OVITO 3.16 (file format version 30017).
    if(field.identifier == "minOutlineWidth") {
        if(stream.formatVersion() < 30017) {
            return [](const SerializedPropertyField& field, ObjectLoadStream& stream, RefMaker& owner) {
                int width;
                stream >> width;
                static_cast<SceneRenderer&>(owner).legacyOutlineSettings().minOutlineWidth = width;
            };
        }
        return [](const SerializedPropertyField& field, ObjectLoadStream& stream, RefMaker& owner) {
            FloatType width;
            stream >> width;
            static_cast<SceneRenderer&>(owner).legacyOutlineSettings().minOutlineWidth = width;
        };
    }
    if(field.identifier == "maxOutlineWidth") {
        if(stream.formatVersion() < 30017) {
            return [](const SerializedPropertyField& field, ObjectLoadStream& stream, RefMaker& owner) {
                int width;
                stream >> width;
                static_cast<SceneRenderer&>(owner).legacyOutlineSettings().maxOutlineWidth = width;
            };
        }
        return [](const SerializedPropertyField& field, ObjectLoadStream& stream, RefMaker& owner) {
            FloatType width;
            stream >> width;
            static_cast<SceneRenderer&>(owner).legacyOutlineSettings().maxOutlineWidth = width;
        };
    }

    return RefTarget::OOMetaClass::overrideFieldDeserialization(stream, field);
}

/******************************************************************************
* Constructor.
******************************************************************************/
SceneRenderer::Implementation::Implementation(RendererService* service) : _service(service), _rhi(service->rhi())
{
}

/******************************************************************************
* Records a non-fatal warning encountered during rendering.
******************************************************************************/
void SceneRenderer::Implementation::reportWarning(const QString& message)
{
    _service->reportWarning(message);
}

#ifdef OVITO_BUILD_BASIC
/******************************************************************************
* Returns an image mask serving as watermark for demo versions of scene renderers.
******************************************************************************/
const QImage& SceneRenderer::watermark()
{
    static const QImage watermark = []() {
        QFont font;
        font.setPointSize(36);
        font.setBold(true);
        QFontMetrics fm(font);
        QRect boundingRect = fm.boundingRect("OVITO Pro Demo");
        boundingRect.adjust(-20, -20, 20, 20);

        QImage watermark(boundingRect.size(), QImage::Format_RGBA8888);
        watermark.fill(QColor(0, 0, 0, 0));
        QPainter painter(&watermark);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setPen(QColor(128, 128, 128, 255));
        painter.setFont(font);
        painter.drawText(watermark.rect(), Qt::AlignCenter, "OVITO Pro Demo");
        return watermark;
    }();

    return watermark;
}
#endif

}   // End of namespace
