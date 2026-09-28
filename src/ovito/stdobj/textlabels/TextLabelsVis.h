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

#include <ovito/stdobj/properties/Property.h>
#include <ovito/core/dataset/data/DataVis.h>
#include <ovito/core/rendering/TextBillboardPrimitive.h>
#include <ovito/core/rendering/SceneRenderer.h>

namespace Ovito {

/**
 * \brief Visualizes the values of a string property as text labels placed at the positions of the data elements.
 *
 * The labels are rendered as camera-facing billboards placed in the 3d scene: they keep a constant
 * apparent size on screen but are depth-tested against the scene geometry, so a label behind an opaque
 * object is occluded by it. Each label is shifted toward the camera by the rendered radius of the data
 * element it annotates plus the user-defined depth offset, so that it surfaces in front of the element's
 * glyph instead of being buried inside. All labels are rasterized into a shared, deduplicated texture
 * atlas and rendered with one instanced draw call per atlas page.
 *
 * The element must be attached to a property of data type DataBuffer::String. Turning the values of some
 * other property into label texts is the job of the TextLabelsModifier.
 */
class OVITO_STDOBJ_EXPORT TextLabelsVis : public DataVis
{
    OVITO_CLASS(TextLabelsVis)

public:
    /// Selects how much of the label visualization data a caller of PropertyContainer::getLabelVisData() needs.
    ///
    /// Determining the rendered size of the data elements can be as expensive as computing the effective
    /// radius of every particle. The bounding box computation does not use that information at all, which
    /// is why it asks for the anchor points only.
    enum class LabelDataRequest {
        AnchorsOnly,    ///< Only the anchor positions and the label texts are needed.
        Complete        ///< The per-element radii are needed in addition to the anchor points.
    };

    /// Selects the point along an elongated data element, such as a bond or a vector, at which its
    /// label is anchored. Containers whose data elements have no extent ignore this setting.
    ///
    /// The enumerator names match those of VectorVis::ArrowPosition, which selects the corresponding
    /// point along the rendered arrows.
    enum ElementAnchor {
        Base,       ///< The label is anchored at the start point of the element.
        Center,     ///< The label is anchored at the mid point of the element.
        Head        ///< The label is anchored at the end point of the element.
    };
    Q_ENUM(ElementAnchor);

    /// Struct holding the per-element data provided by the PropertyContainer that owns the string
    /// property the TextLabelsVis is attached to.
    struct LabelData {
        ConstDataBufferPtr positions;  ///< The anchor points of the labels.
        ConstDataBufferPtr texts;      ///< The property providing the values to be displayed as text.

        /// Optional per-element radius of the rendered glyphs (in world units), which is used to shift a
        /// label away from the element it annotates. Takes precedence over 'uniformRadius'.
        ConstDataBufferPtr radii;

        /// The radius of the rendered glyphs (in world units), used when 'radii' is not provided.
        FloatType uniformRadius = 0;
    };

    /// Blanks out the label strings of those elements whose anchor point is cut away by a cutting plane,
    /// so that the labels match the clipped geometry the corresponding vis element renders.
    ///
    /// The cutting planes attached to a data object are not applied by the pipeline but only at render time,
    /// which is why the label texts must be filtered here rather than in the modifier generating them.
    /// The predicate is invoked as isCulled(elementIndex, anchorPoint). Note that an empty label string is
    /// skipped by renderAsynchronous(), so no further filtering of the anchor positions is needed.
    template<typename CullPredicate>
    [[nodiscard]] static ConstDataBufferPtr cullLabels(ConstDataBufferPtr texts, const ConstDataBufferPtr& positions, bool cullingRequired, CullPredicate&& isCulled)
    {
        // Note: The data type is checked here, because this method runs before renderAsynchronous() gets to
        // report a non-string property as a warning. Such a property is passed through untouched.
        if(!cullingRequired || !texts || !positions || texts->dataType() != DataBuffer::String)
            return texts;

        BufferWriteAccessAndRef<QString, access_mode::write> filteredTexts = texts.makeCopy();
        size_t index = 0;
        for(const Point3& p : BufferReadAccess<Point3>{positions}) {
            if(isCulled(index, p))
                filteredTexts[index] = QString();
            index++;
        }
        return filteredTexts.take();
    }

public:
    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Lets the visualization element render the data object.
    virtual Future<PipelineStatus> renderAsynchronous(const ConstDataObjectPath& path,
                                                      const PipelineFlowState& flowState,
                                                      OORef<FrameGraph> frameGraph,
                                                      OORef<const SceneNode> sceneNode) override;

