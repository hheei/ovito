////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

#include <ovito/core/Core.h>
#include <ovito/core/app/Application.h>
#include "OpensshConnection.h"
#include "DownloadRequest.h"

#ifdef Q_OS_UNIX
    #include <unistd.h>
    #include <signal.h>
#endif

#ifdef Q_OS_WIN
    #include <QStandardPaths>
#endif

namespace Ovito {

#ifdef Q_OS_WIN
/******************************************************************************
* Returns the absolute path of the Win32 OpenSSH 'sftp.exe' that ships
* with Windows in the System32 directory. This binary is incompatible with
* OVITO's protocol because, when its standard input is a pipe, the MSVC
* C runtime puts standard output into full-buffering mode, which causes the
* sftp responses to interleave with output from the helper '!echo' commands
* that OVITO uses to mark request boundaries.
******************************************************************************/
static QString systemSftpPath()
{
    QString systemRoot = QString::fromLocal8Bit(qgetenv("SystemRoot"));
    if(systemRoot.isEmpty())
        systemRoot = QStringLiteral("C:/Windows");
    return QDir::fromNativeSeparators(systemRoot) + QStringLiteral("/System32/OpenSSH/sftp.exe");
}

/******************************************************************************
* Probes well-known install locations on Windows for an 'sftp.exe' that is
* known to work with OVITO. The Cygwin/MSYS2 build that ships with
* Git for Windows uses line-buffered stdout and is therefore compatible.
* Returns an empty string if no suitable sftp utility was found.
******************************************************************************/
static QString findCompatibleWindowsSftp()
{
    static const QStringList candidates = {
        QStringLiteral("C:/Program Files/Git/usr/bin/sftp.exe"),
        QStringLiteral("C:/Program Files (x86)/Git/usr/bin/sftp.exe"),
        QStringLiteral("C:/msys64/usr/bin/sftp.exe"),
        QStringLiteral("C:/cygwin64/bin/sftp.exe"),
        QStringLiteral("C:/cygwin/bin/sftp.exe"),
    };
    for(const QString& path : candidates) {
        if(QFile::exists(path))
            return path;
    }
    return {};
}

/******************************************************************************
* Returns true if the given path resolves to the broken Win32 OpenSSH
* 'sftp.exe' shipped with Windows in the System32 directory.
******************************************************************************/
static bool isIncompatibleSystemSftp(const QString& path)
{
    QString resolved = path;
    if(QFileInfo(resolved).isRelative())
        resolved = QStandardPaths::findExecutable(resolved);
    if(resolved.isEmpty())
        return false;
    QString canonical = QFileInfo(resolved).canonicalFilePath();
    QString system = QFileInfo(systemSftpPath()).canonicalFilePath();
    if(canonical.isEmpty() || system.isEmpty())
        return false;
    return canonical.compare(system, Qt::CaseInsensitive) == 0;
}
#endif

/******************************************************************************
* Returns the path to the "sftp" utility on the user's computer.
******************************************************************************/
QString OpensshConnection::getSftpPath()
{
    QString path = QSettings().value("ssh/sftp_path").toString();
    if(!path.isEmpty())
        return path;
#ifdef Q_OS_WIN
    // On Windows, prefer the Cygwin/MSYS2 sftp shipped with Git for Windows,
    // because the Win32 OpenSSH 'sftp.exe' in System32 is incompatible (see above).
    QString compatible = findCompatibleWindowsSftp();
    if(!compatible.isEmpty())
        return compatible;
#endif
    return QStringLiteral("sftp");
}

/******************************************************************************
* Saves the path to the "sftp" utility in the application settings store.
******************************************************************************/
void OpensshConnection::setSftpPath(const QString& path)
{
    QSettings settings;
    if(path != QStringLiteral("sftp"))
        settings.setValue("ssh/sftp_path", path);
    else
        settings.remove("ssh/sftp_path");
}

/******************************************************************************
* Constructor.
******************************************************************************/
OpensshConnection::OpensshConnection(const SshConnectionParameters& serverInfo, QObject* parent) : SshConnection(serverInfo, parent)
{
    connect(this, &OpensshConnection::requestFinished, this, &OpensshConnection::processRequests, Qt::QueuedConnection);
}

/******************************************************************************
* Destructor.
******************************************************************************/
OpensshConnection::~OpensshConnection()
{
    disconnectFromHost();
}

/******************************************************************************
* Opens the connection to the host.
******************************************************************************/
void OpensshConnection::connectToHost()
{
    disconnectFromHost();
    if(_state == StateClosed) {
        _process = new QProcess(this);
        connect(_process, &QProcess::started, this, [this]() {
            setState(StateConnecting, true);
            //_process->write("-@progress\n"); // Turn off transfer progress meter.
            _process->write("@!echo \"<<<BEGIN_OVITO_SESSION>>>\"\n");  // This will tell us when the ssh connection has been established.
        });
        connect(_process, &QProcess::finished, this, [this]() {
            _errorMessages.push_back(tr("sftp process has exited."));
            QByteArray errOutput = _process->readAllStandardError().trimmed();
            if(!errOutput.isEmpty())
                _errorMessages.push_back(QString::fromLocal8Bit(errOutput));
            setState(StateError, true);
        });
        connect(_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            switch(error) {
            case QProcess::FailedToStart:
                _errorMessages.push_back(tr("Failed to start the sftp utility. Either the invoked program is missing, or you may have insufficient permissions or resources to invoke the program."));
                break;
            case QProcess::Crashed:
                _errorMessages.push_back(tr("Failed to run the sftp utility. The process crashed some time after starting successfully."));
                break;
            default:
                _errorMessages.push_back(tr("Failed to run the external sftp utility."));
                break;
            }
            setState(StateError, true);
        });
        connect(_process, &QProcess::readyReadStandardOutput, this, &OpensshConnection::onReadyReadStandardOutput);
        setState(StateInit, true);
        QStringList arguments;
        if(connectionParameters().port > 0)
            arguments << QStringLiteral("-o") << QStringLiteral("Port=%1").arg(connectionParameters().port);
        if(!connectionParameters().userName.isEmpty())
            arguments << QStringLiteral("-o") << QStringLiteral("User=%1").arg(connectionParameters().userName);
        arguments << QStringLiteral("-C"); // Enable compression.
        arguments << QStringLiteral("-f"); // Flush files to disk immediately after transfer.
        arguments << QStringLiteral("-q"); // Quiet mode: disable the progress meter as well as warning and diagnostic messages from ssh.
        arguments << QStringLiteral("-o") << QStringLiteral("StrictHostKeyChecking=no");
        if(!qEnvironmentVariableIsEmpty("OVITO_SSH_LOG"))
            arguments << QStringLiteral("-vv"); // Raise logging level.
        arguments << connectionParameters().host;
        _process->setArguments(std::move(arguments));
        _process->setProgram(getSftpPath());
        if(_process->program().isEmpty()) {
            _errorMessages.push_back(tr("Please specify the executable path to the 'sftp' utility on your computer."));
            setState(StateError, true);
            return;
        }

#ifdef Q_OS_WIN
        // Refuse to use the Win32 OpenSSH 'sftp.exe' shipped with Windows: when
        // its stdin is a pipe, the MSVC CRT switches stdout to full buffering,
        // which scrambles the response stream OVITO relies on. If the configured
        // path (or the PATH lookup of "sftp") resolves to that incompatible binary,
        // try to substitute a compatible sftp utility from a known install location.
        if(isIncompatibleSystemSftp(_process->program())) {
            QString compatible = findCompatibleWindowsSftp();
            if(!compatible.isEmpty()) {
                _process->setProgram(compatible);
            }
            else {
                _errorMessages.push_back(tr(
                    "The Win32 OpenSSH 'sftp' utility shipped with Windows (located at %1) "
                    "is not compatible with OVITO due to a known stdout buffering issue "
                    "that scrambles server responses when run as a subprocess.\n\n"
                    "Please install Git for Windows (https://git-scm.com/download/win), which "
                    "includes a compatible sftp utility, typically at "
                    "C:\\Program Files\\Git\\usr\\bin\\sftp.exe. OVITO will pick it up automatically.")
                    .arg(QDir::toNativeSeparators(systemSftpPath())));
                setState(StateError, true);
                return;
            }
        }
#endif

        if(Application::guiEnabled()) {
            // Set SSH_ASKPASS and DISPLAY environment variables to make OpenSSH call the askpass utility.
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            QString askpassPath = QDir(Application::instance()->applicationDirPath()).absolutePath() + QStringLiteral("/ssh_askpass");
            env.insert("SSH_ASKPASS", QDir::toNativeSeparators(askpassPath));
            env.insert("SSH_ASKPASS_REQUIRE", "force");
#ifdef Q_OS_UNIX
            if(!env.contains("DISPLAY")) {
                env.insert("DISPLAY", ":0");
            }
            // Use setsid() to detach the sftp process from the terminal and force
            // it to call the askpass utility.
            // Windows has no terminal so this is not required
            _process->setChildProcessModifier([] { ::setsid(); });
#endif
            _process->setProcessEnvironment(env);
        }
        _process->start();
    }
}

