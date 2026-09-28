###############################################################################
#
#  Copyright 2026 OVITO GmbH, Germany
#
###############################################################################

ovito_add_cpp_test(
    NAME tst_concurrent
    SOURCES tst_concurrent.cpp
    LINK_LIBRARIES Core
)

ovito_add_cpp_test(
    NAME tst_concurrent_pool
    SOURCES tst_concurrent_pool.cpp
    LINK_LIBRARIES Core
)

ovito_add_cpp_test(
    NAME tst_concurrent_loops
    SOURCES tst_concurrent_loops.cpp
    LINK_LIBRARIES Core
)
