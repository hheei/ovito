// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include "GuiSettings.h"

#include <ovito/gui/base/app/IconTheme.h>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QSettings>
#include <QStyleHints>

namespace Ovito {

namespace {

// The settings keys of the workbench shell. They are spelled out here once and never at a call site, because renaming or
// moving one of them silently discards the settings of existing users.
constexpr QLatin1String MainWindowGroup("app/mainwindow");
constexpr QLatin1String MainWindowGeometryKey("geometry");
constexpr QLatin1String MainWindowStateKey("state");

constexpr QLatin1String WorkbenchGeometryKey("app/window/geometry");
constexpr QLatin1String WorkbenchMaximizedKey("app/window/maximized");

constexpr QLatin1String AutomaticColorSchemeKey("ui/automatic_dark_mode");
constexpr QLatin1String KeepDirectoryHistoryKey("file/keep_dir_history");
constexpr QLatin1String FileDialogTypeKey("file/dialog_type");
constexpr QLatin1String DirectoryHistoryGroupPrefix("filedialog/");
constexpr QLatin1String DirectoryHistoryKey("history");
constexpr QLatin1String MultiFileImportModeKey("file/multi_file_import_mode");
constexpr QLatin1String SessionDirectoryGroup("file/scene");
constexpr QLatin1String SessionDirectoryKey("last_directory");
constexpr QLatin1String GraphicsAdapterSetupKey("viewport/adapter_setup_done");

/// The number of recently used directories kept per kind of file dialog, matching what the classic frontend kept.
constexpr int MaxDirectoryHistorySize = 1;

}   // End of anonymous namespace

/******************************************************************************
* Returns the application-wide instance of the settings facade.
******************************************************************************/
GuiSettings& GuiSettings::instance()
{
    static GuiSettings settings;
    // The instance can be created before the Qt application object exists (the classic frontend asks for the color scheme
    // while it sets up its platform integration), so the subscription to the platform's color scheme is attempted here
    // rather than in the constructor; it happens once.
    settings.observeSystemColorScheme();
    return settings;
}

/******************************************************************************
* Constructor.
******************************************************************************/
GuiSettings::GuiSettings() = default;

/******************************************************************************
* Destructor.
******************************************************************************/
GuiSettings::~GuiSettings() = default;

/******************************************************************************
* Subscribes to the color scheme of the platform as soon as there is a platform to ask.
******************************************************************************/
void GuiSettings::observeSystemColorScheme()
{
    // The frontends derive their palette from the color scheme the operating system reports, so a change of the OS theme
    // has to invalidate what they display. This cannot simply happen in the constructor: the singleton is created while
    // the classic frontend sets up its platform integration, which can be before the Qt application object exists, and a
    // run without one has no user interface at all.
    if(_observingColorScheme)
        return;
    if(QGuiApplication* app = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        connect(app->styleHints(), &QStyleHints::colorSchemeChanged, this, &GuiSettings::systemColorSchemeChanged);
        _observingColorScheme = true;
        applyIconTheme();
    }
}

/******************************************************************************
* Is called when the operating system reports a different color scheme.
******************************************************************************/
void GuiSettings::systemColorSchemeChanged()
{
    applyIconTheme();
    Q_EMIT changed();
}

/******************************************************************************
* Makes the icon theme of the process match the current color scheme.
******************************************************************************/
void GuiSettings::applyIconTheme()
{
    // Both frontends take their icons from the same icon set, so the decision which of its two themes is the current one
    // belongs here next to the color scheme it is derived from.
    IconTheme::apply(usingDarkTheme());
}

/******************************************************************************
* Returns whether the UI follows the color scheme reported by the operating system.
******************************************************************************/
bool GuiSettings::followsSystemColorScheme() const
{
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    // Always the case here, and not a user choice: this is what the frontends have done since dark mode support exists.
    return true;
#else
    return QSettings().value(AutomaticColorSchemeKey, false).toBool();
#endif
}

/******************************************************************************
* Sets whether the UI should follow the operating system's color scheme.
******************************************************************************/
void GuiSettings::setFollowsSystemColorScheme(bool enable)
{
    if(followsSystemColorScheme() == enable)
        return;

    QSettings settings;
    if(enable)
        settings.setValue(AutomaticColorSchemeKey, true);
    else
        settings.remove(AutomaticColorSchemeKey);
    applyIconTheme();
    Q_EMIT changed();
}

/******************************************************************************
* Returns whether the UI is currently dark.
******************************************************************************/
bool GuiSettings::usingDarkTheme() const
{
    if(!followsSystemColorScheme())
        return false;
    if(!QGuiApplication::instance())
        return false;
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

/******************************************************************************
* Returns the remembered main window geometry.
******************************************************************************/
QByteArray GuiSettings::mainWindowGeometry() const
{
    QSettings settings;
    settings.beginGroup(MainWindowGroup);
    return settings.value(MainWindowGeometryKey).toByteArray();
}

/******************************************************************************
* Stores the main window geometry.
******************************************************************************/
void GuiSettings::setMainWindowGeometry(const QByteArray& geometry)
{
    if(mainWindowGeometry() == geometry)
        return;

    QSettings settings;
    settings.beginGroup(MainWindowGroup);
    // An empty value means "nothing stored", so the key is removed instead of being written as an empty blob: a fresh
    // settings file stays free of entries that say nothing.
    if(geometry.isEmpty())
        settings.remove(MainWindowGeometryKey);
    else
        settings.setValue(MainWindowGeometryKey, geometry);
    Q_EMIT changed();
}

/******************************************************************************
* Returns the remembered layout of the docked widgets.
******************************************************************************/
QByteArray GuiSettings::mainWindowState() const
{
    QSettings settings;
    settings.beginGroup(MainWindowGroup);
    return settings.value(MainWindowStateKey).toByteArray();
}

/******************************************************************************
* Stores the layout of the docked widgets.
******************************************************************************/
void GuiSettings::setMainWindowState(const QByteArray& state)
{
    if(mainWindowState() == state)
        return;

    QSettings settings;
    settings.beginGroup(MainWindowGroup);
    if(state.isEmpty())
        settings.remove(MainWindowStateKey);
    else
        settings.setValue(MainWindowStateKey, state);
    Q_EMIT changed();
}

/******************************************************************************
* Returns the remembered window rectangle of a frontend without widgets.
******************************************************************************/
QRect GuiSettings::workbenchWindowGeometry() const
{
    const QVariant value = QSettings().value(WorkbenchGeometryKey);
    // An invalid rectangle tells the caller that there is nothing to restore, so the frontend can pick its own default.
    return value.canConvert<QRect>() ? value.toRect() : QRect();
}

/******************************************************************************
* Stores the window rectangle of a frontend without widgets.
******************************************************************************/
void GuiSettings::setWorkbenchWindowGeometry(const QRect& geometry)
{
    if(workbenchWindowGeometry() == geometry)
        return;

    if(geometry.isValid())
        QSettings().setValue(WorkbenchGeometryKey, geometry);
    else
        QSettings().remove(WorkbenchGeometryKey);
    Q_EMIT changed();
}

/******************************************************************************
* Returns whether the window was maximized when the frontend last closed it.
******************************************************************************/
bool GuiSettings::isWorkbenchWindowMaximized() const
{
    return QSettings().value(WorkbenchMaximizedKey, false).toBool();
}

/******************************************************************************
* Stores whether the window is maximized.
******************************************************************************/
void GuiSettings::setWorkbenchWindowMaximized(bool maximized)
{
    if(isWorkbenchWindowMaximized() == maximized)
        return;

    // Not being maximized is the default, so the key is only written when the window was maximized.
    if(maximized)
        QSettings().setValue(WorkbenchMaximizedKey, true);
    else
        QSettings().remove(WorkbenchMaximizedKey);
    Q_EMIT changed();
}

/******************************************************************************
* Returns whether each kind of file dialog keeps its own directory history.
******************************************************************************/
bool GuiSettings::keepDirectoryHistory() const
{
    return QSettings().value(KeepDirectoryHistoryKey, true).toBool();
}

/******************************************************************************
* Sets whether each kind of file dialog keeps its own directory history.
******************************************************************************/
void GuiSettings::setKeepDirectoryHistory(bool enable)
{
    if(keepDirectoryHistory() == enable)
        return;

    QSettings settings;
    if(enable)
        settings.remove(KeepDirectoryHistoryKey);   // The default is "on", so it is not written out.
    else
        settings.setValue(KeepDirectoryHistoryKey, false);
    Q_EMIT changed();
}

/******************************************************************************
* Returns whether the user prefers Qt's widget-based file dialog.
******************************************************************************/
bool GuiSettings::preferQtFileDialog() const
{
    return QSettings().value(FileDialogTypeKey).toString() == QLatin1String("qt");
}

/******************************************************************************
* Sets whether the user prefers Qt's widget-based file dialog.
******************************************************************************/
void GuiSettings::setPreferQtFileDialog(bool prefer)
{
    if(preferQtFileDialog() == prefer)
        return;

    QSettings settings;
    if(prefer)
        settings.setValue(FileDialogTypeKey, QStringLiteral("qt"));
    else
        settings.remove(FileDialogTypeKey);
    Q_EMIT changed();
}

/******************************************************************************
* Returns the directories the given kind of file dialog was used in last.
******************************************************************************/
QStringList GuiSettings::recentDirectories(const QString& dialogClass) const
{
    QSettings settings;
    settings.beginGroup(DirectoryHistoryGroupPrefix + dialogClass);
    return settings.value(DirectoryHistoryKey).toStringList();
}

/******************************************************************************
* Remembers a directory as the most recently used one of the given kind of file dialog.
******************************************************************************/
void GuiSettings::rememberDirectory(const QString& dialogClass, const QString& directory)
{
    if(directory.isEmpty())
        return;

    // The setting is owned by this facade, so every caller respects it: the classic frontend used to check it at some
    // call sites only and remembered the directories anyway at others, such as the import dialog.
    if(!keepDirectoryHistory())
        return;

    QStringList history = recentDirectories(dialogClass);
    const qsizetype index = history.indexOf(directory);
    if(index >= 0)
        history.move(index, 0);
    else {
        history.push_front(directory);
        while(history.size() > MaxDirectoryHistorySize)
            history.pop_back();
    }

    QSettings settings;
    settings.beginGroup(DirectoryHistoryGroupPrefix + dialogClass);
    settings.setValue(DirectoryHistoryKey, history);
    Q_EMIT changed();
}

/******************************************************************************
* Returns how several files of the same kind should be imported.
******************************************************************************/
FileImporter::MultiFileImportMode GuiSettings::multiFileImportMode() const
{
#ifdef OVITO_BUILD_PROFESSIONAL
    return QSettings().value(MultiFileImportModeKey, FileImporter::ImportAsTrajectory).value<FileImporter::MultiFileImportMode>();
#else
    return FileImporter::ImportAsTrajectory;
#endif
}

/******************************************************************************
* Sets how several files of the same kind should be imported.
******************************************************************************/
void GuiSettings::setMultiFileImportMode(FileImporter::MultiFileImportMode mode)
{
    if(multiFileImportMode() == mode)
        return;

    QSettings settings;
    if(mode != FileImporter::ImportAsTrajectory)
        settings.setValue(MultiFileImportModeKey, mode);
    else
        settings.remove(MultiFileImportModeKey);   // Importing a sequence is the default of the professional edition.
    Q_EMIT changed();
}

/******************************************************************************
* Returns the directory a session file was last saved to.
******************************************************************************/
QString GuiSettings::sessionFileDirectory() const
{
    QSettings settings;
    settings.beginGroup(SessionDirectoryGroup);
    return settings.value(SessionDirectoryKey).toString();
}

/******************************************************************************
* Stores the directory a session file was saved to.
******************************************************************************/
void GuiSettings::setSessionFileDirectory(const QString& directory)
{
    if(sessionFileDirectory() == directory)
        return;

    QSettings settings;
    settings.beginGroup(SessionDirectoryGroup);
    if(directory.isEmpty())
        settings.remove(SessionDirectoryKey);
    else
        settings.setValue(SessionDirectoryKey, directory);
    Q_EMIT changed();
}

/******************************************************************************
* Returns whether the user has already confirmed the GPU adapter selection.
******************************************************************************/
bool GuiSettings::graphicsAdapterSetupDone() const
{
    return QSettings().value(GraphicsAdapterSetupKey, false).toBool();
}

/******************************************************************************
* Sets whether the GPU adapter selection has been confirmed.
******************************************************************************/
void GuiSettings::setGraphicsAdapterSetupDone(bool done)
{
    if(graphicsAdapterSetupDone() == done)
        return;

    QSettings settings;
    if(done)
        settings.setValue(GraphicsAdapterSetupKey, true);
    else
        settings.remove(GraphicsAdapterSetupKey);
    Q_EMIT changed();
}

}   // End of namespace
