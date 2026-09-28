// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdobj/StdObj.h>
#include "Vectors.h"
#include "VectorVis.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(Vectors);
OVITO_CLASSINFO(Vectors, "DisplayName", "Vectors");

/******************************************************************************
 * Registers all standard properties with the property traits class.
 ******************************************************************************/
void Vectors::OOMetaClass::initialize()
{
    PropertyContainerClass::initialize();

    setPropertyClassDisplayName(tr("Vectors"));
    setElementDescriptionName(QStringLiteral("vectors"));
    setPythonName(QStringLiteral("vectors"));

    const QStringList emptyList;
    const QStringList xyzList = QStringList() << "X" << "Y" << "Z";
    const QStringList rgbList = QStringList() << "R" << "G" << "B";
    registerStandardProperty(ColorProperty, QStringLiteral("Color"), Property::FloatGraphics, rgbList);
    registerStandardProperty(DirectionProperty, QStringLiteral("Direction"), Property::FloatDefault, xyzList);
    registerStandardProperty(PositionProperty, QStringLiteral("Position"), Property::FloatDefault, xyzList);
    registerStandardProperty(TransparencyProperty, QStringLiteral("Transparency"), Property::FloatGraphics, emptyList);
    registerStandardProperty(SelectionProperty, QStringLiteral("Selection"), Property::IntSelection, emptyList);
}

/******************************************************************************
 * Creates a storage object for standard properties.
 ******************************************************************************/
PropertyPtr Vectors::OOMetaClass::createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type,
                                                                 const ConstDataObjectPath& containerPath) const
{
    int dataType;
    size_t componentCount;

    switch(type) {
        case ColorProperty:
            dataType = Property::FloatGraphics;
            componentCount = 3;
            OVITO_ASSERT(componentCount * sizeof(GraphicsFloatType) == sizeof(ColorG));
            break;
        case DirectionProperty:
            dataType = Property::FloatDefault;
            componentCount = 3;
            OVITO_ASSERT(componentCount * sizeof(FloatType) == sizeof(Vector3));
            break;
        case PositionProperty:
            dataType = Property::FloatDefault;
            componentCount = 3;
            OVITO_ASSERT(componentCount * sizeof(FloatType) == sizeof(Point3));
            break;
        case TransparencyProperty:
            dataType = Property::FloatGraphics;
            componentCount = 1;
            break;
        case SelectionProperty:
            dataType = DataBuffer::IntSelection;
            componentCount = 1;
            break;
        default:
            OVITO_ASSERT_MSG(false, "Vectors::createStandardProperty()", "Invalid standard property type");
            throw Exception(tr("This is not a valid standard property type: %1").arg(type));
    }

    const QStringList& componentNames = standardPropertyComponentNames(type);
    const QString& propertyName = standardPropertyName(type);

    OVITO_ASSERT(componentCount == standardPropertyComponentCount(type));

    PropertyPtr property =
        PropertyPtr::create(DataBuffer::Uninitialized, elementCount, dataType, componentCount, propertyName, type, componentNames);

    // Initialize memory if requested.
    if(init == DataBuffer::Initialized && !containerPath.empty()) {
        // Certain standard properties need to be initialized with default values determined by the attached visual element.
        if(type == ColorProperty) {
            if(const Vectors* vectors = dynamic_object_cast<Vectors>(containerPath.back())) {
                if(VectorVis* vectorsVis = dynamic_object_cast<VectorVis>(vectors->visElement())) {
                    property->fill<ColorG>(vectorsVis->arrowColor().toDataType<GraphicsFloatType>());
                    init = DataBuffer::Uninitialized;
                }
            }
        }
        else if(type == TransparencyProperty) {
            if(const Vectors* vectors = dynamic_object_cast<Vectors>(containerPath.back())) {
                if(VectorVis* vectorsVis = dynamic_object_cast<VectorVis>(vectors->visElement())) {
                    property->fill<GraphicsFloatType>(static_cast<GraphicsFloatType>(vectorsVis->transparency()));
                    init = DataBuffer::Uninitialized;
                }
            }
        }
    }

    if(init == DataBuffer::Initialized) {
        // Default-initialize property values with zeros.
        property->fillZero();
    }

    return property;
}

