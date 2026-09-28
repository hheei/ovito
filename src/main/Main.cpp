// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/app/GuiApplication.h>

#if defined(OVITO_BUILD_PLUGIN_PYSCRIPT) && !defined(OVITO_BUILD_BASIC)
    // Explicitly build 'ovito' executable against Python library.
    // The following include directive will pull in the Python headers.
    #include <ovito/pyscript/PyScript.h>
#endif

/**
 * This is the main entry point for the graphical desktop application.
 *
 * Note that most of the application logic is found in the Core and the Gui
 * library modules of OVITO, not in this executable module.
 */
int main(int argc, char** argv)
{
#if defined(OVITO_BUILD_PLUGIN_PYSCRIPT) && !defined(OVITO_BUILD_BASIC)
    // This (useless) call to a Python C API function is needed to force-link the Python library into the executable.
    // We have to make sure the Python lib gets loaded into process memory before any of OVITO's plugin Python modules
    // are loaded, because they depend on the Python lib but were not explicitly linking to it.
    if(Py_IsInitialized())
        return 1;
#endif

    // Initialize the application.
    Ovito::OORef<Ovito::GuiApplication> app = Ovito::OORef<Ovito::GuiApplication>::create();

    int exitCode = 1;
    if(app->initialize(argc, argv)) {

        bool runEventLoop = true;
        if(!QCoreApplication::instance()) {
            // Application::initialize() may return successfully but without creating a Qt application object.
            // This happens, for example, when the --version command line parameter has been specified by the user.
            // In this case we quit immediately without entering the event loop.
            app->taskManager().executePendingWork();
            exitCode = app->taskManager().isShuttingDown() ? 1 : 0;

            // A Python script may have created an OVITO main window in the meantime.
            // If so, we still want to enter the Qt event loop to keep the window open.
            runEventLoop = (QGuiApplication::topLevelWindows().isEmpty() == false);
        }

        // Check again if a Qt application object has been created. If so, enter the Qt event loop.
        if(QCoreApplication::instance() && runEventLoop) {
            exitCode = QCoreApplication::exec();
        }
    }
    app->shutdown();

    return exitCode;
}
