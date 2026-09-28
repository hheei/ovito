// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdobj/StdObj.h>
#include <ovito/core/dataset/io/FileSource.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/simcell/SimulationCellVis.h>
#include "StandardFrameLoader.h"

namespace Ovito {

/******************************************************************************
 * Returns the simulation cell object, newly creating it first if necessary unless create is false.
 ******************************************************************************/
SimulationCell* StandardFrameLoader::simulationCell(bool create)
{
    if(!_simulationCell) {
        _simulationCell = state().getMutableObject<SimulationCell>();
        if(!_simulationCell && create) {
            _simulationCell = state().createObject<SimulationCell>(pipelineNode(), AffineTransformation::Zero(), true, true, true, false);
            _isSimulationCellNewlyCreated = true;
        }
    }
    return _simulationCell;
}

/******************************************************************************
* Removes any existing simulation cell object from the state.
******************************************************************************/
void StandardFrameLoader::removeSimulationCell()
{
    if(const SimulationCell* cell = state().getObject<SimulationCell>())
        state().removeObject(cell);
    _simulationCell = nullptr;
    _isSimulationCellNewlyCreated = false;
}

/******************************************************************************
* Finalizes the data loaded by a sub-class.
******************************************************************************/
void StandardFrameLoader::loadFile()
{
    // Only initialize the vis element once, when it was newly created.
    if(isSimulationCellNewlyCreated()) {
        // Set up the vis element for the simulation cell.
        if(SimulationCellVis* cellVis = dynamic_object_cast<SimulationCellVis>(simulationCell()->visElement())) {
            // Choose an appropriate line width that depends on the cell's size.
            FloatType cellDiameter = (
                    simulationCell()->cellMatrix().column(0) +
                    simulationCell()->cellMatrix().column(1) +
                    simulationCell()->cellMatrix().column(2)).length();
            cellVis->setCellLineWidth(std::max(cellDiameter * FloatType(1.4e-3), FloatType(1e-8)));
            // Take a snapshot of the object's parameter values, which serves as reference to detect future changes made by the user.
            cellVis->freezeInitialParameterValues({SNAPSHOT_PROPERTY_FIELD(SimulationCellVis::cellLineWidth)});
        }
    }

    // Log in 2d/3d flag and PBC flags set by the file reader as default values for the simulation cell.
    // This is needed for the Python code generator to detect manual changes subsequently made by the user.
    if(_simulationCell) {
        _simulationCell->freezeInitialParameterValues({
            SNAPSHOT_PROPERTY_FIELD(SimulationCell::pbcX),
            SNAPSHOT_PROPERTY_FIELD(SimulationCell::pbcY),
            SNAPSHOT_PROPERTY_FIELD(SimulationCell::pbcZ),
            SNAPSHOT_PROPERTY_FIELD(SimulationCell::is2D)});
    }
}

}   // End of namespace
