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

#include <ovito/stdobj/StdObj.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/rendering/TextPrimitive.h>
#include <ovito/core/rendering/TextBillboardPrimitive.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include "TextLabelsVis.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(TextLabelsVis);
OVITO_CLASSINFO(TextLabelsVis, "DisplayName", "Labels");
DEFINE_PROPERTY_FIELD(TextLabelsVis, font);
DEFINE_PROPERTY_FIELD(TextLabelsVis, fontSize);
DEFINE_PROPERTY_FIELD(TextLabelsVis, textColor);
DEFINE_PROPERTY_FIELD(TextLabelsVis, outlineEnabled);
DEFINE_PROPERTY_FIELD(TextLabelsVis, outlineColor);
DEFINE_PROPERTY_FIELD(TextLabelsVis, alignment);
DEFINE_PROPERTY_FIELD(TextLabelsVis, elementAnchor);
DEFINE_PROPERTY_FIELD(TextLabelsVis, offsetX);
DEFINE_PROPERTY_FIELD(TextLabelsVis, offsetY);
DEFINE_PROPERTY_FIELD(TextLabelsVis, depthOffset);
DEFINE_PROPERTY_FIELD(TextLabelsVis, alwaysInFront);
DEFINE_PROPERTY_FIELD(TextLabelsVis, backgroundEnabled);
DEFINE_PROPERTY_FIELD(TextLabelsVis, backgroundColor);
DEFINE_PROPERTY_FIELD(TextLabelsVis, maxLabelCount);
SET_PROPERTY_FIELD_LABEL(TextLabelsVis, font, "Font");
SET_PROPERTY_FIELD_LABEL(TextLabelsVis, fontSize, "Font size");
SET_PROPERTY_FIELD_LABEL(TextLabelsVis, textColor, "Text color");
SET_PROPERTY_FIELD_LABEL(TextLabelsVis, outlineEnabled, "Text outline");
SET_PROPERTY_FIELD_LABEL(TextLabelsVis, outlineColor, "Outline color");
SET_PROPERTY_FIELD_LABEL(TextLabelsVis, alignment, "Position");
SET_PROPERTY_FIELD_LABEL(TextLabelsVis, elementAnchor, "Anchor");
SET_PROPERTY_FIELD_LABEL(TextLabelsVis, offsetX, "Offset X");
SET_PROPERTY_FIELD_LABEL(TextLabelsVis, offsetY, "Offset Y");
SET_PROPERTY_FIELD_LABEL(TextLabelsVis, depthOffset, "Depth offset");
SET_PROPERTY_FIELD_LABEL(TextLabelsVis, alwaysInFront, "Always in front");
SET_PROPERTY_FIELD_LABEL(TextLabelsVis, backgroundEnabled, "Background");
SET_PROPERTY_FIELD_LABEL(TextLabelsVis, backgroundColor, "Background color");
SET_PROPERTY_FIELD_LABEL(TextLabelsVis, maxLabelCount, "Maximum number of labels");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TextLabelsVis, fontSize, FloatParameterUnit, 0, 1);
SET_PROPERTY_FIELD_UNITS(TextLabelsVis, offsetX, PercentParameterUnit);
SET_PROPERTY_FIELD_UNITS(TextLabelsVis, offsetY, PercentParameterUnit);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(TextLabelsVis, depthOffset, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(TextLabelsVis, maxLabelCount, IntegerParameterUnit, 0);

/******************************************************************************
* Constructor.
******************************************************************************/
void TextLabelsVis::initializeObject(ObjectInitializationFlags flags)
{
    DataVis::initializeObject(flags);
}

/******************************************************************************
* Translates the label placement into the alignment flags of a text primitive.
*
* The 'alignment' parameter of this vis element specifies on which side of a data element
* its label is placed. A text primitive, in contrast, interprets its alignment flags as the
* corner of the text box that gets pinned to the anchor point. Both are mirror images of each
* other, e.g. a label that should appear to the right of an element must have its left edge
* pinned to the element's position.
******************************************************************************/
static int labelPlacementToTextAlignment(int placement)
{
    int alignment = 0;

    if(placement & Qt::AlignLeft) alignment |= Qt::AlignRight;
    else if(placement & Qt::AlignRight) alignment |= Qt::AlignLeft;
    else alignment |= Qt::AlignHCenter;

    if(placement & Qt::AlignTop) alignment |= Qt::AlignBottom;
    else if(placement & Qt::AlignBottom) alignment |= Qt::AlignTop;
    else alignment |= Qt::AlignVCenter;

    return alignment;
}

/******************************************************************************
* The glyph texture atlas shared by all labels of one rendered label set.
*
* Every distinct label string is rasterized exactly once into a tile of the atlas, no matter
* how many data elements carry it. The atlas is cached in the vis cache keyed on the string
* property and the text style, so it survives camera motion and is only rebuilt when the label
* data or the label appearance changes.
******************************************************************************/
namespace {
struct TextAtlas
{
    struct Tile {
        int page = -1;                       ///< Index of the atlas page holding the tile (-1 if the string rasterizes to nothing).
        Vector4F uvRect = Vector4F::Zero();  ///< The tile's region in normalized atlas coordinates (u0,v0,u1,v1).
        Vector2F sizePx = Vector2F::Zero();  ///< The tile size in device pixels.
    };

    std::vector<QImage> pages;               ///< The atlas page images.
    QHash<QString, int> tileByString;        ///< Maps each unique label string to its index in 'tiles'.
    std::vector<Tile> tiles;
};
}   // End of anonymous namespace

/******************************************************************************
* Rasterizes the unique label strings into a shelf-packed texture atlas.
*
* The function runs inside a vis-cache lookup, i.e. while the cache's mutex is held. The
* expensive parts - measuring and painting the glyphs - are therefore spread over the thread
* pool, with each worker painting into its own independent tile image; only the trivial
* packing and the final row-wise copy into the page images run serially.
******************************************************************************/
static void buildTextAtlas(TextAtlas& atlas, const BufferReadAccess<QString>& labelStrings, const QFont& font,
                           const ColorA& textColor, const ColorA& outlineColor, const ColorA& backgroundColor,
                           FloatType backgroundMargin, int textAlignment, qreal devicePixelRatio, QImage::Format imageFormat)
{
    // Collect the unique non-empty label strings in order of first occurrence.
    std::vector<const QString*> uniqueStrings;
    for(const QString& text : labelStrings) {
        if(text.isEmpty())
            continue;
        if(!atlas.tileByString.contains(text)) {
            atlas.tileByString.insert(text, (int)uniqueStrings.size());
            uniqueStrings.push_back(&text);
        }
    }
    atlas.tiles.resize(uniqueStrings.size());
    if(uniqueStrings.empty())
        return;

    // Rasterize each unique string into its own tile image. The tiles are independent of each
    // other, so the loop runs concurrently. Painting a QImage with QPainter is thread-safe as
    // long as no two workers touch the same image, and each worker configures its own private
    // TextPrimitive/QFont copies. The painter setup below mirrors the one used by
    // FrameGraph::renderTextAsImagePrimitives() for the 2d overlay text.
    std::vector<QImage> tileImages(uniqueStrings.size());
    TaskProgress progress(this_task::ui());
    parallelFor(uniqueStrings.size(), 16, progress, [&](size_t i) {
        TextPrimitive text;
        text.setText(*uniqueStrings[i]);
        text.setFont(font);
        text.setColor(textColor);
        text.setOutlineColor(outlineColor);         // An alpha of 0 disables the outline.
        text.setBackgroundColor(backgroundColor);   // An alpha of 0 disables the background.
        text.setBackgroundMargin(backgroundMargin);
        text.setAlignment(textAlignment);           // Selects the paragraph alignment of multi-line rich text.
        text.setTextFormat(Qt::AutoText);
        const Qt::TextFormat resolvedTextFormat = text.resolvedTextFormat();

        // Measure the text as if it was drawn at (0,0). Note that the uncached overload is used
        // deliberately: the cached one would contend on the vis-cache mutex, which the caller of
        // this function already holds.
        const QRectF textBounds = text.queryLocalBounds(devicePixelRatio, resolvedTextFormat);
        const QRectF boundingBox = text.computeBounds(textBounds.size(), devicePixelRatio);
        const QRect pixelBounds = boundingBox.toAlignedRect();
        if(pixelBounds.isEmpty())
            return;     // A whitespace-only string produces no pixels; its tile stays at page -1.

        QImage image(pixelBounds.width(), pixelBounds.height(), imageFormat);
        image.setDevicePixelRatio(devicePixelRatio);
        // Painting the background rectangle means simply filling the tile with the background
        // color, so the background costs no extra geometry - it rides along in the atlas.
        image.fill(backgroundColor.a() > 0.0 ? static_cast<QColor>(backgroundColor) : QColor{0,0,0,0});
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.translate(-boundingBox.left() / devicePixelRatio, -boundingBox.top() / devicePixelRatio);
        QPointF textOffset(-textBounds.left(), -textBounds.top());
        if(textAlignment & Qt::AlignRight) textOffset.rx() += -textBounds.width();
        else if(textAlignment & Qt::AlignHCenter) textOffset.rx() += -textBounds.width() / 2;
        if(textAlignment & Qt::AlignBottom) textOffset.ry() += -textBounds.height();
        else if(textAlignment & Qt::AlignVCenter) textOffset.ry() += -textBounds.height() / 2;
        painter.translate(textOffset.x() / devicePixelRatio, textOffset.y() / devicePixelRatio);
        text.draw(painter, resolvedTextFormat, textBounds.width() / devicePixelRatio);
        painter.end();
        tileImages[i] = std::move(image);
    });

    // Pack the tiles into atlas pages using simple shelf packing: tiles are placed on rows
    // ("shelves") of decreasing height, so sorting them by height first keeps the shelves dense.
    // A small padding between the tiles prevents the linear texture sampler from bleeding in
    // pixels of a neighboring tile.
    constexpr int TilePadding = 2;
    constexpr int MaxPageSize = 4096;   // Conservative texture size limit supported by all QRhi backends.

    std::vector<size_t> order(tileImages.size());
    std::iota(order.begin(), order.end(), (size_t)0);
    std::ranges::sort(order, std::ranges::greater{}, [&](size_t i) { return tileImages[i].height(); });

    // Choose the atlas width from the total tile area, so that a full atlas comes out roughly square.
    qint64 totalArea = 0;
    int maxTileWidth = 0;
    for(const QImage& img : tileImages) {
        totalArea += qint64(img.width() + TilePadding) * (img.height() + TilePadding);
        maxTileWidth = std::max(maxTileWidth, img.width() + 2 * TilePadding);
    }
    const int atlasWidth = std::min(std::max((int)std::ceil(std::sqrt((double)totalArea)), maxTileWidth), MaxPageSize);

    // Assign each tile a position on a page. An oversized tile that does not fit the regular
    // page width gets a dedicated page of its own size; the GPU may reject such a texture if it
    // exceeds the device limits, but that only omits the affected labels with a warning.
    struct Placement { int page; int x, y; };
    std::vector<Placement> placements(tileImages.size());
    struct Page { int width, height; };
    std::vector<Page> pages;
    int x = TilePadding, y = TilePadding, shelfHeight = 0, currentPage = -1;
    for(size_t i : order) {
        const QImage& img = tileImages[i];
        if(img.isNull())
            continue;
        const int w = img.width(), h = img.height();
        if(w + 2 * TilePadding > atlasWidth) {
            pages.push_back({w + 2 * TilePadding, h + 2 * TilePadding});
            placements[i] = {(int)pages.size() - 1, TilePadding, TilePadding};
            continue;
        }
        if(currentPage < 0 || y + h + TilePadding > MaxPageSize) {
            pages.push_back({atlasWidth, 0});
            currentPage = (int)pages.size() - 1;
            x = TilePadding; y = TilePadding; shelfHeight = 0;
        }
        else if(x + w + TilePadding > atlasWidth) {
            y += shelfHeight + TilePadding;
            x = TilePadding;
            shelfHeight = 0;
            if(y + h + TilePadding > MaxPageSize) {
                pages.push_back({atlasWidth, 0});
                currentPage = (int)pages.size() - 1;
                y = TilePadding;
            }
        }
        placements[i] = {currentPage, x, y};
        pages[currentPage].height = std::max(pages[currentPage].height, y + h + TilePadding);
        x += w + TilePadding;
        shelfHeight = std::max(shelfHeight, h);
    }

    // Allocate the page images and copy the tiles into them row by row. All images share the
    // same RGBA format, so a plain byte copy per scanline suffices.
    atlas.pages.reserve(pages.size());
    for(const Page& page : pages) {
        QImage& pageImage = atlas.pages.emplace_back(page.width, page.height, imageFormat);
        pageImage.fill(Qt::transparent);
    }
    for(size_t i = 0; i < tileImages.size(); i++) {
        const QImage& img = tileImages[i];
        if(img.isNull())
            continue;
        const Placement& p = placements[i];
        QImage& pageImage = atlas.pages[p.page];
        for(int row = 0; row < img.height(); row++)
            std::memcpy(pageImage.scanLine(p.y + row) + (size_t)p.x * 4, img.constScanLine(row), (size_t)img.width() * 4);
        atlas.tiles[i].page = p.page;
        atlas.tiles[i].uvRect = Vector4F(
            (float)p.x / pageImage.width(), (float)p.y / pageImage.height(),
            (float)(p.x + img.width()) / pageImage.width(), (float)(p.y + img.height()) / pageImage.height());
        atlas.tiles[i].sizePx = Vector2F(img.width(), img.height());
    }
}

/******************************************************************************
* Returns a short piece of information to be displayed next to the element's
* title in the pipeline editor.
*
* This vis element has no parameter of its own that would be worth displaying here.
* What the user actually wants to see is which property is being turned into labels, and that
* information lives in the modifier which generated the string property the element is attached to.
* The modifier is located by following the back-reference of the data object to the pipeline node
* that created it. Note that the modifier is asked for its own short info instead of being cast to a
* concrete class, because the modifier classes are defined in a module that builds on top of this one.
******************************************************************************/
QVariant TextLabelsVis::getPipelineEditorShortInfo(Scene* scene) const
{
    OVITO_ASSERT(this_task::isMainThread());
    if(!scene || !scene->animationSettings())
        return {};

    // A visual element may be shared by several pipelines and be attached to more than one property.
    // Gather the information from all of them, discarding duplicates.
    QStringList infoStrings;
    for(Pipeline* pipeline : pipelines(true)) {
        // Note: Only the cached pipeline output may be accessed here, because this method must never block.
        const PipelineFlowState state = pipeline->getCachedPipelineOutput(scene->animationSettings()->currentTime());
        for(const ConstDataObjectPath& objectPath : pipeline->getDataObjectsForVisElement(state, const_cast<TextLabelsVis*>(this))) {
            // Follow the back-reference of the label property to the modifier that generated it.
            OORef<const RefTarget> creatorNode = objectPath.back()->createdByNode().lock();
            const ModificationNode* modNode = dynamic_object_cast<ModificationNode>(creatorNode.get());
            if(!modNode || !modNode->modifier())
                continue;
            const QString info = modNode->modifier()->getPipelineEditorShortInfo(scene, const_cast<ModificationNode*>(modNode)).toString();
            if(!info.isEmpty() && !infoStrings.contains(info))
                infoStrings.push_back(info);
        }
    }
    if(infoStrings.isEmpty())
        return {};  // Let the pipeline editor fall back to displaying the titles of the attached data objects.

    return infoStrings.join(QStringLiteral(", "));
}

/******************************************************************************
* Computes the bounding box of the object.
******************************************************************************/
Box3 TextLabelsVis::boundingBoxImmediate(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, TimeInterval& validityInterval)
{
    const PropertyContainer* container = path.nextToLastAs<PropertyContainer>();
    if(!container)
        return {};

    // The labels themselves live in screen space and cannot contribute a world-space extent.
    // Report the bounding box of the anchor points instead, so that "zoom to extents" still works
    // when the labels are the only enabled visual element.
    //
    // Note: The resource frame is held in a named local, not passed as a temporary, so that it stays
    // alive for the entire call and the nested cache lookups performed by getLabelVisData() can share
    // their results. Only the anchor points are requested here: the rendered size of the data elements
    // does not enter the bounding box, and determining it means a pass over all elements.
    const RendererResourceCache::ResourceFrame resourceFrame = this_task::ui()->datasetContainer().visCache()->acquireResourceFrame();
    LabelData labelData = container->getLabelVisData(path, flowState, resourceFrame, LabelDataRequest::AnchorsOnly, elementAnchor());
    if(!labelData.positions)
        return {};

    OVITO_ASSERT(labelData.positions->dataType() == DataBuffer::FloatDefault);
    OVITO_ASSERT(labelData.positions->componentCount() == 3);

    // Elements with an empty label string are not rendered and must not contribute to the bounding box,
    // because they have either been deselected by the modifier or cut away by a slicing plane.
    BufferReadAccess<QString> texts(labelData.texts && labelData.texts->dataType() == DataBuffer::String ? labelData.texts : nullptr);

    Box3 bbox;
    size_t index = 0;
    for(const Point3& p : BufferReadAccess<Point3>(labelData.positions)) {
        if(!texts || (index < texts.size() && !texts[index].isEmpty()))
            bbox.addPoint(p);
        index++;
    }
    return bbox;
}

/******************************************************************************
* Lets the visualization element render the data object.
******************************************************************************/
Future<PipelineStatus> TextLabelsVis::renderAsynchronous(const ConstDataObjectPath& path, const PipelineFlowState& flowState, OORef<FrameGraph> frameGraph, OORef<const SceneNode> sceneNode)
{
    // Get the container the labeled property belongs to. The data object path ends in the string
    // property this vis element is attached to, so the container is its predecessor - the same
    // element that boundingBoxImmediate() picks.
    DataOORef<const PropertyContainer> container = path.nextToLastAs<const PropertyContainer>();
    if(!container)
        co_return PipelineStatus{};

    // Note: This is the last access to 'path' and 'flowState', which are only valid until the coroutine
    // suspends for the first time. The buffers referenced by the LabelData struct are owning pointers
    // and thus remain valid in the worker thread.
    container->verifyIntegrity();
    const LabelData labelData = container->getLabelVisData(path, flowState, frameGraph->visCache(), LabelDataRequest::Complete, elementAnchor());
    if(!labelData.positions || !labelData.texts)
        co_return PipelineStatus{};

    OVITO_ASSERT(labelData.positions->size() == container->elementCount());
    OVITO_ASSERT(labelData.positions->componentCount() == 3 && labelData.positions->dataType() == DataBuffer::FloatDefault);

    // Note: All problems are reported as warnings, never as errors, because the frame graph builder turns
    // an error status of a vis element into an exception, which would abort an entire batch render run.
    if(labelData.texts->componentCount() != 1)
        co_return PipelineStatus{PipelineStatus::Warning, tr("The property used for the text labels must be a scalar property.")};
    if(labelData.texts->dataType() != DataBuffer::String)
        co_return PipelineStatus{PipelineStatus::Warning,
                tr("The property used for the text labels must be a string property. Use the 'Add text labels' modifier to turn the values "
                   "of another property into label texts.")};
    if(labelData.texts->size() != labelData.positions->size())
        co_return PipelineStatus{PipelineStatus::Warning, tr("The property used for the text labels has an inconsistent length.")};

    if(labelData.texts->size() == 0)
        co_return PipelineStatus{};

    // Rendering one text primitive per data element gets prohibitively expensive for large datasets.
    // The input is checked against the limit configured by the user further below, once the number of
    // non-empty label strings is known. Note that the limit must be applied to that number rather than
    // to the element count: labeling a handful of selected particles out of a million must still work.
    const size_t labelCountLimit = (size_t)std::max(0, maxLabelCount());

    // Determine the size of the viewport in physical device pixels.
    const QSize physicalSize((int)std::lround(frameGraph->viewportDeviceIndependentSize().width() * frameGraph->devicePixelRatio()),
                             (int)std::lround(frameGraph->viewportDeviceIndependentSize().height() * frameGraph->devicePixelRatio()));
    if(physicalSize.isEmpty())
        co_return PipelineStatus{};

    // Resolve the font used by the labels.
    if(fontSize() > FloatType(1)) co_return PipelineStatus{PipelineStatus::Warning, tr("Text label font size is too large.")};
    const FloatType effectiveFontSize = fontSize() * physicalSize.height();
    if(effectiveFontSize < FloatType(1))
        co_return PipelineStatus{};  // Nothing to render. Also guards against frame graphs with a dummy viewport size.
    QFont font = this->font();
    TextPrimitive::setFontPixelSize(font, effectiveFontSize / frameGraph->devicePixelRatio()); // Font size is always in logical units.

    // The label texts are taken verbatim from the property the vis element is attached to.
    BufferReadAccess<QString> labelStrings(labelData.texts);

    // Set up the projection of anchor points into window space. Note that the projection parameters are
    // copied into a local variable, because the frame graph builder may still update them while this
    // coroutine is suspended.
    const ViewProjectionParameters projParams = frameGraph->projectionParams();
    // Querying the world transformation matrix must happen in the main thread, because the scene node is not thread-safe.
    const AffineTransformation worldTM = sceneNode->getWorldTransform(frameGraph->time());
    const AffineTransformation modelViewTM = projParams.viewMatrix * worldTM;
    const FloatType halfWidth = FloatType(0.5) * physicalSize.width();
    const FloatType halfHeight = FloatType(0.5) * physicalSize.height();

    // The labels are rendered as depth-tested billboards by default, which live in the scene layer so that
    // they are properly occluded by other 3d objects.
    // Note that the command group must be created in the main thread and up front, because its position
    // in the frame graph determines the order in which the rendering commands are executed.
    const bool prepend = !alwaysInFront();
    FrameGraph::RenderingCommandGroup& commandGroup = frameGraph->addCommandGroup(FrameGraph::SceneLayer, prepend);

    // Copy the remaining parameters into local variables before entering the worker thread to avoid
    // data races with the GUI thread, which may modify them while we are rendering.
    const int alignment = this->alignment();
    const Color textColor = this->textColor();
    const bool outlineEnabled = this->outlineEnabled();
    const Color outlineColor = this->outlineColor();
    const bool backgroundEnabled = this->backgroundEnabled();
    const Color backgroundColor = this->backgroundColor();
    const FloatType depthOffset = std::max(this->depthOffset(), FloatType(0));
    const bool alwaysInFront = this->alwaysInFront();

    // The screen-space displacement applied to every label.
    const Vector2 offset(offsetX() * physicalSize.width(), -offsetY() * physicalSize.height());

    // The label atlas built below default-constructs QFont objects on thread-pool workers, which
    // requires a fully constructed Qt application object. In headless mode the application is
    // created lazily, so it must be brought up here - synchronously, while still on the main
    // thread - before any worker can race with its construction. This call must stay in the
    // frame-graph prologue of every vis element that touches QFont on a worker: it covers frame
    // graphs built without a RenderThread (e.g. glTF scene export), and it deliberately runs only
    // when text is actually rendered - creating the application unconditionally per render would
    // pin the headless QPA platform and prevent a Python session from opening the OVITO desktop
    // window after rendering a text-free scene.
    TextPrimitive::ensureFontRenderingCapability();

    // Perform the actual work in a background thread to not block the GUI for too long.
    co_await ExecutorAwaiter(ThreadPoolExecutor());

    // Converts a length given in world units into a length in device pixels, measured at the given
    // position in view space. Note that ViewProjectionParameters::projectedPixelSize() cannot be used
    // here, because its perspective branch measures along the x-axis but divides by the window height.
    auto worldLengthToPixels = [&](const Point3& viewPos, FloatType length) -> FloatType {
        if(!projParams.isPerspective)
            return length / projParams.fieldOfView * halfHeight;
        const Point3 p1 = projParams.projectionMatrix * viewPos;
        const Point3 p2 = projParams.projectionMatrix * (viewPos + Vector3(0, length, 0));
        return (p1 - p2).length() * halfHeight;
    };

    // The direction in which a label is pushed away from the element it annotates, given in window
    // coordinates, whose y-axis points downwards. Note that this must be a unit vector, so that a
    // diagonally placed label ends up on the silhouette of the glyph and not beyond its corner.
    Vector2 shiftDirection = Vector2::Zero();
    if(alignment & Qt::AlignRight) shiftDirection.x() = 1;
    else if(alignment & Qt::AlignLeft) shiftDirection.x() = -1;
    if(alignment & Qt::AlignTop) shiftDirection.y() = -1;
    else if(alignment & Qt::AlignBottom) shiftDirection.y() = 1;
    if(shiftDirection != Vector2::Zero())
        shiftDirection.normalize();

    // The fraction of the label quad's size between the quad's top-left corner and the anchor
    // point. This translates the label placement into the quad corner that gets pinned to the
    // anchor, mirroring labelPlacementToTextAlignment(): a label to the right of its element has
    // its left edge (factor 0) at the anchor, a label above its element its bottom edge (factor 1,
    // window y-axis pointing down).
    Vector2 alignmentFactor(0.5, 0.5);
    if(alignment & Qt::AlignRight) alignmentFactor.x() = 0;
    else if(alignment & Qt::AlignLeft) alignmentFactor.x() = 1;
    if(alignment & Qt::AlignTop) alignmentFactor.y() = 1;
    else if(alignment & Qt::AlignBottom) alignmentFactor.y() = 0;

    // Per-element size information used to shift the labels away from the elements they annotate.
    // Note that the radii are accessed untyped, because the containers may supply them as either
    // single or double precision floats. A radius array that does not match the number of anchor
    // points is ignored: a data table, for example, designates its radius column by name, and
    // nothing guarantees that such a column is a per-row property.
    RawBufferReadAccess radii(labelData.radii && labelData.radii->size() == labelData.positions->size() ? labelData.radii : nullptr);
    const bool shiftLabels = (radii || labelData.uniformRadius > 0) && shiftDirection != Vector2::Zero();

    // Labels whose anchor point lies this far outside the viewport can never become visible.
    //
    // A label extends away from its anchor point by at most its own size, so the margin has to cover
    // that - otherwise a long label reaching into the viewport from outside would be culled. Its width
    // is bounded from above by the length of the longest string times the font size, which is a safe
    // over-estimate for any font whose glyphs are not wider than they are tall. Measuring the strings
    // themselves would be far more expensive than the culling saves. Note that the vertical extent is
    // just a couple of lines, so the two directions get separate margins.
    const qsizetype longestLabel = std::ranges::max(std::views::transform(labelStrings, [](const QString& s) { return s.size(); }));
    const FloatType cullMarginX = effectiveFontSize * FloatType(longestLabel + 2);
    const FloatType cullMarginY = 3 * effectiveFontSize;

    // Project the anchor points into window space, discarding all labels that cannot become visible.
    // The view-space depth is kept around to sort the labels back-to-front further below.
    struct VisibleLabel {
        FloatType viewZ;
        Point2 pos;
        size_t index;
    };
    // Count the labels that will actually be rendered. Elements with an empty string - unselected
    // elements, or elements cut away by a slicing plane - do not count towards the limit.
    const size_t validLabelCount = std::ranges::count_if(labelStrings, [](const QString& text) { return !text.isEmpty(); });
    if(validLabelCount > labelCountLimit) {
        co_return PipelineStatus{PipelineStatus::Warning,
                tr("Text labels are not rendered, because the number of labels (%1) exceeds the configured maximum of %2. "
                   "Raise the label limit of the visual element or reduce the number of labeled elements.")
                    .arg(validLabelCount)
                    .arg(labelCountLimit)};
    }

    BufferReadAccess<Point3> positions(labelData.positions);

    // The projection of one anchor point does not depend on any other, so the loop is spread over the
    // thread pool. Each worker collects into its own vector, which keeps the workers from contending
    // on a shared container; the per-worker results are concatenated afterwards. The chunk size keeps
    // small inputs - the common case - on a single thread, where the split would only cost overhead.
    TaskProgress progress(this_task::ui());
    std::vector<std::vector<VisibleLabel>> perWorkerLabels = parallelForCollect<std::vector<VisibleLabel>>(
        labelStrings.size(), 1024, progress,
        [&](size_t i, std::vector<VisibleLabel>& result) {
            if(labelStrings[i].isEmpty())
                return;

            // Cull points that are not in front of the camera. Note that the projection parameters are still
            // preliminary at this point during the construction of the frame graph, i.e. the near/far plane
            // distances may still change. Only the x/y window coordinates computed here are reliable.
            const Point3 viewPos = modelViewTM * positions[i];
            if(projParams.isPerspective && viewPos.z() >= 0)
                return;

            const Vector4 clipPos = projParams.projectionMatrix * Vector4(viewPos.x(), viewPos.y(), viewPos.z(), 1);
            if(clipPos.w() == 0)
                return;

            Point2 pos(( clipPos.x() / clipPos.w() + 1) * halfWidth,
                       (-clipPos.y() / clipPos.w() + 1) * halfHeight);

            // Shift the label away from the element, such that it comes to rest at the visual edge of the
            // rendered glyph instead of on top of it. Note that a centered label is never shifted.
            if(shiftLabels) {
                const FloatType radius =
                    worldLengthToPixels(viewPos, radii ? radii.get<FloatType>(i, 0) : labelData.uniformRadius);
                pos += shiftDirection * radius;
            }

            pos += offset;
            if(pos.x() < -cullMarginX || pos.y() < -cullMarginY ||
               pos.x() > physicalSize.width() + cullMarginX || pos.y() > physicalSize.height() + cullMarginY)
                return;

            result.push_back({viewPos.z(), pos, i});
        });

    std::vector<VisibleLabel> visibleLabels;
    visibleLabels.reserve(validLabelCount);
    for(std::vector<VisibleLabel>& workerLabels : perWorkerLabels)
        visibleLabels.insert(visibleLabels.end(), std::make_move_iterator(workerLabels.begin()), std::make_move_iterator(workerLabels.end()));
    if(visibleLabels.empty()) co_return PipelineStatus{};

    // Sort the labels back to front, so that labels of nearby elements are drawn on top of labels of more
    // distant elements. Note that view-space z-coordinates are more negative for farther points. The sort
    // also restores a deterministic order, which the parallel loop above does not by itself guarantee:
    // the element index breaks ties so that two labels at the same depth never swap between runs.
    std::ranges::sort(visibleLabels, {}, [](const VisibleLabel& l) { return std::make_pair(l.viewZ, l.index); });

    // Rasterize the unique label strings into a shared texture atlas, so that data elements
    // carrying the same text share one glyph image and one region of GPU memory. The atlas is
    // cached across frames keyed on the string property and the text style: moving the camera
    // reuses it as-is, only a change of the label data or appearance triggers a rebuild.
    //
    // Note: The optional background rectangle is baked into the atlas tiles, so the background
    // costs neither extra geometry nor extra texture memory per label.
    const int textAlignment = labelPlacementToTextAlignment(alignment);
    const ColorA effectiveOutlineColor = outlineEnabled ? ColorA(outlineColor) : ColorA(0, 0, 0, 0);
    const ColorA effectiveBackgroundColor = backgroundEnabled ? ColorA(backgroundColor) : ColorA(0, 0, 0, 0);
    const FloatType backgroundMargin = backgroundEnabled ? 0.15 * effectiveFontSize : FloatType(0);
    const qreal devicePixelRatio = frameGraph->devicePixelRatio();
    const TextAtlas& atlas = frameGraph->visCache().lookup<TextAtlas>(
        RendererResourceKey<struct TextAtlasCache, ConstDataBufferPtr, QString, ColorA, ColorA, ColorA, FloatType, qreal, int>{
            labelData.texts, font.key(), ColorA(textColor), effectiveOutlineColor, effectiveBackgroundColor,
            effectiveFontSize, devicePixelRatio, textAlignment},
        [&](TextAtlas& atlas) {
            buildTextAtlas(atlas, labelStrings, font, ColorA(textColor), effectiveOutlineColor, effectiveBackgroundColor,
                           backgroundMargin, textAlignment, devicePixelRatio, frameGraph->preferredImageFormat());
        });

    // Partition the visible labels by the atlas page their tile ended up on. Iterating the
    // sorted list keeps the back-to-front order within each page, which is what the renderer
    // relies on for correct alpha blending.
    struct PageEntry { size_t visibleIndex; int tileIndex; };
    std::vector<std::vector<PageEntry>> perPage(atlas.pages.size());
    for(size_t v = 0; v < visibleLabels.size(); v++) {
        const int tileIndex = atlas.tileByString.value(labelStrings[visibleLabels[v].index], -1);
        if(tileIndex < 0 || atlas.tiles[tileIndex].page < 0)
            continue;
        perPage[atlas.tiles[tileIndex].page].push_back({v, tileIndex});
    }

    // Determine the bounding box of the visible anchor points and the largest occurring radius.
    // The box is padded by that radius plus the depth offset, because the labels are shifted toward
    // the camera by up to this length and the near clipping plane must not cut them off.
    Box3 anchorBox;
    FloatType maxRadius = radii ? 0 : labelData.uniformRadius;
    for(const VisibleLabel& label : visibleLabels) {
        anchorBox.addPoint(positions[label.index]);
        if(radii)
            maxRadius = std::max(maxRadius, radii.get<FloatType>(label.index, 0));
    }

    // Create one billboard primitive per atlas page (a single page in the common case), each
    // rendered as one instanced draw call.
    for(size_t pageIndex = 0; pageIndex < perPage.size(); pageIndex++) {
        const std::vector<PageEntry>& entries = perPage[pageIndex];
        if(entries.empty())
            continue;
        this_task::throwIfCanceled();

        // Build the per-instance data buffers in back-to-front order.
        BufferFactory<Point3> positionsBuffer(entries.size());
        BufferFactory<Vector4F> uvRectsBuffer(entries.size());
        BufferFactory<Vector2F> sizesBuffer(entries.size());
        BufferFactory<float> radiiBuffer;
        if(radii)
            radiiBuffer = BufferFactory<float>(entries.size());
        for(size_t j = 0; j < entries.size(); j++) {
            const VisibleLabel& label = visibleLabels[entries[j].visibleIndex];
            const TextAtlas::Tile& tile = atlas.tiles[entries[j].tileIndex];
            positionsBuffer[j] = positions[label.index];
            uvRectsBuffer[j] = tile.uvRect;
            sizesBuffer[j] = tile.sizePx;
            if(radii)
                radiiBuffer[j] = (float)radii.get<FloatType>(label.index, 0);
        }

        std::unique_ptr<TextBillboardPrimitive> primitive = std::make_unique<TextBillboardPrimitive>();
        primitive->setAtlasImage(atlas.pages[pageIndex]);
        primitive->setPositions(positionsBuffer.take());
        primitive->setUVRects(uvRectsBuffer.take());
        primitive->setSizes(sizesBuffer.take());
        if(radii)
            primitive->setRadii(radiiBuffer.take());
        else
            primitive->setUniformRadius(labelData.uniformRadius);
        primitive->setMaxRadius(maxRadius);
        primitive->setAlignmentFactor(alignmentFactor);
        primitive->setShiftDirection(shiftDirection);
        primitive->setPixelOffset(offset);
        primitive->setDepthOffset(depthOffset);
        primitive->setAlwaysInFront(alwaysInFront);

        // Note: The group-level overload is used here, with an explicitly precomputed bounding
        // box, because the FrameGraph-level overloads may only be called from the main thread.
        commandGroup.addPrimitiveNonpickable(std::move(primitive), worldTM, anchorBox.padBox(maxRadius + depthOffset));
    }

    co_return PipelineStatus{};
}

}   // End of namespace
