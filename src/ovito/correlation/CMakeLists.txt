# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Define the plugin module.
OVITO_STANDARD_PLUGIN(CorrelationFunctionPlugin
    SOURCES
        SpatialCorrelationFunctionModifier.cpp
    PLUGIN_DEPENDENCIES Particles
    PRIVATE_LIB_DEPENDENCIES kissfft
)

# Propagate list of plugins to parent scope.
SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)
