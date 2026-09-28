# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# Define the GUI module, which provides the user interface for the parent module.
OVITO_STANDARD_PLUGIN(MeshGui
    SOURCES
        surface/SurfaceMeshVisEditor.cpp
        surface/SurfaceMeshInspectionApplet.cpp
        io/ParaViewVTMImporterEditor.cpp
    PLUGIN_DEPENDENCIES Mesh StdObjGui
    GUI_PLUGIN
    HAS_NO_EXPORTS
)
