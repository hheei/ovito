// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/core/dataset/io/FileSourceImporter.h>

namespace Ovito {

/**
 * \brief Base class for file parsers that load property objects and/or simulation cell definitions.
 */
class OVITO_STDOBJ_EXPORT StandardFrameLoader : public FileSourceImporter::FrameLoader
{
public:

    /// Constructor.
    using FileSourceImporter::FrameLoader::FrameLoader;

    /// Returns the simulation cell object, newly creating it first if necessary unless create is false.
    SimulationCell* simulationCell(bool create = true);

    /// Removes any existing simulation cell object from the state.
    void removeSimulationCell();

    /// Returns true if the file reader has already loaded a simulation cell definition.
    bool hasSimulationCell() const { return _simulationCell != nullptr; }

    /// Indicates that the simulation cell object was newly created by this file reader.
    bool isSimulationCellNewlyCreated() const { return _isSimulationCellNewlyCreated; }

    /// Registers a new numeric element type with the given ID and an optional name string.
    template<typename StringType>
        requires (std::same_as<StringType, QString> || std::same_as<StringType, QStringView> || std::same_as<StringType, QLatin1String>)
    const ElementType* addNumericType(const PropertyContainerClass& containerClass, Property* typedProperty, int id, const StringType& name, ElementTypeClassPtr elementTypeClass = {}) {
        return typedProperty->addNumericType(containerClass, id, name, elementTypeClass);
    }

    /// Registers a new numeric element type with the given ID and an optional name string.
    const ElementType* addNumericType(const PropertyContainerClass& containerClass, Property* typedProperty, int id, const std::string& name, ElementTypeClassPtr elementTypeClass = {}) {
        return addNumericType(containerClass, typedProperty, id, QLatin1String(name.c_str(), name.size()), elementTypeClass);
    }

    /// Registers a new named element type and automatically gives it a unique numeric ID.
    template<typename StringType>
        requires (std::same_as<StringType, QString> || std::same_as<StringType, QStringView> || std::same_as<StringType, QLatin1String>)
    const ElementType* addNamedType(const PropertyContainerClass& containerClass, Property* typedProperty, const StringType& name, ElementTypeClassPtr elementTypeClass = {}, int startNumericIdAt = 1) {
        return typedProperty->addNamedType(containerClass, name, elementTypeClass, startNumericIdAt);
    }

    /// Registers a new named element type and automatically gives it a unique numeric ID.
    const ElementType* addNamedType(const PropertyContainerClass& containerClass, Property* typedProperty, const std::string& name, ElementTypeClassPtr elementTypeClass = {}, int startNumericIdAt = 1) {
        return addNamedType(containerClass, typedProperty, QLatin1String(name.c_str(), name.size()), elementTypeClass, startNumericIdAt);
    }

protected:

    /// Finalizes the particle data loaded by a sub-class.
    virtual void loadFile() override;

private:

    /// The simulation cell object.
    SimulationCell* _simulationCell = nullptr;

    /// Indicates that the simulation cell object was newly created by this file reader.
    bool _isSimulationCellNewlyCreated = false;
};

}   // End of namespace
