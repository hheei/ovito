# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Define the GUI module, which provides the user interface for the parent module.
OVITO_STANDARD_PLUGIN(CorrelationFunctionPluginGui
    SOURCES
        SpatialCorrelationFunctionModifierEditor.cpp
    PRIVATE_LIB_DEPENDENCIES Qwt
    PLUGIN_DEPENDENCIES CorrelationFunctionPlugin ParticlesGui
    GUI_PLUGIN
    HAS_NO_EXPORTS
)
