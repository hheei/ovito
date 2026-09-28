// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdobj/StdObj.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include <ovito/core/dataset/data/BufferAccess.h>
#include <ovito/core/oo/RefTarget.h>

namespace Ovito {

/**
 * \brief Stores a selection set of particles or other elements and provides corresponding modification functions.
 *
 * This class is used by some modifiers to store the selection state of particles and other elements.
 *
 * This selection state can either be stored in an index-based fashion using a bit array,
 * or as a list of unique identifiers. The second storage scheme is less efficient,
 * but supports situations where the order or the number of elements changes.
 */
class OVITO_STDOBJ_EXPORT ElementSelectionSet : public RefTarget
{
    OVITO_CLASS(ElementSelectionSet)

public:

    /// Controls the mode of operation of the setSelection() method.
    enum SelectionMode {
        SelectionReplace,       //< Replace the selection with the new selection set.
        SelectionAdd,           //< Add the selection set to the existing selection.
        SelectionSubtract       //< Subtracts the selection set from the existing selection.
    };

public:

    /// Adopts the selection set from the given input property container.
    void resetSelection(const PropertyContainer* container);

    /// Clears the selection set.
    void clearSelection(const PropertyContainer* container);

    /// Inverts the selection state of all elements.
    void invertSelection(const PropertyContainer* container);

    /// Selects all elements in the given container.
    void selectAll(const PropertyContainer* container);

    /// Toggles the selection state of a single element.
    void toggleElement(const PropertyContainer* container, size_t elementIndex);

    /// Toggles the selection state of a single element.
    void toggleElementById(IdentifierIntType elementId);

    /// Toggles the selection state of a single element.
    void toggleElementByIndex(size_t elementIndex);

    /// Replaces the selection.
    void setSelection(const PropertyContainer* container, ConstPropertyPtr selection, SelectionMode mode = SelectionReplace);

    /// Copies the stored selection set into the given output selection property.
    PipelineStatus applySelection(PropertyContainer* container, BufferReadAccess<IdentifierIntType> identifierProperty);

    /// Returns the number of elements in the stored selection set.
    size_t selectedCount() const;

protected:

    /// Loads the class' contents from the given stream.
    virtual void loadFromStream(ObjectLoadStream& stream) override;

private:

    /// The selection flags array.
    DECLARE_REFERENCE_FIELD(DataOORef<const Property>, selection);

    /// Stores the selection as a list of element identifiers.
    DECLARE_PROPERTY_FIELD(QSet<qlonglong>{}, selectedIdentifiers); // Note: using qlonglong instead of IdentifierIntType for file format backward compatibility with OVITO 3.8

    /// Controls whether the object should store the identifiers of selected elements (when available).
    DECLARE_PROPERTY_FIELD(bool{true}, useIdentifiers);
};

}   // End of namespace
