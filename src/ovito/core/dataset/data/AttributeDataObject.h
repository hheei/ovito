// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/dataset/data/DataObject.h>
#include <ovito/core/dataset/pipeline/PipelineNode.h>

namespace Ovito {

/**
 * \brief A data object holding a primitive value (e.g. a number or a string).
 */
class OVITO_CORE_EXPORT AttributeDataObject : public DataObject
{
    OVITO_CLASS(AttributeDataObject)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags) { DataObject::initializeObject(flags); }

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags, QVariant&& value) {
        DataObject::initializeObject(flags);
        _value.mutableValue() = std::move(value);
    }

    /// Returns the display title of this object.
    virtual QString objectTitle() const override {
        if(!identifier().isEmpty())
            return identifier();
        return DataObject::objectTitle();
    }

protected:

    /// Loads the class' contents from the given stream.
    virtual void loadFromStream(ObjectLoadStream& stream) override;

private:

    /// The attribute's value.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QVariant{}, value, setValue, PROPERTY_FIELD_WAS_RUNTIME_PROPERTY_FIELD);
};

}   // End of namespace
