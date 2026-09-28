// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * A custom version of the QMessageBox dialog class.
 *
 * On macOS, this wrapper class prevents QMessageBox from using the native dialog window,
 * which shows various issues since Qt 6.4.
 */
class MessageDialog : public QMessageBox
{
public:

    /// Constructor.
    MessageDialog(QWidget* parent = nullptr) : QMessageBox(parent) {
#ifdef Q_OS_MACOS
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
#endif
    }

    /// Constructor.
    MessageDialog(QMessageBox::Icon icon, const QString& title, const QString& text, QMessageBox::StandardButtons buttons = NoButton, QWidget* parent = nullptr, Qt::WindowFlags f = Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint)
        : QMessageBox(icon, title, text, buttons, parent, f) {
#ifdef Q_OS_MACOS
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
#endif
    }

#ifdef Q_OS_MACOS
    /// Destructor.
    ~MessageDialog() {
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, false);
    }
#endif

#ifdef Q_OS_MACOS
    static QMessageBox::StandardButton critical(QWidget* parent, const QString& title, const QString& text, QMessageBox::StandardButtons buttons = Ok, QMessageBox::StandardButton defaultButton = NoButton) {
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
        auto result = QMessageBox::critical(parent, title, text, buttons, defaultButton);
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, false);
        return result;
    }

    static QMessageBox::StandardButton question(QWidget* parent, const QString& title, const QString& text, QMessageBox::StandardButtons buttons = StandardButtons(Yes | No), QMessageBox::StandardButton defaultButton = NoButton) {
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
        auto result = QMessageBox::question(parent, title, text, buttons, defaultButton);
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, false);
        return result;
    }
#endif
};

}   // End of namespace
