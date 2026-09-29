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
QUrl QmlIcons::url(const QString& iconPath, const QString& themeName) const
{
    if(iconPath.isEmpty())
        return {};

    // The icon path travels as the id of the image provider request and is therefore percent-encoded; the provider
    // resolves it through the same rule the desktop frontend applies to the icons of its QActions (IconTheme::icon).
    // The theme is a leading segment of the id and does not change how an icon is rendered - it keeps the URLs of the
    // two themes apart, so that an Image refreshes its pixmap when the color scheme changes.
    const QByteArray encoded = QUrl::toPercentEncoding(iconPath);
    return QUrl(QStringLiteral("image://%1/%2/%3").arg(providerName(), themeName, QString::fromLatin1(encoded)));
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
    // The first segment of the id is the theme the icon was requested for; the icon is resolved with the theme the
    // process uses (IconTheme), so the segment only separates the caches of the two color schemes.
    const qsizetype separator = id.indexOf(u'/');
    const QString iconPath = QUrl::fromPercentEncoding((separator < 0 ? id : id.mid(separator + 1)).toLatin1());

    // The item asks for the size it displays the icon with, and Qt Quick scales what comes back to it; the icon is
    // rasterized at the scale factor of the screen, so that it stays crisp on a high resolution display. The factor is
    // the one of the process: a workbench window on a screen with a different scale factor is a multi-screen setup this
    // frontend does not distinguish yet.
    const qreal devicePixelRatio = qApp ? qApp->devicePixelRatio() : qreal(1);
    const QImage image = IconTheme::image(iconPath, requestedSize, devicePixelRatio);

    if(size)
        *size = image.size();
    return image;
}

}   // End of namespace
