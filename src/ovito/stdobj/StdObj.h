// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

//
// Standard precompiled header file included by all source files in this module
//

#ifndef __OVITO_STDOBJ_
#define __OVITO_STDOBJ_

#include <ovito/core/Core.h>

namespace Ovito
{
    class Property;
    class PropertyContainer;
    class PropertyContainerClass;
    using PropertyContainerClassPtr = const PropertyContainerClass*;
    using PropertyPtr = DataOORef<Property>;
    using ConstPropertyPtr = DataOORef<const Property>;
    class OwnerPropertyRef;
    class PropertyReference;
    class ElementType;
    class ElementTypeClass;
    using ElementTypeClassPtr = const ElementTypeClass*;
    class InputColumnMapping;
    template<class PropertyContainerType> class TypedInputColumnMapping;
    class InputColumnReader;
    class SimulationCell;
    class Lines;
    class LinesVis;
    class SimulationCellVis;
    class DataTable;
    class PropertyColorMapping;
    class StandardFrameLoader;
    class Vectors;
    class VectorVis;
    class TextLabelsVis;
    class BufferPythonAccessGuard; // Note: This class is defined in another plugin module (StdObjPython).
}

#endif
