// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/qml/QmlFrontend.h>

namespace Ovito {

/**
 * \brief Exposes the C++ side of the Qt Quick frontend to the QML scene.
 *
 * This class is registered as the "viewportController" context property of the QML engine. It creates and owns the
 * viewport items that the QML layout asks for. The state of the window itself - the status line, the task progress and
 * the dialogs - belongs to QmlWorkbenchController, and the commands of the workbench (including Undo and Redo, which
 * act on the shared undo stack) come from the command layer that is exposed as "commandManager".
 *
 * The viewports are those of the current dataset. Whenever that dataset is replaced, the controller discards its
 * viewport items and emits viewportConfigurationChanged(), which tells the QML scene to ask for viewport items again -
 * a viewport item belongs to the viewport of one dataset and cannot be reused for the next one.
 */
class OVITO_GUIQML_EXPORT QmlViewportController : public QObject
{
    Q_OBJECT

    /// The number of viewports of the current dataset, i.e. the number of viewport items the QML scene should have.
    Q_PROPERTY(int viewportCount READ viewportCount NOTIFY viewportConfigurationChanged)

public:

    /// Constructor.
    explicit QmlViewportController(QmlMainWindowUI& ui, QObject* parent = nullptr);

    /// Returns the number of viewports of the current dataset.
    int viewportCount() const;

    /// Creates the viewport item for the viewport with the given index of the current dataset and adds it to the QML scene.
    /// Calling this again for the same index replaces the viewport item that was created before.
    /// \return The new viewport item, or null if the current dataset has no viewport with that index.
    Q_INVOKABLE QQuickItem* createViewportItem(QQuickItem* parentItem, int viewportIndex);

    /// Returns the viewport item that has been created for the viewport with the given index, or null.
    QuickViewportItem* viewportItem(int viewportIndex) const;

    /// Gives the input focus to the viewport item displaying the given viewport.
    void setViewportInputFocus(Viewport* viewport);

Q_SIGNALS:

    /// Is emitted when the current dataset or its set of viewports has changed.
    void viewportConfigurationChanged();

private:

    /// Discards the viewport items of the previous dataset.
    void discardViewportItems();

private:

    /// The user interface this controller belongs to.
    QmlMainWindowUI& _ui;

    /// The viewport items that have been created for the viewports of the current dataset, indexed by viewport.
    std::vector<QPointer<QuickViewportItem>> _viewportItems;

    /// Gives every viewport item the renderer the user selected for the interactive viewports.
    void applyInteractiveRenderer();

    /// Returns the renderer that provides the settings of the interactive viewports (see ViewportRendererRegistry).
    static OORef<SceneRenderer> interactiveRenderer();

    /// Handles a fatal error reported by a viewport window: the workbench falls back to the default renderer.
    void viewportWindowFatalError(const Exception& ex);

    /// The renderer object that provides the rendering settings of the interactive viewports.
    OORef<SceneRenderer> _interactiveRenderer;

    /// Indicates that a fatal rendering error has been handled; further errors are ignored to avoid a retry loop.
    bool _fatalRenderErrorHandled = false;
};

}   // End of namespace
