// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <QProxyStyle>

namespace Ovito {

/**
 * \brief Customizes some aspects of the standard Qt widget style.
 */
class OvitoStyle : public QProxyStyle
{
public:

#ifdef Q_OS_MACOS
    int pixelMetric(QStyle::PixelMetric metric, const QStyleOption* option = nullptr, const QWidget* widget = nullptr) const override {
        if(metric == PM_ToolBarFrameWidth || metric == PM_ToolBarItemMargin) {
            if(!isMainWindowToolbar(widget))
                return 0;
        }
        return QProxyStyle::pixelMetric(metric, option, widget);
    }
#endif

    void drawControl(ControlElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget = nullptr) const override {
        if(element == CE_ToolBar) {
            if(!isMainWindowToolbar(widget))
                return;
        }
        QProxyStyle::drawControl(element, option, painter, widget);
    }

#ifdef Q_OS_MACOS
    QSize sizeFromContents(ContentsType ct, const QStyleOption* option, const QSize& csz, const QWidget* widget) const override {
        if(ct == CT_ToolButton) {
            if(widget && !isMainWindowToolbar(widget->parentWidget()))
                if(QToolBar* toolbar = qobject_cast<QToolBar*>(widget->parentWidget()))
                    if(toolbar->iconSize().width() >= 24)
                        return QSize(csz.width() + 3, csz.height() + 3);
        }
        return QProxyStyle::sizeFromContents(ct, option, csz, widget);
    }
#endif

private:

    static bool isMainWindowToolbar(const QWidget* widget) {
        if(const QToolBar* toolbar = qobject_cast<const QToolBar*>(widget)) {
            if(QMainWindow* mainWindow = qobject_cast<QMainWindow*>(toolbar->parentWidget())) {
                if(mainWindow->toolBarArea(toolbar) == Qt::TopToolBarArea)
                    return true;
            }
        }
        return false;
    }
};

}   // End of namespace
