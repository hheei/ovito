// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/qml/QmlFrontend.h>

namespace Ovito {

/**
 * \brief Exposes the C++ side of the Qt Quick frontend to the QML scene.
 *
 * This class is registered as the "viewportController" context property of the QML engine. It creates the viewport
 * items that the QML layout asks for and carries the state that the QML scene displays, e.g. the status line message.
 *
 * The viewports are those of the current dataset. Whenever that dataset is replaced, the controller discards its
 * viewport items and emits viewportConfigurationChanged(), which tells the QML scene to ask for viewport items again -
 * a viewport item belongs to the viewport of one dataset and cannot be reused for the next one.
 */
class OVITO_GUIQML_EXPORT QmlViewportController : public QObject
{
    Q_OBJECT

    /// The message displayed in the status line of the workbench window.
    Q_PROPERTY(QString statusMessage READ statusMessage WRITE setStatusMessage NOTIFY statusMessageChanged)

    /// The number of viewports of the current dataset, i.e. the number of viewport items the QML scene should have.
    Q_PROPERTY(int viewportCount READ viewportCount NOTIFY viewportConfigurationChanged)

    /// The label of the operation the Undo command would revert, or an empty string if there is nothing to undo.
    Q_PROPERTY(QString undoText READ undoText NOTIFY undoAvailableChanged)

    /// The label of the operation the Redo command would reapply.
    Q_PROPERTY(QString redoText READ redoText NOTIFY undoAvailableChanged)

    /// Whether there is an operation to undo.
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoAvailableChanged)

    /// Whether there is an operation to redo.
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoAvailableChanged)

public:

    /// Constructor.
    explicit QmlViewportController(QmlMainWindowUI& ui, QObject* parent = nullptr);

    /// Returns the status message displayed in the status line.
    QString statusMessage() const;

    /// Sets the status message displayed in the status line.
    void setStatusMessage(const QString& message);

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

    /// Reverts the last operation of the undo stack, e.g. the last change of the pane sizes.
    Q_INVOKABLE void undo();

    /// Reapplies the operation that was reverted last.
    Q_INVOKABLE void redo();

    /// Returns whether there is an operation to undo.
    bool canUndo() const;

    /// Returns whether there is an operation to redo.
    bool canRedo() const;

    /// Returns the label of the operation the Undo command would revert.
    QString undoText() const;

    /// Returns the label of the operation the Redo command would reapply.
    QString redoText() const;

Q_SIGNALS:

    /// Is emitted when the status message has changed.
    void statusMessageChanged();

    /// Is emitted when the current dataset or its set of viewports has changed.
    void viewportConfigurationChanged();

    /// Is emitted when the undo stack changed, i.e. when the Undo/Redo commands became (un)available or changed their label.
    void undoAvailableChanged();

private:

    /// Discards the viewport items of the previous dataset.
    void discardViewportItems();

private:

    /// The user interface this controller belongs to.
    QmlMainWindowUI& _ui;

    /// The viewport items that have been created for the viewports of the current dataset, indexed by viewport.
    std::vector<QPointer<QuickViewportItem>> _viewportItems;

    /// The renderer object that provides the rendering settings of the interactive viewports.
    OORef<SceneRenderer> _interactiveRenderer;
};

}   // End of namespace