    /// Computes the bounding box of the object.
    virtual Box3 boundingBoxImmediate(AnimationTime time,
                                      const ConstDataObjectPath& path,
                                      const Pipeline* pipeline,
                                      const PipelineFlowState& flowState,
                                      TimeInterval& validityInterval) override;

    /// Returns a short piece of information to be displayed next to the element's title in the pipeline editor.
    virtual QVariant getPipelineEditorShortInfo(Scene* scene) const override;

public:
    Q_PROPERTY(int alignment READ alignment WRITE setAlignment)

protected:
    /// The font used for rendering the text labels.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QFont{}, font, setFont, PROPERTY_FIELD_MEMORIZE);

    /// The font size, specified as a fraction of the viewport height.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.02}, fontSize, setFontSize, PROPERTY_FIELD_MEMORIZE);

    /// The uniform color of the text labels.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS((Color{0, 0, 0}), textColor, setTextColor, PROPERTY_FIELD_MEMORIZE);

    /// Controls the outlining of the text.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, outlineEnabled, setOutlineEnabled, PROPERTY_FIELD_MEMORIZE);

    /// The color of the text outline.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS((Color{1, 1, 1}), outlineColor, setOutlineColor, PROPERTY_FIELD_MEMORIZE);

    /// Controls the placement of a label relative to the anchor point of its data element.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{Qt::AlignHCenter | Qt::AlignVCenter}, alignment, setAlignment, PROPERTY_FIELD_MEMORIZE);

    /// Selects at which point along an elongated data element its label is anchored.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(ElementAnchor{Center}, elementAnchor, setElementAnchor, PROPERTY_FIELD_MEMORIZE);

    /// Controls the horizontal offset of the labels, specified as a fraction of the viewport width.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, offsetX, setOffsetX, PROPERTY_FIELD_MEMORIZE);

    /// Controls the vertical offset of the labels, specified as a fraction of the viewport height.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, offsetY, setOffsetY, PROPERTY_FIELD_MEMORIZE);

    /// Controls the extra distance by which a label is lifted toward the camera beyond the rendered
    /// radius of its data element, specified as an absolute length in world units. Prevents the label
    /// from being clipped by the glyph it annotates. Unlike the automatic lift by the element's radius,
    /// this margin also applies to elements that report no radius at all, e.g. voxel grid cells and
    /// surface mesh vertices. Has no effect while alwaysInFront is set.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.05}, depthOffset, setDepthOffset, PROPERTY_FIELD_MEMORIZE);

    /// Snaps the labels in front of all other objects in the scene: depth testing is disabled
    /// entirely, so no scene geometry ever occludes a label. Mutually exclusive with depthOffset.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, alwaysInFront, setAlwaysInFront, PROPERTY_FIELD_MEMORIZE);

    /// Controls the rendering of a background rectangle behind each label.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, backgroundEnabled, setBackgroundEnabled, PROPERTY_FIELD_MEMORIZE);

    /// The fill color of the background rectangles.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS((Color{1, 1, 1}), backgroundColor, setBackgroundColor, PROPERTY_FIELD_MEMORIZE);

    /// The maximum number of labels to render. Rendering is skipped if the input contains more elements.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{20000}, maxLabelCount, setMaxLabelCount);
};

}  // namespace Ovito
