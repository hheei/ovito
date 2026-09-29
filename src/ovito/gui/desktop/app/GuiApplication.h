// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/utilities/Exception.h>
#include <ovito/core/app/StandaloneApplication.h>

namespace Ovito {

/**
 * \brief The main application with a graphical user interface.
 */
class OVITO_GUI_EXPORT GuiApplication : public StandaloneApplication
{
    Q_OBJECT

public:

    /// Returns the one and only instance of this class.
    static GuiApplication* instance() { return static_cast<GuiApplication*>(Application::instance()); }

    /// Constructor.
    GuiApplication();

    /// Handler function for exceptions.
    virtual void reportError(const Exception& exception, bool blocking = false) override;

    /// Returns whether the application currently uses a dark UI theme.
    bool usingDarkTheme() const;

    /// Initializes an abstract user interface (e.g. a MainWindow).
    static void initializeUserInterface(UserInterface& userInterface, const QStringList& arguments);

    /// Returns whether app's UI should automatically follow the system color scheme.
    static bool automaticallyEnableDarkMode();

protected:

    /// Create the global instance of the right QCoreApplication derived class.
    virtual QCoreApplication* createQtApplicationImpl(bool supportGui, int& argc, char** argv) override;

    /// Defines the program's command line parameters.
    virtual void registerCommandLineParameters(QCommandLineParser& parser) override;

    /// Interprets the command line parameters provided to the application.
    virtual bool processCommandLineParameters() override;

    /// Prepares application to start running.
    virtual MainThreadOperation startupApplication() override;

    /// Is called at program startup once the event loop is running.
    virtual void postStartupInitialization() override;

    /// Handles events sent to the Qt application object.
    virtual bool eventFilter(QObject* watched, QEvent* event) override;

#ifdef OVITO_SSH_CLIENT
    /// \brief Asks the user for the login password for a SSH server.
    /// \return True on success, false if user has canceled the operation.
    static bool askUserForPassword(const QString& hostname, const QString& username, QString& password);

    /// \brief Asks the user for the answer to a keyboard-interactive question sent by the SSH server.
    /// \return True on success, false if user has canceled the operation.
    static bool askUserForKbiResponse(const QString& hostname, const QString& username, const QString& instruction, const QString& question, bool showAnswer, QString& answer);

    /// \brief Asks the user for the passphrase for a private SSH key.
    /// \return True on success, false if user has canceled the operation.
    static bool askUserForKeyPassphrase(const QString& hostname, const QString& prompt, QString& passphrase);

    /// \brief Asks the user for PKCS#11 smartcard credentials.
    /// \return True on success, false if user has canceled the operation.
    static bool askUserForPKCS11Credentials(const QString& hostname, const QString& username, QString& pkcs11Uri, QString& pin);

    /// \brief Informs the user about an unknown SSH host.
    static bool detectedUnknownSshServer(const QString& hostname, const QString& unknownHostMessage, const QString& hostPublicKeyHash);
#endif

private:

};

}   // End of namespace
