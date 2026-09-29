// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>

namespace Ovito {

class QmlMainWindowUI;
class QuickViewportItem;
class Viewport;

/**
 * \brief The context menu of a viewport, as the QML scene presents it.
 *
 * The classic frontend opens a QMenu when the user clicks the caption of a viewport
 * (`ViewportsPanel::onViewportMenuRequested`). The event itself needs no new plumbing here: the caption click already
 * travels through `BaseViewportWindow::mousePressEvent()` into `ViewportInputManager::requestContextMenu()`, which the
 * QML frontend now consumes (see QmlViewportController) - what was missing was only a surface that shows the menu.
 *
 * This object is that surface's model: it carries the state the menu displays (which view type is active, whether the
 * grid is shown, whether the camera rotation is constrained, whether the viewport is the maximized one) and performs the
 * two actions that write to shared state (the view type and the constrain-rotation flag are persisted like the classic
 * frontend persists them). The QML scene owns the presentation and decides how the entries look.
 *
 * The menu acts on the viewport of the viewport item it was opened for, which is also the viewport the caption click
 * activated - so entries that go through a shared command (Maximize) act on the active viewport, exactly as they do in
 * the classic frontend.
 */
class OVITO_GUIQML_EXPORT QmlViewportMenu : public QObject
{
    Q_OBJECT

    /// Whether the menu is currently displayed for a viewport.
    Q_PROPERTY(bool open READ isOpen NOTIFY changed)

    /// The viewport item the menu was opened for, i.e. the item the QML scene pops the menu up on.
    Q_PROPERTY(QQuickItem* item READ item NOTIFY changed)

    /// The view type of the viewport the menu acts on, as a Viewport::ViewType value.
    Q_PROPERTY(int viewType READ viewType NOTIFY changed)

    /// The view types the submenu offers, in the order the classic menu lists them, as { value, text } maps.
    Q_PROPERTY(QVariantList viewTypes READ viewTypes CONSTANT)

    /// Whether the viewport shows the construction grid.
    Q_PROPERTY(bool gridVisible READ gridVisible NOTIFY changed)

    /// Whether camera rotations are constrained to the orbit plane. This is a global viewport setting.
    Q_PROPERTY(bool constrainRotation READ constrainRotation NOTIFY changed)

    /// Whether the viewport the menu acts on is the maximized one.
    Q_PROPERTY(bool maximized READ isMaximized NOTIFY changed)

public:

    /// Constructor.
    explicit QmlViewportMenu(QmlMainWindowUI& ui, QObject* parent = nullptr);

    /// Returns whether the menu is currently displayed.
    bool isOpen() const { return _item != nullptr; }

    /// Returns the viewport item the menu was opened for, or null while the menu is closed.
    QQuickItem* item() const { return _item; }

    /// Returns the view type of the viewport the menu acts on.
    int viewType() const;

    /// Returns the view types the submenu offers.
    QVariantList viewTypes() const;

    /// Returns whether the viewport shows the construction grid.
    bool gridVisible() const;

    /// Returns whether the camera rotation is constrained to the orbit plane.
    bool constrainRotation() const;

    /// Returns whether the viewport the menu acts on is the maximized one.
    bool isMaximized() const;

    /// Opens the menu for the viewport of the given viewport item, at the given position in that item's coordinates.
    void openFor(QuickViewportItem* item, const QPointF& pos);

    /// Switches the viewport to the given view type, remembering it as the start-up view of the maximized viewport.
    Q_INVOKABLE void setViewType(int viewType);

    /// Shows or hides the construction grid of the viewport.
    Q_INVOKABLE void setGridVisible(bool visible);

    /// Constrains camera rotations to the orbit plane or releases them (a global, persisted viewport setting).
    Q_INVOKABLE void setConstrainRotation(bool constrain);

    /// Maximizes the viewport or restores it, through the shared Maximize command.
    Q_INVOKABLE void toggleMaximize();

    /// Tells the model that its menu is no longer displayed.
    Q_INVOKABLE void close();

Q_SIGNALS:

    /// Is emitted when the menu should be displayed, with the item to pop it up on and the position in its coordinates.
    void opened(QQuickItem* item, qreal x, qreal y);

    /// Is emitted when the state the menu displays has changed, and when the menu closes.
    void changed();

private:

    /// Returns the viewport the menu acts on, or null while the menu is closed.
    Viewport* viewport() const;

private:

    /// The user interface this menu belongs to.
    QmlMainWindowUI& _ui;

    /// The viewport item the menu was opened for, or null while the menu is closed.
    QPointer<QQuickItem> _item;

    /// The viewport the menu acts on. It is kept weakly: the menu must not keep a viewport of a replaced data set alive.
    OOWeakRef<Viewport> _viewport;
};

}   // End of namespace
