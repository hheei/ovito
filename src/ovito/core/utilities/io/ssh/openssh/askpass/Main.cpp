// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <iostream>
#include <QApplication>
#include <QInputDialog>
#include <QTimer>

int main(int argc, char* argv[])
{
    QTimer::singleShot(0, []() {
        QInputDialog inputDialog;
        inputDialog.setWindowTitle(QStringLiteral("OVITO - SSH Connection"));
        inputDialog.setTextEchoMode(QLineEdit::Password);

        QString labelText = QStringLiteral("<p style=\"margin-right: 200px\">A password is required.</p>");
        QStringList arguments = QCoreApplication::arguments();
        if(arguments.count() > 1)
            labelText.append(QStringLiteral("<p>%1</p>").arg(arguments[1].toHtmlEscaped()));
        inputDialog.setLabelText(labelText);

        if(inputDialog.exec() == QDialog::Accepted) {
            std::cout << qPrintable(inputDialog.textValue()) << std::endl;
        }
        else {
            QCoreApplication::exit(-1);
        }
    });

    return QApplication(argc, argv).exec();
}
