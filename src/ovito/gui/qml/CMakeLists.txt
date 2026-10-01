# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Find the required Qt modules for the Qt Quick / QML frontend.
OVITO_FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS Quick QuickControls2 REQUIRED)

# Define plugin module.
OVITO_STANDARD_PLUGIN(GuiQml
    SOURCES
        app/QmlFrontend.cpp
        app/QmlFrontendService.cpp
        mainwin/QmlMainWindowUI.cpp
        mainwin/QmlIcons.cpp
        mainwin/QmlViewportController.cpp
        mainwin/QmlViewportLayout.cpp
        mainwin/QmlWorkbenchController.cpp
        models/QmlAnimationModel.cpp
        models/QmlPipelineController.cpp
        viewport/QuickViewportWindow.cpp
        viewport/QuickViewportItem.cpp
        viewport/QuickViewportRenderer.cpp
        viewport/QuickRendererService.cpp
        viewport/QmlViewportMenu.cpp
        resources/qml.qrc
    PLUGIN_DEPENDENCIES
        GuiBase
    PRECOMPILED_HEADERS
        QmlFrontend.h
)

# Qt6::QuickControls2 is what makes the shell's controls (buttons, progress bar, dialogs) available; Qt6::QuickDialogs is
# the file selection dialog. The QML modules themselves are loaded by the QML engine at run time.
TARGET_LINK_LIBRARIES(GuiQml PUBLIC Qt6::Quick Qt6::QuickControls2)

# Inject the copyright notice text (cmake/Version.cmake) into the source code of the About dialog of this frontend, the
# same way the classic frontend's About dialog receives it.
SET_PROPERTY(SOURCE mainwin/QmlWorkbenchController.cpp
    APPEND PROPERTY COMPILE_DEFINITIONS
    "OVITO_COPYRIGHT_NOTICE=\"${OVITO_COPYRIGHT_NOTICE}\"")

# Build the Phase 1 prototype application, which renders an OVITO scene inside a Qt Quick window.
# This is a technical spike; it is not the final frontend entry point (see docs/design/UI_PLAN.md).
ADD_SUBDIRECTORY(spike)

# Propagate list of plugins to parent scope.
SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)
