// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <ovito/core/Core.h>

#define NCERR(x)  Ovito::NetCDFError::ncerr((x), __FILE__, __LINE__)
#define NCERRI(x, info)  Ovito::NetCDFError::ncerr_with_info((x), __FILE__, __LINE__, info)

namespace Ovito {

/// Performs one-time global initialization of the HDF5 library, which underlies both the
/// NetCDF and the VASP HDF5 file readers of OVITO. In particular, it disables HDF5's atexit()
/// library-termination handler (H5_term_library -> H5FL_garbage_coll), which would otherwise
/// crash during process shutdown when HDF5 is used from a dynamically loaded plugin.
///
/// Every code path that accesses the HDF5 or NetCDF library should call this function first.
/// This guarantees that HDF5's atexit handler is suppressed before the library is first
/// initialized, regardless of which file reader touches it first. The function is thread-safe
/// and may be called any number of times.
OVITO_NETCDF_INTEGRATION_EXPORT void initializeHDF5Library();

/**
 * RAII helper class that is used by OVITO to coordinate concurrent
 * access to the functions from the NetCDF library, which are not thread-safe.
 */
class OVITO_NETCDF_INTEGRATION_EXPORT NetCDFExclusiveAccess
{
public:

    /// Constructor, which blocks until exclusive access to the NetCDF functions is available
    /// or the current task has been canceled - whichever happens first.
    NetCDFExclusiveAccess();

    /// Destructor, which releases exclusive access to the NetCDF functions.
    ~NetCDFExclusiveAccess();

private:

    /// Indicates that this object currently has exclusive access to the NetCDF functions.
    bool _isLocked = false;

    /// The global mutex used to serialize access to the NetCDF library functions.
    static QRecursiveMutex _netcdfMutex;
};

/**
 * Namespace class, which provides error handling routines for NetCDF function calls.
 */
class OVITO_NETCDF_INTEGRATION_EXPORT NetCDFError
{
public:

    /// Checks for NetCDF errors and throws exception in case of an error.
    static void ncerr(int err, const char* file, int line);

    /// Checks for NetCDF errors and throws exception in case of an error (and attaches additional information to exception string).
    static void ncerr_with_info(int err, const char* file, int line, const QString& info);
};

}   // End of namespace
