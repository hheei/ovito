// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>

namespace Ovito {

#ifdef OVITO_DEBUG
/******************************************************************************
* Destructor.
******************************************************************************/
PipelineFlowState::~PipelineFlowState()
{
}
#endif

/******************************************************************************
* Makes the last object in the data path mutable and returns a pointer to the mutable copy.
* Also update the data path to point to the new object.
******************************************************************************/
DataObject* PipelineFlowState::makeMutableInplace(ConstDataObjectPath& path)
{
    OVITO_ASSERT(path.empty() == false);
    OVITO_ASSERT(path.front() == data());
    DataObject* parent = mutableData();
    path.front() = parent;
    for(auto obj = std::next(path.begin()); obj != path.end(); ++obj) {
        *obj = parent = parent->makeMutable(*obj);
    }
    return parent;
}

}   // End of namespace
