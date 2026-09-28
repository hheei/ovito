// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include "ProcessChannel.h"

namespace Ovito {

class LsChannel : public ProcessChannel
{
    Q_OBJECT

public:

    /// Constructor.
    explicit LsChannel(LibsshConnection* connection, const QString& location);

    /// Returns the directory listing received from remote host.
    const QStringList& directoryListing() const { return _directoryListing; }

Q_SIGNALS:

    /// This signal is generated before transmission of a directory listing begins.
    void receivingDirectory();

    /// This signal is generated after a directory listing has been fully transmitted.
    void receivedDirectoryComplete(const QStringList& listing);

private Q_SLOTS:

    /// Is called whenever data arrives from the remote process.
    void processData();

private:

    QStringList _directoryListing;
};

} // End of namespace
