// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include "LsChannel.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
LsChannel::LsChannel(LibsshConnection* connection, const QString& location) :
    ProcessChannel(connection, QStringLiteral("ls -A -U -1 -p --color=never \"%1/\"").arg(location))
{
    connect(this, &QIODevice::readyRead, this, &LsChannel::processData);
    connect(this, &ProcessChannel::opened, this, &LsChannel::receivingDirectory);
    connect(this, &ProcessChannel::finished, this, [this](int exitCode) {
        if(exitCode == 0) {
            Q_EMIT receivedDirectoryComplete(_directoryListing);
        }
        else {
            setError(tr("Failed to produce remote directory listing: 'ls' command returned exit code %1").arg(exitCode));
        }
    });
}

/******************************************************************************
* Is called whenever data arrives from the remote process.
******************************************************************************/
void LsChannel::processData()
{
    while(canReadLine()) {
        QByteArray line = readLine();
        line.chop(1);   // Remote end of line character.
        if(line.size() == 0)
            continue;
        if(line.startsWith('"') && line.endsWith('"'))
            line = line.mid(1, line.size() - 2); // Remove quotes around filenames.
        if(line.endsWith('/'))
            continue;    // Skip directory entries.
        _directoryListing.push_back(QString::fromLocal8Bit(line));
    }
}

} // End of namespace
