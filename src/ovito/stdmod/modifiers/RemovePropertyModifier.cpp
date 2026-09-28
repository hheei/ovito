// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdmod/StdMod.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "RemovePropertyModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(RemovePropertyModifier);
OVITO_CLASSINFO(RemovePropertyModifier, "DisplayName", "Remove property");
OVITO_CLASSINFO(RemovePropertyModifier, "Description", "Removes one or more properties associated with particles, bonds, or other data elements.");
OVITO_CLASSINFO(RemovePropertyModifier, "ModifierCategory", "Modification");
DEFINE_PROPERTY_FIELD(RemovePropertyModifier, propertiesToRemove);
SET_PROPERTY_FIELD_LABEL(RemovePropertyModifier, propertiesToRemove, "Properties to remove");

/******************************************************************************
 * Is called by the system after the modifier has been inserted into a data pipeline.
 ******************************************************************************/
void RemovePropertyModifier::initializeObject(ObjectInitializationFlags flags)
{
    Modifier::initializeObject(flags);
    // Operate on particle properties by default.
    setDefaultSubject(QStringLiteral("Particles"), QStringLiteral("Particles"));
}

/******************************************************************************
 * Is called when the value of a property of this object has changed.
 ******************************************************************************/
void RemovePropertyModifier::propertyChanged(const PropertyFieldDescriptor* field)
{
    // Clear the list of properties to remove when the subject changes.
    if(field == PROPERTY_FIELD(RemovePropertyModifier::subject) && !shouldIgnoreChanges() && !isUndoingOrRedoing() &&
       this_task::isInteractive()) {
        setPropertiesToRemove({});
    }

    // Changes of some the modifier's parameters affect the result of getPipelineEditorShortInfo().
    if(field == PROPERTY_FIELD(propertiesToRemove) || field == PROPERTY_FIELD(subject)) {
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }

    GenericPropertyModifier::propertyChanged(field);
}

/******************************************************************************
 * Returns a short piece of information (typically a string or color) to be displayed next to the modifier's title in the pipeline
 * editor list.
 ******************************************************************************/
QVariant RemovePropertyModifier::getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const
{
    QString shortInfo;
    if(subject() && !propertiesToRemove().empty()) {
        shortInfo = QStringLiteral("%1: %2").arg(subject().dataTitleOrPath()).arg(propertiesToRemove().join(QStringLiteral(", ")));
    }
    return shortInfo;
}

/******************************************************************************
 * Modifies the input data.
 ******************************************************************************/
Future<PipelineFlowState> RemovePropertyModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    // Early exit if there are no properties to remove.
    if(propertiesToRemove().empty()) {
        return std::move(state);
    }

    // Look up the property container which we will operate on.
    PropertyContainer* container = state.expectMutableLeafObject<PropertyContainer>(subject());
    container->verifyIntegrity();

    // Remove properties from the container.
    for(const QString& pname : propertiesToRemove()) {
        if(const Property* prop = container->getProperty(pname)) {
            container->removeProperty(prop);
        }
        else if(!this_task::isInteractive()) {
            throw Exception(tr("Property '%1' to be removed does not exist in the input data container.").arg(pname));
        }
    }

    return std::move(state);
}

}  // namespace Ovito