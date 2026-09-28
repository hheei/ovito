// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include "SshRequest.h"

namespace Ovito {

class OpensshConnection;

class FileListingRequest : public SshRequest
{
    Q_OBJECT

public:

    /// Constructor.
    FileListingRequest(OpensshConnection* connection, const QString& path);

Q_SIGNALS:

    /// This signal is generated before transmission of a directory listing begins.
    void receivingDirectory();

    /// This signal is generated after a directory listing has been fully transmitted.
    void receivedDirectoryComplete(const QStringList& listing);

protected:

    /// Starts sending commands to the SFTP server.
    virtual void start(QIODevice* device) override;

    /// Handles messages from the SFTP program.
    virtual void handleSftpResponse(QIODevice* device, const QByteArray& line) override;

    /// Handles responses from the SFTP program.
    virtual bool handleSftpError(const QByteArray& line) override;

private:

    const QString _path;
    QStringList _listing;
};

} // End of namespace
