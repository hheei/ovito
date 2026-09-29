// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/base/GUIBase.h>

namespace Ovito {

/**
 * \brief A user interface command: the frontend-neutral description of an operation the user can invoke.
 *
 * A command owns everything that is the same in every frontend: its identifier, its title, its icon,
 * its shortcut, whether it is currently enabled, checked and visible, and the code that runs when it
 * is invoked. A frontend only presents the command and calls trigger():
 *
 *  - The classic QtWidgets workbench presents it through the QAction that ActionManager keeps
 *    synchronized with the command (menus, toolbars, buttons).
 *  - The Qt Quick workbench presents it through the Command objects exposed to QML (menu entries,
 *    buttons, keyboard shortcuts).
 *
 * Because the command is the single owner of the state, all state changes go through this class.
 * Writing to the QAction view directly (for example `actionManager()->getAction(id)->setEnabled(false)`)
 * is not supported: the next state change of the command overwrites the QAction again.
 *
 * A checkable command does not toggle itself when trigger() is called, because the control that the
 * user operates (a QAction, a QML button) already flips the check state and reports it back through
 * setChecked(). Input-mode commands are the exception; see ViewportModeCommand.
 */
class OVITO_GUIBASE_EXPORT Command : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString text READ text NOTIFY changed)
    Q_PROPERTY(QString toolTip READ toolTip NOTIFY changed)
    Q_PROPERTY(QString statusTip READ statusTip NOTIFY changed)
    Q_PROPERTY(QString iconPath READ iconPath NOTIFY changed)
    Q_PROPERTY(QKeySequence shortcut READ shortcut NOTIFY changed)
    Q_PROPERTY(QColor highlightColor READ highlightColor NOTIFY changed)
    Q_PROPERTY(bool checkable READ isCheckable NOTIFY changed)
    Q_PROPERTY(bool checked READ isChecked WRITE setChecked NOTIFY changed)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY changed)

public:

    /// Constructor. The command is registered with a frontend through ActionManager::addCommand().
    Command(const QString& id,
            const QString& text,
            const QString& iconPath = {},
            const QString& statusTip = {},
            const QKeySequence& shortcut = {},
            QObject* parent = nullptr);

    /// Returns the unique identifier of this command.
    const QString& id() const { return _id; }

    /// Returns the title of this command as it is presented to the user.
    const QString& text() const { return _text; }

    /// Returns the tool tip of this command, which is its title plus its shortcut.
    QString toolTip() const;

    /// Returns the status-bar text of this command.
    const QString& statusTip() const { return _statusTip; }

    /// Returns the icon path of this command (a theme icon name or a resource path, may be empty).
    const QString& iconPath() const { return _iconPath; }

    /// Returns the default shortcut of this command.
    const QKeySequence& shortcut() const { return _shortcut; }

    /// Returns the accent color that a frontend may use to highlight the control of this command.
    const QColor& highlightColor() const { return _highlightColor; }

    /// Indicates whether this command has a check state.
    bool isCheckable() const { return _checkable; }

    /// Returns the check state of this command.
    bool isChecked() const { return _checked; }

    /// Indicates whether this command can currently be invoked.
    bool isEnabled() const { return _enabled; }

    /// Indicates whether this command should be presented to the user at all.
    bool isVisible() const { return _visible; }

    /// Sets the title of this command.
    void setText(const QString& text);

    /// \brief Overrides the tool tip of this command. An empty string restores the automatic tool tip.
    void setToolTip(const QString& toolTip);

    /// Sets the status-bar text of this command.
    void setStatusTip(const QString& statusTip);

    /// Sets the icon path of this command.
    void setIconPath(const QString& iconPath);

    /// Sets the default shortcut of this command.
    void setShortcut(const QKeySequence& shortcut);

    /// Sets the accent color of the control that presents this command.
    void setHighlightColor(const QColor& color);

    /// Turns this command into a checkable one.
    void setCheckable(bool checkable);

    /// Sets the check state of this command.
    void setChecked(bool checked);

    /// Enables or disables this command.
    void setEnabled(bool enabled);

    /// Controls whether this command is presented to the user.
    void setVisible(bool visible);

    /// \brief Invokes this command, i.e. runs the operation the user has asked for.
    /// Does nothing while the command is disabled.
    virtual void trigger();

Q_SIGNALS:

    /// This signal is emitted after the command has been invoked.
    void triggered();

    /// This signal is emitted when the check state of this command changed.
    void toggled(bool checked);

    /// This signal is emitted whenever one of the properties of this command changed.
    void changed();

private:

    /// The unique identifier of this command.
    QString _id;

    /// The title presented to the user.
    QString _text;

    /// An explicit tool tip of this command, or an empty string to use the title plus shortcut.
    QString _toolTip;

    /// The status-bar text of this command.
    QString _statusTip;

    /// The icon of this command.
    QString _iconPath;

    /// The default shortcut of this command.
    QKeySequence _shortcut;

    /// Optional accent color for the control that presents this command.
    QColor _highlightColor;

    /// Whether this command has a check state and the current check state.
    bool _checkable = false;
    bool _checked = false;

    /// Whether this command can currently be invoked and whether it is presented to the user.
    bool _enabled = true;
    bool _visible = true;
};

/**
 * \brief A command that activates a viewport input mode (zoom, pan, orbit, selection, ...).
 *
 * The input mode is the source of truth for the check state of this command: the command is checked
 * while its input mode is active. Invoking the command pushes the input mode onto the input manager,
 * or pops it again if it is already active - except for exclusive modes such as the selection mode,
 * which cannot be turned off by the user.
 */
class OVITO_GUIBASE_EXPORT ViewportModeCommand : public Command, public UserInterfaceComponent<UserInterface>
{
    Q_OBJECT

public:

    /// Constructor.
    ViewportModeCommand(UserInterface& ui,
                        const QString& id,
                        const QString& text,
                        OORef<ViewportInputMode> inputMode,
                        const QColor& highlightColor = QColor(),
                        const QString& iconPath = {},
                        const QString& statusTip = {},
                        const QKeySequence& shortcut = {},
                        QObject* parent = nullptr);

    /// Invokes the command by activating or deactivating the input mode.
    void trigger() override;

    /// \brief Activates the input mode, unless it is active already.
    Q_SLOT void activateMode() { setModeActive(true); }

    /// \brief Deactivates the input mode, unless it is an exclusive mode that must stay active.
    Q_SLOT void deactivateMode() { setModeActive(false); }

    /// Activates or deactivates the input mode.
    void setModeActive(bool active);

private:

    /// The input mode this command controls.
    OORef<ViewportInputMode> _inputMode;
};

}   // End of namespace
