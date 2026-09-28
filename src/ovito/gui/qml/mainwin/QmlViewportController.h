// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/qml/QmlFrontend.h>

namespace Ovito {

/**
 * \brief Exposes the C++ side of the Qt Quick frontend to the QML scene.
 *
 * This class is registered as the "viewportController" context property of the QML engine. It creates the
 * viewport items requested by the QML layout and carries the state that the QML scene needs to display,
 * e.g. the status line message.
 */
class OVITO_GUIQML_EXPORT QmlViewportController : public QObject
{
    Q_OBJECT

    /// The message displayed in the status line of the workbench window.
    Q_PROPERTY(QString statusMessage READ statusMessage WRITE setStatusMessage NOTIFY statusMessageChanged)

public:

    /// Constructor.
    explicit QmlViewportController(QmlMainWindowUI& ui, QObject* parent = nullptr);

    /// Returns the status message displayed in the status line.
    QString statusMessage() const;

    /// Sets the status message displayed in the status line.
    void setStatusMessage(const QString& message);

    /// Creates a viewport item for the next viewport of the current dataset and adds it to the QML scene.
    Q_INVOKABLE QQuickItem* createViewportItem(QQuickItem* parentItem);

    /// Gives the input focus to the viewport item displaying the given viewport.
    void setViewportInputFocus(Viewport* viewport);

Q_SIGNALS:

    /// Is emitted when the status message has changed.
    void statusMessageChanged();

private:

    /// The user interface this controller belongs to.
    QmlMainWindowUI& _ui;

    /// The viewport items that have been created for the current dataset.
    std::vector<QPointer<QuickViewportItem>> _viewportItems;

    /// The renderer object that provides the rendering settings of the interactive viewports.
    OORef<SceneRenderer> _interactiveRenderer;

    /// The index of the next viewport to create an item for.
    int _nextViewportIndex = 0;
};

}   // End of namespace
