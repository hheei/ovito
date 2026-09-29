// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/gui/qml/QmlFrontend.h>
#include <QQuickImageProvider>
#include <QUrl>

class QQmlEngine;

namespace Ovito {

/**
 * \brief Presents the icon set of the shared GUI layer to QML.
 *
 * The icons of OVITO are Qt resources that come in a pair of icon themes (see IconTheme in the shared GUI layer), which
 * the desktop frontend reaches through QIcon. QML needs a URL it can put into an Image, so this class publishes an image
 * provider: `Icons.url("viewport_maximize")` returns the source URL of that icon, and the provider behind it renders the
 * icon at the size and the scale factor the QML item asks for.
 *
 * The type is registered as a QML singleton instance (`Icons` of the `Ovito.Qml` import), so every component of the shell
 * can use it without having it passed down.
 */
class OVITO_GUIQML_EXPORT QmlIcons : public QObject
{
    Q_OBJECT

    /// The name of the icon theme the icons are taken from, i.e. which of the two color schemes is current.
    Q_PROPERTY(QString themeName READ themeName NOTIFY changed)

public:

    /// Returns the icon set of the process. It is registered as a QML singleton, so there is one instance for all
    /// workbench windows of the application.
    static QmlIcons& instance();

    /// Constructor.
    explicit QmlIcons(QObject* parent = nullptr);

    /// Registers the image provider of this class with a QML engine.
    static void registerImageProvider(QQmlEngine& engine);

    /// Returns the source URL of an icon of a command or of the icon set. The theme the icon belongs to is part of the
    /// URL, because QML caches an image by its URL: an Image therefore has to pass Icons.themeName on, which also makes
    /// its binding depend on the theme, so that the shell's icons follow a change of the color scheme at runtime.
    Q_INVOKABLE QUrl url(const QString& iconPath, const QString& themeName) const;

    /// Returns the name of the icon theme the icons are taken from.
    QString themeName() const;

Q_SIGNALS:

    /// Is emitted when the color scheme changed, i.e. when the icons of the other theme have to be taken.
    void changed();

private:

    /// The name of the image provider this class publishes its URLs under.
    static const QString& providerName();
};

/**
 * \brief The image provider behind QmlIcons: it renders an icon of the shared icon set into an image.
 */
class QmlIconImageProvider : public QQuickImageProvider
{
public:

    /// Constructor.
    QmlIconImageProvider();

    /// Renders the requested icon. The id of the request is the icon path of a command, or the name of an icon.
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
};

}   // End of namespace
