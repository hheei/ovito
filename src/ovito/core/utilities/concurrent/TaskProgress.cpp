////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

#include <ovito/core/Core.h>
#include <ovito/core/utilities/concurrent/TaskProgress.h>

namespace Ovito {

/******************************************************************************
* Computes overall progress of the task, taking into account nested sub-steps.
* Returns a pair of the current total progress value and the total maximum value.
******************************************************************************/
std::pair<int, int> TaskProgress::computeTotalProgress() const
{
    float percentage;
    int totalProgressMaximum;
    if(_progressMaximum > 0) {
        percentage = (float)_progressValue / _progressMaximum;
        totalProgressMaximum = 1000;
    }
    else if(!_subProgressStack.empty()) {
        percentage = 0;
        totalProgressMaximum = 1000;
    }
    else {
        percentage = 0;
        totalProgressMaximum = 0;
    }
    for(auto level = _subProgressStack.crbegin(); level != _subProgressStack.crend(); ++level) {
        int subProgress = level->first;
        OVITO_ASSERT(subProgress >= 0);
        if(std::holds_alternative<int>(level->second)) {
            int nsteps = std::get<int>(level->second);
            OVITO_ASSERT(subProgress <= nsteps);
            percentage = (percentage + (float)subProgress) / nsteps;
        }
        else {
            const auto& weights = std::get<std::vector<int>>(level->second);
            OVITO_ASSERT(subProgress <= weights.size());
            int weightSum1 = std::accumulate(weights.cbegin(), std::next(weights.cbegin(), subProgress), 0);
            int weightSum2 = std::accumulate(std::next(weights.cbegin(), subProgress), weights.cend(), 0);
            percentage = ((float)weightSum1 + percentage * (subProgress < weights.size() ? weights[subProgress] : 0)) / (weightSum1 + weightSum2);
        }
    }
    int totalProgressValue = static_cast<int>(percentage * totalProgressMaximum);
    return std::make_pair(totalProgressValue, totalProgressMaximum);
}

}   // End of namespace
