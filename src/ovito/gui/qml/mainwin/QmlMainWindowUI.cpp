// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/gui/qml/mainwin/QmlViewportController.h>
#include <ovito/gui/qml/mainwin/QmlViewportLayout.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <QtQml/qqml.h>
#include "QmlMainWindowUI.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(QmlMainWindowUI);

/******************************************************************************
* Registers the C++ classes of the workbench shell with the QML type system.
******************************************************************************/
static void registerQmlTypes()
{
    // The classes are not creatable from QML: the workbench shell gets its panes, handles and controllers from the
    // frontend. Registering them nevertheless makes the properties of the shell's QML components statically typed, so
    // that a typo in a property name is an error instead of a silently broken binding.
    static const bool registered = []() {
        qmlRegisterUncreatableType<QmlViewportPane>("Ovito.Qml", 1, 0, "ViewportPane",
            QStringLiteral("Panes are created by the viewport layout model."));
        qmlRegisterUncreatableType<QmlViewportSplitter>("Ovito.Qml", 1, 0, "ViewportSplitter",
            QStringLiteral("Handles are created by the viewport layout model."));
        qmlRegisterUncreatableType<QmlViewportLayout>("Ovito.Qml", 1, 0, "ViewportLayout",
            QStringLiteral("There is one viewport layout per workbench window."));
        qmlRegisterUncreatableType<QmlViewportController>("Ovito.Qml", 1, 0, "ViewportController",
            QStringLiteral("There is one viewport controller per workbench window."));
        return true;
    }();
    Q_UNUSED(registered);
}

/******************************************************************************
* Constructor.
******************************************************************************/
QmlMainWindowUI::QmlMainWindowUI()
{
}

/******************************************************************************
* Destructor.
******************************************************************************/
QmlMainWindowUI::~QmlMainWindowUI()
{
    // The controller is a child object of the window, so deleting the window deletes it as well.
    delete _view;
}

/******************************************************************************
* Creates the Qt Quick window and loads the workbench UI.
******************************************************************************/
void QmlMainWindowUI::initializeWindow()
{
    OVITO_ASSERT(!_view);

    // Create the window that displays the workbench UI.
    auto* view = new QQuickView();
    view->setTitle(tr("OVITO"));
    view->setResizeMode(QQuickView::SizeRootObjectToView);
    view->setColor(QColor(24, 24, 24));

    // Create the input manager, the undo stack and the action manager. They are frontend-neutral and come from the
    // gui/base module; the window owns them because it is a QObject.
    initializeWorkbench(view);

    // Create the objects that the QML scene talks to: the controller that owns the viewport items, and the model that
    // lays the panes of the viewport layout out.
    _qmlController = new QmlViewportController(*this, view);
    _viewportLayout = new QmlViewportLayout(*this, view);

    // Make the C++ side of the frontend available to QML. The types have to be registered before the QML file that uses
    // them is loaded.
    registerQmlTypes();
    view->rootContext()->setContextProperty(QStringLiteral("viewportController"), _qmlController);
    view->rootContext()->setContextProperty(QStringLiteral("viewportLayout"), _viewportLayout);

    // Create a default dataset if no dataset has been loaded yet, so that the workbench has viewports to display.
    initializeDataset();

    // Load the workbench UI.
    view->setSource(QUrl(QStringLiteral("qrc:/ovito/gui/qml/WorkbenchWindow.qml")));
    if(view->status() == QQuickView::Error) {
        for(const QQmlError& error : view->errors())
            qWarning() << "QML error:" << error.toString();
        delete view;
        throw Exception(tr("Failed to load the QML workbench user interface."));
    }

    view->resize(1280, 800);
    view->show();

    _view = view;
}

/******************************************************************************
* Creates the action manager of this workbench.
******************************************************************************/
ActionManager* QmlMainWindowUI::createActionManager(QObject* parent)
{
    return new ActionManager(parent, *this);
}

/******************************************************************************
* Creates the default dataset if no dataset has been loaded yet.
******************************************************************************/
void QmlMainWindowUI::initializeDataset()
{
    if(datasetContainer().currentSet() == nullptr)
        datasetContainer().setCurrentSet(OORef<DataSet>::create());
}

/******************************************************************************
* Displays a message string in the window's status bar.
******************************************************************************/
void QmlMainWindowUI::showStatusBarMessage(const QString& message, int timeout)
{
    Q_UNUSED(timeout);
    if(_statusMessage != message) {
        _statusMessage = message;
        if(_qmlController)
            Q_EMIT _qmlController->statusMessageChanged();
    }
}

/******************************************************************************
* Hides any messages currently displayed in the status bar.
******************************************************************************/
void QmlMainWindowUI::clearStatusBarMessage()
{
    showStatusBarMessage({});
}

/******************************************************************************
* Gives the active viewport the input focus.
******************************************************************************/
void QmlMainWindowUI::setViewportInputFocus()
{
    if(_qmlController) {
        if(ViewportConfiguration* viewportConfig = datasetContainer().activeViewportConfig())
            _qmlController->setViewportInputFocus(viewportConfig->activeViewport());
    }
}

/******************************************************************************
* Cancels all running tasks associated with this user interface and closes the user interface.
******************************************************************************/
bool QmlMainWindowUI::shutdown()
{
    if(_view)
        _view->hide();
    return UserInterface::shutdown();
}

/******************************************************************************
* Shows the error message in the status line of the workbench window.
******************************************************************************/
void QmlMainWindowUI::displayErrorMessage(const Exception& ex, bool blocking)
{
    Q_UNUSED(blocking);

    // The prototype has no error dialog yet, so the first line of the message is shown in the status line.
    showStatusBarMessage(ex.message().split(QLatin1Char('\n')).first());
}

/******************************************************************************
* Displays a message box to the user.
******************************************************************************/
UserInterface::MessageBoxButton QmlMainWindowUI::showMessageBox(MessageBoxIcon icon, const QString& title, const QString& text, int buttons, MessageBoxButton defaultButton, const QString& detailedText)
{
    Q_UNUSED(icon);
    Q_UNUSED(buttons);
    Q_UNUSED(detailedText);

    // The prototype has no message box yet. Log the message instead of blocking the application.
    qWarning().noquote() << title << ":" << text;
    return defaultButton;
}

}   // End of namespace
