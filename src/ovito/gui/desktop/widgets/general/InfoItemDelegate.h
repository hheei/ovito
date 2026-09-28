// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * This Qt item delegate class that can display an additional info string or color next to each item.
 *
 * To use this delegate with a QAbstractItemView widget, subclass QAbstractItemModel and reimplement the data() method to return
 * the info data for the custom `infoRole` specified in the InfoItemDelegate constructor. The delegate will then render the info next to each item.
 */
class InfoItemDelegate : public QStyledItemDelegate
{
public:

    /// Constructor.
    explicit InfoItemDelegate(QObject* parent, int infoRole) : QStyledItemDelegate(parent), _infoRole(infoRole) {}

    /// Renders the item.
    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    /// Returns the Qt data role used to obtain the info data from the model.
    int infoRole() const { return _infoRole; }

private:

    /// Blend two RGB colors.
    static QColor blendColors(const QColor& color1, const QColor& color2, qreal ratio)
    {
        int r = color1.red() * (1 - ratio) + color2.red() * ratio;
        int g = color1.green() * (1 - ratio) + color2.green() * ratio;
        int b = color1.blue() * (1 - ratio) + color2.blue() * ratio;
        return QColor(r, g, b);
    }

private:

    /// The role used to obtain the info data from the model.
    int _infoRole;
};

}   // End of namespace