/******************************************************************************
* Closes the connection to the host.
******************************************************************************/
void OpensshConnection::disconnectFromHost()
{
    if(_process) {
        setState(StateClosing, false);
        disconnect(_process, nullptr, this, nullptr);
        if(_process->state() == QProcess::Running) {
            connect(_process, &QProcess::finished, _process, &QObject::deleteLater);
            _process->setParent(nullptr);
            _process->write("-@quit\n");
            _process->closeWriteChannel();
        }
        else {
            _process->deleteLater();
        }
        _process = nullptr;
    }
    if(_state != StateClosed && _state != StateCanceledByUser)
        setState(StateClosed, true);
}

/******************************************************************************
* Handles QProcess::readyReadStandardOutput() signal.
******************************************************************************/
void OpensshConnection::onReadyReadStandardOutput()
{
    for(;;) {
        QByteArray line = _process->readLine();
        if(line.isEmpty())
            break;

        if(_state == StateConnecting && line.contains("<<<BEGIN_OVITO_SESSION>>>")) {
            connect(_process, &QProcess::readyReadStandardError, this, &OpensshConnection::onReadyReadStandardError);
            setState(StateOpened, true);
            processRequests();
        }
        else if(line.contains("<<<END_OVITO_REQUEST>>>")) {
            OVITO_ASSERT(_requestInFlight);
            _requestInFlight = false;
            if(_activeRequest)
                delete _activeRequest.data();
            OVITO_ASSERT(_activeRequest.isNull());
            Q_EMIT requestFinished();
        }
        else if(_state == StateOpened && _requestInFlight) {
            if(!_activeRequest.isNull())
                _activeRequest->handleSftpResponse(_process, line);
        }
        else {
#ifdef OVITO_DEBUG
            std::cout << "stdout: ";
#endif
            std::cout << line.trimmed().constData() << std::endl;
        }
    }
}

