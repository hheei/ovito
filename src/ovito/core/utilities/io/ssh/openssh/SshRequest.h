// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>

namespace Ovito {

class OpensshConnection;

class SshRequest : public QObject
{
    Q_OBJECT

public:

    /// Destructor.
    virtual ~SshRequest() {
        Q_EMIT closed();
    }

    /// Initiate the request.
    void submit();

Q_SIGNALS:

    /// This signal is generated when the process terminated (for whatever reason).
    void closed();

    /// This signal is generated when the download process failed because of some error.
    void error(const QString& errorMessage);

protected:

    /// Constructor.
    explicit SshRequest(OpensshConnection* connection);

    /// Tells the request to start sending commands to the SFTP server.
    virtual void start(QIODevice* device) = 0;

    /// Handles responses from the SFTP program.
    virtual void handleSftpResponse(QIODevice* device, const QByteArray& line) = 0;

    /// Handles responses from the SFTP program.
    virtual bool handleSftpError(const QByteArray& line) {
        if(line.startsWith("Connection closed")) {
            _isInterruptable = false;
            Q_EMIT error(tr("SSH connection was closed."));
            return true;
        }
        return false;
    }

    /// Puts a path argument in quotes and escapes special characters.
    static QByteArray quoteAgument(const QString& arg);

    bool _isInterruptable = false;

    friend class OpensshConnection;
};

} // End of namespace
