///////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  OVITO is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
///////////////////////////////////////////////////////////////////////////////

#include <ovito/core/Core.h>
#include <ovito/core/utilities/concurrent/Task.h>
#include "NetCDFIntegration.h"

#include <netcdf.h>
#include <hdf5.h>

namespace Ovito {

// The global mutex used to serialize access to the NetCDF library functions.
QRecursiveMutex NetCDFExclusiveAccess::_netcdfMutex;

/******************************************************************************
* Performs one-time global initialization of the HDF5 library.
******************************************************************************/
void initializeHDF5Library()
{
    static std::once_flag flag;
    std::call_once(flag, []() {
        // Prevent HDF5 from installing its atexit() library-termination handler. That handler
        // (H5_term_library -> H5FL_garbage_coll) frees HDF5's internal free-list pools during
        // process shutdown and crashes when invoked from a dynamically loaded plugin. Calling
        // H5dont_atexit() before the library is first initialized suppresses the handler; it
        // only sets a flag and does not initialize the library itself. The operating system
        // reclaims all memory at process exit, so skipping HDF5's own cleanup is safe.
        H5dont_atexit();
        // Suppress the printing of HDF5 error messages to the console.
        H5Eset_auto2(H5E_DEFAULT, nullptr, nullptr);
    });
}

/******************************************************************************
* Constructor, which blocks until exclusive access to the NetCDF functions is
* available or the current task has been canceled.
******************************************************************************/
NetCDFExclusiveAccess::NetCDFExclusiveAccess()
{
    while(!_netcdfMutex.tryLock(10)) {
        this_task::throwIfCanceled();
    }
    _isLocked = true;
}

/******************************************************************************
* Destructor, which releases exclusive access to the NetCDF functions.
******************************************************************************/
NetCDFExclusiveAccess::~NetCDFExclusiveAccess()
{
    if(_isLocked)
        _netcdfMutex.unlock();
}

/******************************************************************************
* Check for NetCDF error and throw exception
******************************************************************************/
void NetCDFError::ncerr(int err, const char* file, int line)
{
    if(err != NC_NOERR)
        throw Exception(QString("NetCDF I/O error: %1 (line %2 of %3)").arg(QString(nc_strerror(err))).arg(line).arg(file));
}

/******************************************************************************
* Check for NetCDF error and throw exception (and attach additional information
* to exception string)
******************************************************************************/
void NetCDFError::ncerr_with_info(int err, const char* file, int line, const QString& info)
{
    if(err != NC_NOERR)
        throw Exception(QString("NetCDF I/O error: %1 %2 (line %3 of %4)").arg(QString(nc_strerror(err))).arg(info).arg(line).arg(file));
}

}   // End of namespace
