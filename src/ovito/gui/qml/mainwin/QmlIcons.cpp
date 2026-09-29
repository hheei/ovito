// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include "QmlIcons.h"

#include <ovito/gui/base/app/GuiSettings.h>
#include <ovito/gui/base/app/IconTheme.h>

#include <QQmlEngine>

namespace Ovito {

/******************************************************************************
* Returns the icon set of the process.
******************************************************************************/
QmlIcons& QmlIcons::instance()
{
    static QmlIcons icons;
    return icons;
}

/******************************************************************************
* Constructor.
******************************************************************************/
QmlIcons::QmlIcons(QObject* parent) : QObject(parent)
{
    // The icons come from the icon theme that matches the current color scheme; when the user interface switches between
    // light and dark, the items that display an icon have to ask for it again.
    connect(&GuiSettings::instance(), &GuiSettings::changed, this, &QmlIcons::changed);
}

/******************************************************************************
* Registers the image provider of this class with a QML engine.
******************************************************************************/
void QmlIcons::registerImageProvider(QQmlEngine& engine)
{
    // The engine takes ownership of the provider.
    engine.addImageProvider(providerName(), new QmlIconImageProvider());
}

/******************************************************************************
* Returns the source URL of an icon of a command or of the icon set.
******************************************************************************/
QUrl QmlIcons::url(const QString& iconPath) const
{
    if(iconPath.isEmpty())
        return {};

    // The icon path travels as the id of the image provider request and is therefore percent-encoded; the provider
    // resolves it through the same rule the desktop frontend applies to the icons of its QActions (IconTheme::icon).
    const QByteArray encoded = QUrl::toPercentEncoding(iconPath);
    return QUrl(QStringLiteral("image://%1/%2").arg(providerName(), QString::fromLatin1(encoded)));
}

/******************************************************************************
* Returns the name of the icon theme the icons are taken from.
******************************************************************************/
QString QmlIcons::themeName() const
{
    return IconTheme::currentThemeName();
}

/******************************************************************************
* The name of the image provider this class publishes its URLs under.
******************************************************************************/
const QString& QmlIcons::providerName()
{
    static const QString name = QStringLiteral("ovito-icon");
    return name;
}

/******************************************************************************
* Constructor.
******************************************************************************/
QmlIconImageProvider::QmlIconImageProvider() : QQuickImageProvider(QQuickImageProvider::Image)
{
}

/******************************************************************************
* Renders the requested icon.
******************************************************************************/
QImage QmlIconImageProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize)
{
    const QString iconPath = QUrl::fromPercentEncoding(id.toLatin1());

    // The item asks for the size it displays the icon with, and Qt Quick scales what comes back to it; the icon is
    // rasterized at the scale factor of the screen, so that it stays crisp on a high resolution display.
    const qreal devicePixelRatio = qApp ? qApp->devicePixelRatio() : qreal(1);
    const QImage image = IconTheme::image(iconPath, requestedSize, devicePixelRatio);

    if(size)
        *size = image.size();
    return image;
}

}   // End of namespace
