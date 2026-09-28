// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/utilities/io/ssh/SshConnection.h>
#include "SshRequest.h"

namespace Ovito {

class OVITO_CORE_EXPORT OpensshConnection : public SshConnection
{
    Q_OBJECT

public:

    /// Constructor.
    explicit OpensshConnection(const SshConnectionParameters& serverInfo, QObject* parent = nullptr);

    /// Destructor.
    virtual ~OpensshConnection();

    /// Returns the host this connection is to.
    const QString& hostname() const { return connectionParameters().host; }

    /// Returns the kind of ssh connection this is.
    virtual SshImplementation implementation() const override { return Openssh; }

    /// Returns the path to the "sftp" utility on the user's computer.
    static QString getSftpPath();

    /// Saves the path to the "sftp" utility in the application settings store.
    static void setSftpPath(const QString& path);

public Q_SLOTS:

    /// Opens the connection to the host.
    virtual void connectToHost() override;

    /// Closes the connection to the host.
    virtual void disconnectFromHost() override;

private Q_SLOTS:

    /// Handles QProcess::readyReadStandardOutput() signal.
    void onReadyReadStandardOutput();

    /// Handles QProcess::readyReadStandardError() signal.
    void onReadyReadStandardError();

    /// Starts the next waiting request.
    void processRequests();

Q_SIGNALS:

    void requestFinished();

private:

    /// The sftp process.
    QProcess* _process = nullptr;

    /// The active request.
    QPointer<SshRequest> _activeRequest;

    bool _requestInFlight = false;

    friend class SshRequest;
};

} // End of namespace
