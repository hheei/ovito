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
