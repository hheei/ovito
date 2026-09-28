# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Find the required Qt modules for the Qt Quick / QML frontend.
OVITO_FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS Quick REQUIRED)

# Define plugin module.
OVITO_STANDARD_PLUGIN(GuiQml
    SOURCES
        mainwin/QmlMainWindowUI.cpp
        mainwin/QmlViewportController.cpp
        viewport/QuickViewportWindow.cpp
        viewport/QuickViewportItem.cpp
        viewport/QuickViewportRenderer.cpp
        resources/qml.qrc
    PLUGIN_DEPENDENCIES
        GuiBase
    PRECOMPILED_HEADERS
        QmlFrontend.h
)

TARGET_LINK_LIBRARIES(GuiQml PUBLIC Qt6::Quick)

# Build the Phase 1 prototype application, which renders an OVITO scene inside a Qt Quick window.
# This is a technical spike; it is not the final frontend entry point (see docs/design/UI_PLAN.md).
ADD_SUBDIRECTORY(spike)

# Propagate list of plugins to parent scope.
SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)
