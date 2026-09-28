# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

ADD_SUBDIRECTORY(base)
ADD_SUBDIRECTORY(vpwindow)
IF(OVITO_BUILD_APP)
    ADD_SUBDIRECTORY(desktop)
ENDIF()

# Experimental Qt Quick / QML frontend. It does not depend on the classic QtWidgets frontend and can be built
# alongside it. See docs/design/UI_PLAN.md.
IF(OVITO_BUILD_QML_FRONTEND)
    ADD_SUBDIRECTORY(qml)
ENDIF()

# Propagate list of plugins to parent scope.
SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)
