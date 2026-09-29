// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/gui/base/GUIBase.h>
#include <QIcon>
#include <QImage>

namespace Ovito {

/**
 * \brief The icon set the two frontends share.
 *
 * The shared GUI layer ships the icons of both frontends as a pair of icon themes (`ovito-light` and `ovito-dark`,
 * see `gui/base/resources/icons`), which hold the same named icons drawn for a light and for a dark user interface.
 * There is no tinting step: the icon that matches the current color scheme is looked up by name.
 *
 * This class owns that rule so that it exists once. The desktop frontend uses it to fill the icons of the `QAction`
 * views of the commands, and the Qt Quick frontend resolves the same icon paths through its image provider.
 */
class OVITO_GUIBASE_EXPORT IconTheme
{
public:

    /// Returns the name of the icon theme that matches the given color scheme.
    static QString themeName(bool darkTheme);

    /// Makes the icon theme that matches the given color scheme the current one of the process.
    static void apply(bool darkTheme);

    /// Returns the name of the icon theme the process currently uses.
    static QString currentThemeName() { return QIcon::themeName(); }

    /// Returns the icon of a command's icon path: a literal resource path if it starts with ':', the name of a themed
    /// icon of this icon set otherwise. Returns a null icon for an empty path or an unknown name.
    static QIcon icon(const QString& iconPath);

    /// Renders the same icon as an image of the given device-pixel size, which is what an image provider hands to QML.
    /// A null image is returned for an icon that cannot be resolved.
    static QImage image(const QString& iconPath, const QSize& deviceSize, qreal devicePixelRatio);
};

}   // End of namespace
