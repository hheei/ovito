# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Find the required Qt modules.
OVITO_FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS Widgets REQUIRED)

# Define plugin module.
OVITO_STANDARD_PLUGIN(ViewportWindow
    SOURCES
        WidgetViewportWindow.cpp
    PLUGIN_DEPENDENCIES
        GuiBase
    LIB_DEPENDENCIES
        Qt6::Widgets
)

# Propagate list of plugins to parent scope.
SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)