/******************************************************************************
 * Returns the position of a point along an arrow, expressed as a fraction of the arrow vector.
 ******************************************************************************/
static FloatType anchorFraction(TextLabelsVis::ElementAnchor anchor)
{
    if(anchor == TextLabelsVis::Base) return FloatType(0);
    if(anchor == TextLabelsVis::Head) return FloatType(1);
    return FloatType(0.5);
}

/******************************************************************************
 * Returns the position of the base point of an arrow, expressed as a fraction of the arrow vector.
 ******************************************************************************/
static FloatType arrowFraction(VectorVis::ArrowPosition position)
{
    if(position == VectorVis::Base) return FloatType(0);
    if(position == VectorVis::Head) return FloatType(1);
    return FloatType(0.5);
}

/******************************************************************************
 * Returns the data for visualizing a property from this container as text labels
 * using a TextLabelsVis element.
 ******************************************************************************/
TextLabelsVis::LabelData Vectors::getLabelVisData(const ConstDataObjectPath& path,
                                                  const PipelineFlowState& state,
                                                  const RendererResourceCache::ResourceFrame& visCache,
                                                  TextLabelsVis::LabelDataRequest request,
                                                  TextLabelsVis::ElementAnchor anchor) const
{
    const Property* positionProperty = getProperty(PositionProperty);
    if(!positionProperty) return {};

    // Without a vis element, the arrows are not rendered at all and the base points are the only
    // anchor points that can be reported.
    const VectorVis* vectorVis = visElement<VectorVis>();
    if(!vectorVis)
        return {.positions = positionProperty, .texts = path.lastAs<DataBuffer>()};

    // The labels must sit on the arrows as they are rendered by the vis element, which is why the
    // arrow geometry computed by VectorVis::renderSynchronous() is reproduced here.
    FloatType scalingFac = vectorVis->scalingFactor();
    if(vectorVis->reverseArrowDirection())
        scalingFac = -scalingFac;
    const FloatType t = (anchorFraction(anchor) - arrowFraction(vectorVis->arrowPosition())) * scalingFac;
    const Vector3 offset = vectorVis->offset();

    const Property* directionProperty = getProperty(DirectionProperty);
    if(!directionProperty || directionProperty->componentCount() != 3 || directionProperty->dataType() != DataBuffer::FloatDefault)
        directionProperty = nullptr;

    ConstDataBufferPtr anchorPositions = positionProperty;
    if((t != 0 && directionProperty) || offset != Vector3::Zero()) {
        // Look up the displaced anchor points in the cache.
        anchorPositions = visCache.lookup<ConstDataBufferPtr>(
            RendererResourceKey<struct VectorAnchorsCache, ConstDataObjectRef, ConstDataObjectRef, FloatType, Vector3>{
                positionProperty, directionProperty, t, offset},
            [&](ConstDataBufferPtr& anchors) {
                BufferReadAccess<Point3> basePositions(positionProperty);
                BufferFactory<Point3> points(basePositions.size());
                if(t != 0 && directionProperty) {
                    BufferReadAccess<Vector3> directions(directionProperty);
                    for(size_t i = 0; i < basePositions.size(); i++)
                        points[i] = basePositions[i] + t * directions[i] + offset;
                }
                else {
                    for(size_t i = 0; i < basePositions.size(); i++)
                        points[i] = basePositions[i] + offset;
                }
                anchors = points.take();
            });
    }

    // Report the radius of the arrow cylinders, so that the vis element can shift a label clear of
    // the arrow it belongs to. A disabled vis element draws no arrows and thus has no size to shift
    // a label clear of - but where an arrow would be still defines the anchor points above.
    return {.positions = std::move(anchorPositions),
            .texts = path.lastAs<DataBuffer>(),
            .uniformRadius = vectorVis->isEnabled() ? vectorVis->arrowWidth() : FloatType(0)};
}

/******************************************************************************
 * Constructor.
 ******************************************************************************/
void Vectors::initializeObject(ObjectInitializationFlags flags)
{
    PropertyContainer::initializeObject(flags);

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject) && !flags.testFlag(ObjectInitializationFlag::DontCreateVisElement)) {
        // Create and attach a default visualization element for rendering the vectors.
        setVisElement(OORef<VectorVis>::create(flags));
    }
}

}  // namespace Ovito
