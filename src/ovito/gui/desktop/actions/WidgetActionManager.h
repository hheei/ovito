// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>

namespace Ovito {

/*
 * \brief Manages all available user interface actions.
 */
class OVITO_GUI_EXPORT WidgetActionManager : public ActionManager
{
    Q_OBJECT

public:

    /// Constructor.
    WidgetActionManager(QObject* parent, MainWindowUI& ui);

    /// Returns the main window user interface this action manager belongs to.
    MainWindowUI& ui() const { return static_cast<MainWindowUI&>(ActionManager::ui()); }

    /// Returns the main window this action manager belongs to.
    MainWindow* mainWindow() const { return ui().mainWindow(); }

private Q_SLOTS:

    /// Is called when the user selects a command in the quick search field.
    void onQuickSearchCommandSelected(const QModelIndex& index);

    void on_Quit_triggered();
    void on_HelpAbout_triggered();
    void on_HelpSystemInfo_triggered();
    void on_HelpShowOnlineHelp_triggered();
    void on_HelpShowScriptingReference_triggered();
    void on_HelpRequestFeature_triggered();
    void on_FileOpen_triggered();
    void on_FileSave_triggered();
    void on_FileSaveAs_triggered();
    void on_FileImport_triggered();
    void on_FileRemoteImport_triggered();
    void on_FileExport_triggered();
    void on_FileNewWindow_triggered();
    void on_Settings_triggered();
    void on_AnimationSettings_triggered();
    void on_RenderActiveViewport_triggered();
    void on_ClonePipeline_triggered();
    void on_RenamePipeline_triggered();
    void on_NewPipelineFileSource_triggered();
    void on_ConfigureViewportGraphics_triggered();

private:

    void setupCommandSearch();
};

}   // End of namespace
