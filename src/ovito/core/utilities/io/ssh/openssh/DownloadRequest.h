// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include "SshRequest.h"

namespace Ovito {

class OpensshConnection;

class DownloadRequest : public SshRequest
{
    Q_OBJECT

public:

    /// Constructor.
    DownloadRequest(OpensshConnection* connection, const QString& path);

Q_SIGNALS:

    /// This signal is generated before transmission of a file begins.
    void receivingFile(qint64 fileSize);

    /// This signal is generated during data transmission.
    void receivedData(qint64 totalReceivedBytes);

    /// This signal is generated after a file has been fully transmitted.
    void receivedFileComplete(std::unique_ptr<QTemporaryFile>* localFile);

protected:

    /// Starts sending commands to the SFTP server.
    virtual void start(QIODevice* device) override;

    /// Handles messages from the SFTP program.
    virtual void handleSftpResponse(QIODevice* device, const QByteArray& line) override;

    /// Handles responses from the SFTP program.
    virtual bool handleSftpError(const QByteArray& line) override;

    /// Handles timer events.
    virtual void timerEvent(QTimerEvent* event) override;

private:

    const QString _path;
    qint64 _bytesReceived = 0;
    std::unique_ptr<QTemporaryFile> _localFile;
    QBasicTimer _timer;
};

} // End of namespace
