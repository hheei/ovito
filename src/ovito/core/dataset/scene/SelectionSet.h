// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/oo/RefTarget.h>
#include "SceneNode.h"

namespace Ovito {

/**
 * \brief Stores a selection of scene nodes.
 *
 * This selection set class holds a reference list to all SceneNode objects
 * that are selected.
 *
 * The current selection set can be accessed via the DataSetManager::currentSelection() method.
 */
class OVITO_CORE_EXPORT SelectionSet : public RefTarget
{
public:
    /// Give this class its own metaclass.
    class OVITO_CORE_EXPORT SelectionSetClass : public RefTarget::OOMetaClass
    {
    public:
        /// Inherit constructor from base class.
        using RefTarget::OOMetaClass::OOMetaClass;

        /// Provides a custom function that takes care of the deserialization of a serialized property field.
        /// This is needed for backward compatibility with OVITO 3.11.
        virtual SerializedPropertyField::CustomDeserializationFunctionPtr overrideFieldDeserialization(LoadStream& stream, const SerializedPropertyField& field) const override;
    };
    OVITO_CLASS_META(SelectionSet, SelectionSetClass)

public:

    /// \brief Creates an empty selection set.
    using RefTarget::RefTarget;

    /// \brief Adds a scene node to this selection set.
    /// \param node The node to be added.
    /// \undoable
    void push_back(OORef<SceneNode> node);

    /// \brief Inserts a scene node into this selection set.
    /// \param index The index at which to insert the node into the list.
    /// \param node The node to be inserted.
    /// \undoable
    void insert(qsizetype index, OORef<SceneNode> node);

    /// \brief Removes a scene node from this selection set.
    /// \param node The node to be unselected.
    /// \undoable
    void remove(const SceneNode* node);

    /// \brief Removes a scene node from this selection set.
    /// \param index The index of the node to be unselected.
    /// \undoable
    void removeByIndex(qsizetype index) { _nodes.remove(this, PROPERTY_FIELD(nodes), index); }

    /// \brief Clears the selection.
    ///
    /// All nodes are removed from the selection set.
    /// \undoable
    void clear() { _nodes.clear(this, PROPERTY_FIELD(nodes)); }

    /// \brief Resets the selection set to contain only the given node.
    /// \param node The node to be selected.
    void setNode(SceneNode* node) {
        if(node)
            setNodes({node});
        else
            clear();
    }

    /// \brief Returns the first scene node from the selection, or NULL if the set is empty.
    SceneNode* firstNode() const {
        return nodes().empty() ? nullptr : nodes().front();
    }

private:

    /// References to the selected scene nodes.
    DECLARE_MODIFIABLE_VECTOR_REFERENCE_FIELD_FLAGS(OORef<SceneNode>, nodes, setNodes, PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES | PROPERTY_FIELD_NEVER_CLONE_TARGET);
};

}   // End of namespace
