// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/dataset/io/FileImporter.h>

#include <QByteArray>
#include <QRect>
#include <QStringList>

namespace Ovito {

/**
 * \brief The single place that knows which settings the workbenches persist, under which key, with which default and in
 *        which form.
 *
 * The two frontends are built on the same shell layer, so a setting one of them stores is a setting the other one has to
 * be able to read: the color scheme decided in the classic settings dialog, the directory a file dialog should reopen
 * in, the geometry of the main window. Keeping the key names, the defaults and the value formats here means that neither
 * frontend names a storage path of its own, that a settings dialog of either frontend has one place to write to, and
 * that a QML shell does not invent a second, subtly different convention for the same thing.
 *
 * \note The boundary: this class owns the settings of the *workbench shell* — window, file dialogs, color scheme, and
 *       the flags the first start sets. Settings of an individual feature (property colors, modifier parameters,
 *       pipeline caches, ...) stay with the feature that owns them, even when that feature lives in the GUI layer.
 *
 * The values live in the application's `QSettings` store — the same native store the classic frontend has always
 * written to — so no settings of existing users have to be migrated.
 */
class OVITO_GUIBASE_EXPORT GuiSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool usingDarkTheme READ usingDarkTheme NOTIFY changed)

public:

    /// Returns the application-wide instance.
    static GuiSettings& instance();

    // ---- Color scheme (the classic style and the Qt Quick theme follow the same rule) ----

    /// Returns whether the UI follows the color scheme reported by the operating system. Always the case on Linux and
    /// macOS, where the frontends have never had a choice; elsewhere it is a user decision that defaults to off.
    bool followsSystemColorScheme() const;

    /// Sets whether the UI should follow the operating system's color scheme. Ignored on Linux and macOS, where the
    /// setting does not exist in the user interface either.
    void setFollowsSystemColorScheme(bool enable);

    /// Returns whether the UI is currently dark, which is the case when the system color scheme is followed *and* that
    /// scheme is dark. Both frontends take their palette from this one rule.
    bool usingDarkTheme() const;

    // ---- Main window of the classic frontend ----
    // Both values are opaque blobs written by QWidget::saveGeometry()/saveState(). A geometry blob also carries the
    // screen the window was on and whether it was maximized, so they are stored exactly as the classic frontend always
    // stored them and must not be "simplified" into plain rectangles.

    /// Returns the remembered main window geometry, or an empty blob if none was stored yet.
    QByteArray mainWindowGeometry() const;

    /// Stores the main window geometry.
    void setMainWindowGeometry(const QByteArray& geometry);

    /// Returns the remembered layout of the docked widgets, or an empty blob if none was stored yet.
    QByteArray mainWindowState() const;

    /// Stores the layout of the docked widgets.
    void setMainWindowState(const QByteArray& state);

    // ---- Window of a frontend that stores a plain rectangle (the Qt Quick shell) ----

    /// Returns the remembered window rectangle, or an invalid rectangle if none was stored yet. The Qt Quick shell
    /// cannot use the blob above: it does not have a QWidget whose geometry it could save.
    QRect workbenchWindowGeometry() const;

    /// Stores the window rectangle.
    void setWorkbenchWindowGeometry(const QRect& geometry);

    /// Returns whether the window was maximized when the frontend last closed it.
    bool isWorkbenchWindowMaximized() const;

    /// Stores whether the window is maximized.
    void setWorkbenchWindowMaximized(bool maximized);

    // ---- File dialogs ----

    /// Returns whether each kind of file dialog keeps its own history of recently used directories.
    bool keepDirectoryHistory() const;

    /// Sets whether each kind of file dialog keeps its own history of recently used directories.
    void setKeepDirectoryHistory(bool enable);

    /// Returns whether the user prefers Qt's widget-based file dialog over the platform's native one.
    bool preferQtFileDialog() const;

    /// Sets whether the user prefers Qt's widget-based file dialog over the platform's native one.
    void setPreferQtFileDialog(bool prefer);

    /// Returns the directories the given kind of file dialog was used in last. \a dialogClass identifies the kind of file
    /// dialog, for example `import` or `scene`.
    QStringList recentDirectories(const QString& dialogClass) const;

    /// Moves \a directory to the front of the directory history of the given kind of file dialog and persists the
    /// history immediately, so that a frontend which is terminated without a clean shutdown does not lose it.
    void rememberDirectory(const QString& dialogClass, const QString& directory);

    /// Returns how several files of the same kind should be imported. Only the professional edition lets the user choose.
    FileImporter::MultiFileImportMode multiFileImportMode() const;

    /// Sets how several files of the same kind should be imported.
    void setMultiFileImportMode(FileImporter::MultiFileImportMode mode);

    /// Returns the directory a session file was last saved to.
    QString sessionFileDirectory() const;

    /// Stores the directory a session file was saved to.
    void setSessionFileDirectory(const QString& directory);

    // ---- First start ----

    /// Returns whether the user has already confirmed the GPU adapter selection that the classic frontend shows on its
    /// first start. In the settings file this key lives in a `[viewport]` section, which is what the key path maps to.
    bool graphicsAdapterSetupDone() const;

    /// Sets whether the GPU adapter selection has been confirmed.
    void setGraphicsAdapterSetupDone(bool done);

Q_SIGNALS:

    /// Emitted after one of the values above has changed, and when the operating system reports another color scheme.
    /// A single signal is enough: a frontend re-reads what it displays instead of subscribing to individual keys.
    void changed();

private:

    /// Private constructor — use instance() to access.
    GuiSettings();

    /// Declared and defined out of line so that the DLL export of the class covers it on MSVC as well.
    ~GuiSettings() override;

    /// Subscribes to the color scheme of the platform as soon as there is an application object to ask. The instance can
    /// be created before that, because the classic frontend asks for the color scheme while it sets up the platform
    /// integration.
    void observeSystemColorScheme();

    /// Is called when the operating system reports a different color scheme.
    void systemColorSchemeChanged();

    /// Whether the icon theme of the process matches the current color scheme. The icons of both frontends come from
    /// the same pair of icon themes (IconTheme), and this is the one place that decides which of the two is current.
    void applyIconTheme();

    /// Whether the color scheme of the platform is watched for changes already.
    bool _observingColorScheme = false;

    Q_DISABLE_COPY_MOVE(GuiSettings)
};

}   // End of namespace
