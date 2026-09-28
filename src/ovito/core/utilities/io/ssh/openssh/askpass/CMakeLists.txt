# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Find the required Qt modules.
OVITO_FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS Core Gui Widgets REQUIRED)

# Builds the main executable of the application.
ADD_EXECUTABLE(ssh_askpass Main.cpp)

# Link Qt framework
TARGET_LINK_LIBRARIES(ssh_askpass PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

# Under Linux, Qt's Xcb platform plugin will dynamically load the DBus Qt module.
# To avoid the dynamic linker pulling the wrong versions of the Qt module from the
# system path, we make the executable preload the module on application startup.
IF(LINUX AND OVITO_REDISTRIBUTABLE_PACKAGE)
    OVITO_FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS DBus REQUIRED)
    TARGET_LINK_LIBRARIES(ssh_askpass PRIVATE Qt6::DBus)
ENDIF()

# Place the executable into the right installation directory.
SET_TARGET_PROPERTIES(ssh_askpass PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OVITO_BINARY_DIRECTORY}")
INSTALL(TARGETS ssh_askpass DESTINATION "${OVITO_RELATIVE_BINARY_DIRECTORY}")

# Inherit INSTALL_RPATH target property from main Ovito executable target.
SET_TARGET_PROPERTIES(ssh_askpass PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
SET_TARGET_PROPERTIES(ssh_askpass PROPERTIES INSTALL_RPATH "$<TARGET_PROPERTY:Ovito,INSTALL_RPATH>")