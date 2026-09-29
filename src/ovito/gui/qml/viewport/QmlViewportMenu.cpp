// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT


#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/gui/qml/mainwin/QmlMainWindowUI.h>
#include <ovito/gui/qml/viewport/QuickViewportItem.h>
#include <ovito/gui/qml/viewport/QuickViewportWindow.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/gui/base/app/GuiTaskScope.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/viewport/ViewportSettings.h>
#include "QmlViewportMenu.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
QmlViewportMenu::QmlViewportMenu(QmlMainWindowUI& ui, QObject* parent) : QObject(parent), _ui(ui)
{
    // The menu offers to maximize or restore the viewport it was opened for, so its state follows the viewport layout.
    connect(&_ui.datasetContainer(), &DataSetContainer::maximizedViewportChanged, this, &QmlViewportMenu::changed);
}

/******************************************************************************
* Returns the viewport the menu acts on.
******************************************************************************/
Viewport* QmlViewportMenu::viewport() const
{
    // Locking the weak reference keeps the viewport alive for the duration of the call; the viewport itself is owned by
    // the viewport configuration of the current data set.
    return _viewport.lock().get();
}

/******************************************************************************
* Returns the view type of the viewport the menu acts on.
******************************************************************************/
int QmlViewportMenu::viewType() const
{
    if(Viewport* vp = viewport())
        return static_cast<int>(vp->viewType());
    return 0;
}

/******************************************************************************
* Returns the view types the submenu offers.
******************************************************************************/
QVariantList QmlViewportMenu::viewTypes() const
{
    // The same list, in the same order, as the classic viewport menu offers it (ViewportMenu::ViewportMenu).
    static const std::pair<Viewport::ViewType, const char*> types[] = {
        { Viewport::VIEW_TOP, QT_TR_NOOP("Top") },
        { Viewport::VIEW_BOTTOM, QT_TR_NOOP("Bottom") },
        { Viewport::VIEW_FRONT, QT_TR_NOOP("Front") },
        { Viewport::VIEW_BACK, QT_TR_NOOP("Back") },
        { Viewport::VIEW_LEFT, QT_TR_NOOP("Left") },
        { Viewport::VIEW_RIGHT, QT_TR_NOOP("Right") },
        { Viewport::VIEW_ORTHO, QT_TR_NOOP("Ortho") },
        { Viewport::VIEW_PERSPECTIVE, QT_TR_NOOP("Perspective") }
    };

    QVariantList result;
    result.reserve(std::size(types));
    for(const auto& [type, text] : types)
        result.push_back(QVariantMap{ { QStringLiteral("value"), static_cast<int>(type) }, { QStringLiteral("text"), tr(text) } });
    return result;
}

/******************************************************************************
* Returns whether the viewport shows the construction grid.
******************************************************************************/
bool QmlViewportMenu::gridVisible() const
{
    if(Viewport* vp = viewport())
        return vp->isGridVisible();
    return false;
}

/******************************************************************************
* Returns whether the camera rotation is constrained to the orbit plane.
******************************************************************************/
bool QmlViewportMenu::constrainRotation() const
{
    return ViewportSettings::getSettings().constrainCameraRotation();
}

/******************************************************************************
* Returns whether the viewport the menu acts on is the maximized one.
******************************************************************************/
bool QmlViewportMenu::isMaximized() const
{
    if(Viewport* vp = viewport()) {
        if(ViewportConfiguration* config = _ui.datasetContainer().activeViewportConfig())
            return config->maximizedViewport() == vp;
    }
    return false;
}

/******************************************************************************
* Opens the menu for the viewport of the given viewport item.
******************************************************************************/
void QmlViewportMenu::openFor(QuickViewportItem* item, const QPointF& pos)
{
    OVITO_ASSERT(item);
    if(!item || !item->viewportWindow())
        return;

    _item = item;
    _viewport = item->viewportWindow()->viewport();
    // The state the menu displays is read when it is opened, so tell the scene that it can now pop the menu up. The
    // state itself is reported by the individual properties, which the scene binds its entries to.
    Q_EMIT changed();
    Q_EMIT opened(item, pos.x(), pos.y());
}

/******************************************************************************
* Switches the viewport to the given view type.
******************************************************************************/
void QmlViewportMenu::setViewType(int viewType)
{
    if(Viewport* vp = viewport()) {
        // The viewport is edited from a QML callback, i.e. from an event handler that has no task context of its own.
        GuiTaskScope taskScope(_ui);
        vp->setViewType(static_cast<Viewport::ViewType>(viewType), true, false);

        // Remember which view type the maximized viewport has, so that the next start opens with it (this is what the
        // classic viewport menu does as well).
        if(ViewportConfiguration* config = _ui.datasetContainer().activeViewportConfig()) {
            if(config->maximizedViewport() == vp) {
                ViewportSettings::getSettings().setDefaultMaximizedViewportType(vp->viewType());
                ViewportSettings::getSettings().save();
            }
        }
        Q_EMIT changed();
    }
}

/******************************************************************************
* Shows or hides the construction grid of the viewport.
******************************************************************************/
void QmlViewportMenu::setGridVisible(bool visible)
{
    if(Viewport* vp = viewport()) {
        if(vp->isGridVisible() != visible) {
            GuiTaskScope taskScope(_ui);
            vp->setGridVisible(visible);
            Q_EMIT changed();
        }
    }
}

/******************************************************************************
* Constrains camera rotations to the orbit plane or releases them.
******************************************************************************/
void QmlViewportMenu::setConstrainRotation(bool constrain)
{
    ViewportSettings& settings = ViewportSettings::getSettings();
    if(settings.constrainCameraRotation() != constrain) {
        settings.setConstrainCameraRotation(constrain);
        settings.save();
        Q_EMIT changed();
    }
}

/******************************************************************************
* Maximizes the viewport or restores it.
******************************************************************************/
void QmlViewportMenu::toggleMaximize()
{
    // Maximizing is a command of the shared command layer, so both frontends have exactly one implementation of it. It
    // acts on the active viewport, which is the viewport the menu was opened for: clicking the caption of a viewport
    // makes it the active one (BaseViewportWindow::mousePressEvent).
    if(ActionManager* manager = _ui.actionManager())
        manager->triggerCommand(ACTION_VIEWPORT_MAXIMIZE);
    Q_EMIT changed();
}

/******************************************************************************
* Tells the model that its menu is no longer displayed.
******************************************************************************/
void QmlViewportMenu::close()
{
    if(_item) {
        _item.clear();
        _viewport.reset();
        Q_EMIT changed();
    }
}

}   // End of namespace
