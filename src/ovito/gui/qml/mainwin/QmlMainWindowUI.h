// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/gui/qml/mainwin/QmlViewportController.h>
#include <ovito/gui/qml/mainwin/QmlViewportLayout.h>
#include <ovito/gui/base/app/WorkbenchUI.h>

namespace Ovito {

/**
 * \brief The user interface implementation of the Qt Quick / QML frontend.
 *
 * Each instance owns one Qt Quick window showing the workbench UI. This class plays the role that MainWindowUI plays
 * for the classic QtWidgets frontend; the parts of the user interface contract that do not depend on the presentation
 * are provided by the WorkbenchUI base class (see gui/base).
 *
 * Note that UserInterface is not a QObject, so the parts of this class that need to be reachable from QML are provided
 * by the separate QmlViewportController object.
 */
class OVITO_GUIQML_EXPORT QmlMainWindowUI : public WorkbenchUI
{
    OVITO_CLASS(QmlMainWindowUI)

public:

    /// Constructor.
    QmlMainWindowUI();

    /// Destructor.
    ~QmlMainWindowUI();

    /// Creates the Qt Quick window and loads the workbench UI.
    void initializeWindow();

    /// Returns the Qt Quick window displaying the workbench.
    QQuickView* view() const { return _view; }

    /// Returns the object that exposes the frontend to the QML scene.
    QmlViewportController* qmlController() const { return _qmlController; }

    /// Returns the model that lays the viewport panes of the workbench out.
    QmlViewportLayout* viewportLayout() const { return _viewportLayout; }

    /// Returns the status message currently displayed in the workbench window.
    const QString& statusMessage() const { return _statusMessage; }

    /// Displays a message string in the window's status bar.
    void showStatusBarMessage(const QString& message, int timeout = 0) override;

    /// Hides any messages currently displayed in the status bar.
    void clearStatusBarMessage() override;

    /// Gives the active viewport the input focus.
    void setViewportInputFocus() override;

    /// Cancels all running tasks associated with this user interface and closes the user interface.
    bool shutdown() override;

    /// Displays a message box to the user.
    MessageBoxButton showMessageBox(MessageBoxIcon icon, const QString& title, const QString& text, int buttons, MessageBoxButton defaultButton = NoButton, const QString& detailedText = {}) override;

protected:

    // Implementation of the presentation hooks of WorkbenchUI (see gui/base/app/WorkbenchUI.h).

    /// Creates the action manager of this workbench.
    virtual ActionManager* createActionManager(QObject* parent) override;

    /// Shows the error message in the status line of the workbench window.
    /// The message has already been written to the terminal by WorkbenchUI::reportError().
    virtual void displayErrorMessage(const Exception& ex, bool blocking) override;

private:

    /// Creates the default dataset if no dataset has been loaded yet.
    void initializeDataset();

    /// The Qt Quick window displaying the workbench UI.
    QPointer<QQuickView> _view;

    /// The object exposing this frontend to the QML scene.
    QPointer<QmlViewportController> _qmlController;

    /// The layout of the viewport panes displayed by this window.
    QPointer<QmlViewportLayout> _viewportLayout;

    /// The message currently shown in the status line.
    QString _statusMessage;
};

}   // End of namespace
