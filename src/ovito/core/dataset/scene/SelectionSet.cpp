// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/dataset/scene/SelectionSet.h>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SelectionSet);
DEFINE_VECTOR_REFERENCE_FIELD(SelectionSet, nodes);
SET_PROPERTY_FIELD_LABEL(SelectionSet, nodes, "Nodes");

/******************************************************************************
* Adds a scene node to this selection set.
******************************************************************************/
void SelectionSet::push_back(OORef<SceneNode> node)
{
    OVITO_CHECK_OBJECT_POINTER(node);
    if(nodes().contains(node))
        throw Exception(tr("Node is already in the selection set."));

    // Insert into children array.
    _nodes.push_back(this, PROPERTY_FIELD(nodes), std::move(node));
}

/******************************************************************************
* Inserts a scene node into this selection set.
******************************************************************************/
void SelectionSet::insert(qsizetype index, OORef<SceneNode> node)
{
    OVITO_CHECK_OBJECT_POINTER(node);
    if(nodes().contains(node))
        throw Exception(tr("Node is already in the selection set."));

    // Insert into children array.
    _nodes.insert(this, PROPERTY_FIELD(nodes), index, std::move(node));
}

/******************************************************************************
* Removes a scene node from this selection set.
******************************************************************************/
void SelectionSet::remove(const SceneNode* node)
{
    int index = _nodes.indexOf(node);
    if(index == -1) return;
    removeByIndex(index);
    OVITO_ASSERT(!nodes().contains(node));
}

/******************************************************************************
* Provides a custom function that takes care of the deserialization of a
* serialized property field that has been removed from the class.
* This is needed for file backward compatibility with OVITO 3.11.
******************************************************************************/
RefTarget::SerializedPropertyField::CustomDeserializationFunctionPtr SelectionSet::OOMetaClass::overrideFieldDeserialization(LoadStream& stream, const SerializedPropertyField& field) const
{
    // For backward compatibility with OVITO 3.11:
    // The Pipeline class has been split from the SceneNode base class in OVITO 3.12. This means we have to handle
    // the deserialization of the nodes field here, which used to be a list of SceneNode or Pipeline objects (now only SceneNode instances).
    if(field.definingClass == &SelectionSet::OOClass() && stream.formatVersion() < 30013) {
        if(field.identifier == "nodes") {
            return [](const SerializedPropertyField& field, ObjectLoadStream& stream, RefMaker& owner) {
                qint32 numNodes;
                stream >> numNodes;
                for(qint32 i = 0; i < numNodes; i++) {
                    OORef<RefTarget> node = stream.loadObject<RefTarget>();
                    if(OORef<Pipeline> pipeline = dynamic_object_cast<Pipeline>(node))
                        node = pipeline->deserializationSceneNode();
                    static_object_cast<SelectionSet>(&owner)->_nodes.insert(&owner, PROPERTY_FIELD(nodes), i, static_object_cast<SceneNode>(std::move(node)));
                }
            };
        }
    }
    return RefTarget::OOMetaClass::overrideFieldDeserialization(stream, field);
}

}   // End of namespace