/******************************************************************************
* Handles QProcess::readyReadStandardError() signal.
******************************************************************************/
void OpensshConnection::onReadyReadStandardError()
{
    auto lines = _process->readAllStandardError().split('\n');
    for(const QByteArray& line : lines) {
        if(line.isEmpty())
            continue;

        if(_state == StateOpened && _requestInFlight && !_activeRequest.isNull()) {
            if(_activeRequest->handleSftpError(line))
                continue;
        }
#ifdef OVITO_DEBUG
        std::cerr << "stderr: ";
#endif
        std::cerr << line.trimmed().constData() << std::endl;
    }
}

/******************************************************************************
* Proceed with processing the next waiting request.
******************************************************************************/
void OpensshConnection::processRequests()
{
    if(_state == StateOpened && !_requestInFlight && _activeRequest.isNull()) {
        _activeRequest = findChild<SshRequest*>({}, Qt::FindDirectChildrenOnly);
        if(!_activeRequest.isNull()) {
            connect(_activeRequest, &SshRequest::closed, this, [&]() {
#ifdef Q_OS_UNIX
                if(_activeRequest.data() == QObject::sender() && _activeRequest->_isInterruptable && _requestInFlight && _process && _process->processId() > 0) {
                    ::kill(pid_t(_process->processId()), SIGINT);
                }
#endif
                _activeRequest.clear();
            });
            _activeRequest->start(_process);
            if(_process && !_activeRequest.isNull()) {
                // Signal end of request.
                _requestInFlight = true;
                _process->write("@!echo \"<<<END_OVITO_REQUEST>>>\"\n");
            }
        }
    }
}

} // End of namespace
