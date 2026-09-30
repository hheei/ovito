# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

ovito_add_cpp_test(
    NAME tst_automation_contracts
    SOURCES tst_automation_contracts.cpp
    LINK_LIBRARIES Core
)

# The runtime half of the package contract probes real interpreters; see the file's header on how it stays hermetic.
ovito_add_cpp_test(
    NAME tst_python_environment_probe
    SOURCES tst_python_environment_probe.cpp
    LINK_LIBRARIES Core
)

# The data bridge needs an interpreter as well, and it is the only test that moves real bytes across the seam.
ovito_add_cpp_test(
    NAME tst_python_data_bridge
    SOURCES tst_python_data_bridge.cpp
    LINK_LIBRARIES Core
)

# The schema preview reads a user script without running it (AST mode) or by explicitly importing it. It needs an
# interpreter on the machine and skips its cases without one; nothing here touches the OVITO object model.
ovito_add_cpp_test(
    NAME tst_python_schema_preview
    SOURCES tst_python_schema_preview.cpp
    LINK_LIBRARIES Core
)

# The discovery half of the local protocol is a value type and a file, so it needs neither an application nor an event
# loop; it works in a private session directory of its own.
ovito_add_cpp_test(
    NAME tst_session_descriptor
    SOURCES tst_session_descriptor.cpp
    LINK_LIBRARIES Core
)
