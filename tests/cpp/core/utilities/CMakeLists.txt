###############################################################################
#
#  Copyright 2026 OVITO GmbH, Germany
#
###############################################################################

ovito_add_cpp_test(
    NAME tst_containers
    SOURCES tst_containers.cpp
    LINK_LIBRARIES Core
)

ADD_SUBDIRECTORY(concurrent)
