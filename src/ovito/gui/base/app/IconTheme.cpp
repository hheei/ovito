// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include "IconTheme.h"

#include <QGuiApplication>

namespace Ovito {

namespace {

// The names of the two icon themes that ship with the shared GUI layer. They are spelled out here once, because they
// also appear in the index.theme files of the two directories (gui/base/resources/icons).
constexpr QLatin1String DarkIconTheme("ovito-dark");
constexpr QLatin1String LightIconTheme("ovito-light");

// The size an icon is rendered with when the user interface does not ask for a particular one.
constexpr int DefaultIconSize = 24;

}   // End of anonymous namespace

/******************************************************************************
* Returns the name of the icon theme that matches the given color scheme.
******************************************************************************/
QString IconTheme::themeName(bool darkTheme)
{
    return darkTheme ? QString(DarkIconTheme) : QString(LightIconTheme);
}

/******************************************************************************
* Makes the icon theme that matches the given color scheme the current one of the process.
******************************************************************************/
void IconTheme::apply(bool darkTheme)
{
    // The light theme is the fallback, so that an icon still resolves in a session whose desktop environment asks for an
    // icon theme of its own.
    QIcon::setFallbackThemeName(QString(LightIconTheme));

    // QIcon resolves themed names through its loader, which searches the resource paths of this icon set for a theme of
    // that name; this call is process-global and cheap, so it can simply be repeated whenever the color scheme changes.
    QIcon::setThemeName(themeName(darkTheme));
}

/******************************************************************************
* Returns the icon of a command's icon path.
******************************************************************************/
QIcon IconTheme::icon(const QString& iconPath)
{
    if(iconPath.isEmpty())
        return {};
    // An icon path that names a resource directly is used as it is; everything else is the name of an icon of the
    // current theme. This is the rule the desktop frontend has always applied to the icons of its commands.
    if(iconPath.startsWith(QLatin1Char(':')))
        return QIcon(iconPath);
    return QIcon::fromTheme(iconPath);
}

/******************************************************************************
* Renders an icon as an image of the given device-pixel size.
******************************************************************************/
QImage IconTheme::image(const QString& iconPath, const QSize& deviceSize, qreal devicePixelRatio)
{
    const QIcon icon = IconTheme::icon(iconPath);
    if(icon.isNull())
        return {};

    // The caller passes the size the user interface wants to display, in device-independent pixels, and the scale factor
    // of the screen it is displayed on; the icon is rasterized at the resulting device-pixel size, so that it stays
    // crisp on a high resolution display instead of being scaled up by the scene graph.
    const QSize size = deviceSize.isEmpty() ? QSize(DefaultIconSize, DefaultIconSize) : deviceSize;
    const qreal scale = devicePixelRatio > 0 ? devicePixelRatio : qreal(1);
    return icon.pixmap(size, scale).toImage();
}

}   // End of namespace
