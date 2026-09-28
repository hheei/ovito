// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include "ProcessChannel.h"

namespace Ovito {

class ScpChannel : public ProcessChannel
{
    Q_OBJECT

public:

    /// Constructor.
    explicit ScpChannel(LibsshConnection* connection, const QString& location);

    /// Sets the destination buffer for the received file data.
    void setDestinationBuffer(char* buffer) {
        _dataBuffer = buffer;
        processData();
    }

Q_SIGNALS:

    /// This signal is generated before transmission of a file begins.
    void receivingFile(qint64 fileSize);

    /// This signal is generated during data transmission.
    void receivedData(qint64 totalReceivedBytes);

    /// This signal is generated after a file has been fully transmitted.
    void receivedFileComplete(std::unique_ptr<QTemporaryFile>* localFile);

private:

    enum State {
        StateClosed,
        StateConnected,
        StateReceivingFile,
        StateFileComplete
    };

    /// Part of the state machine implementation.
    void setState(State state) { _state = state; }

    /// Returns the current state of the channel.
    State state() const { return _state; }

private Q_SLOTS:

    /// Is called whenever data arrives from the remote process.
    void processData();

private:

    State _state = StateClosed;
    char* _dataBuffer = nullptr;
    qint64 _bytesReceived = 0;
    qint64 _fileSize = 0;

    /// The local destination file during download.
    std::unique_ptr<QTemporaryFile> _localFile;

    /// The memory-mapped destination file.
    uchar* _fileMapping = nullptr;
};

} // End of namespace
