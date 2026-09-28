// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


namespace Ovito {

/******************************************************************************
* This macro performs a runtime-time assertion check.
******************************************************************************/
#ifdef OVITO_DEBUG
    #ifndef __SYCL_DEVICE_ONLY__
        #define OVITO_ASSERT(condition) Q_ASSERT(condition)
    #else
        #define OVITO_ASSERT(condition) assert(condition)
    #endif
#else
    #define OVITO_ASSERT(condition)
#endif

/******************************************************************************
* This macro performs a runtime-time assertion check.
******************************************************************************/
#ifdef OVITO_DEBUG
    #ifndef __SYCL_DEVICE_ONLY__
        #define OVITO_ASSERT_MSG(condition, where, what) Q_ASSERT_X(condition, where, what)
    #else
        #define OVITO_ASSERT_MSG(condition, where, what) OVITO_ASSERT(condition)
    #endif
#else
    #define OVITO_ASSERT_MSG(condition, where, what)
#endif

/******************************************************************************
* This macro performs a compile-time check.
******************************************************************************/
#define OVITO_STATIC_ASSERT(condition) Q_STATIC_ASSERT(condition)

/******************************************************************************
* This macro validates a memory pointer in debug mode.
* If the given pointer does not point to a valid position in memory then
* the debugger is activated.
******************************************************************************/
#define OVITO_CHECK_POINTER(pointer) OVITO_ASSERT_MSG((pointer), "OVITO_CHECK_POINTER", "Invalid object pointer.");

}   // End of namespace


