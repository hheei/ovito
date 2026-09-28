// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once
#include <ovito/gui/desktop/GUI.h>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <QWinEventNotifier>
#endif

namespace Ovito {

/**
 * \brief Abstract interface to a platform-specific pseudo-terminal (PTY) that runs a
 *        child process and exchanges raw bytes with it.
 *
 * Concrete implementations (a forkpty()-based one for Linux/macOS and a ConPTY-based
 * one for Windows) are defined in TerminalBackend.cpp and selected automatically by
 * the create() factory function.
 */
class OVITO_GUI_EXPORT TerminalBackend : public QObject
{
    Q_OBJECT

public:
    /// Constructor.
    explicit TerminalBackend(QObject* parent = nullptr) : QObject(parent) {}

    /// Creates the concrete backend implementation appropriate for the host OS.
    static std::unique_ptr<TerminalBackend> create(QObject* parent = nullptr);

    /// Spawns the child process attached to the pseudo-terminal.
    /// Returns false and sets errorMessage on failure.
    virtual bool start(const QString& program,
                       const QStringList& arguments,
                       const QStringList& environment,
                       const QString& workingDirectory,
                       int initialRows,
                       int initialCols,
                       QString& errorMessage) = 0;

    /// Writes raw bytes to the child process' standard input (the PTY master side).
    virtual void write(const char* data, qsizetype len) = 0;

    /// Informs the PTY (and, in turn, the child process) of a new terminal size.
    virtual void resize(int rows, int cols) = 0;

    /// Requests termination of the child process (best-effort).
    virtual void terminate() = 0;

    /// Returns true if the child process is still believed to be running.
    [[nodiscard]] virtual bool isRunning() const = 0;

Q_SIGNALS:

    /// Emitted on the GUI thread whenever bytes have been read from the child process.
    void dataReceived(const QByteArray& data);

    /// Emitted exactly once when the child process has exited.
    /// 'exitCode' is -1 if it terminated abnormally or the exit code could not be determined.
    void processExited(int exitCode);

    /// Emitted if an unrecoverable I/O error occurs on the pseudo-terminal.
    void errorOccurred(const QString& message);
};

/// forkpty()-based backend implementation, used on Linux and macOS.
/// Implemented in TerminalBackend.cpp.
class UnixPty : public TerminalBackend
{
    Q_OBJECT

public:
    explicit UnixPty(QObject* parent = nullptr) : TerminalBackend(parent) {}
    virtual ~UnixPty() override;

    virtual bool start(const QString& program,
                       const QStringList& arguments,
                       const QStringList& environment,
                       const QString& workingDirectory,
                       int initialRows,
                       int initialCols,
                       QString& errorMessage) override;
    virtual void write(const char* data, qsizetype len) override;
    virtual void resize(int rows, int cols) override;
    virtual void terminate() override;
    [[nodiscard]] virtual bool isRunning() const override;

private Q_SLOTS:

    /// Reads bytes made available on the PTY master file descriptor, or detects child exit.
    void onMasterFdReadyRead();

private:
    /// Waits for the child to terminate, closes the master fd, and emits processExited().
    void handleChildExit();

#if !defined(Q_OS_WIN)
    pid_t _childPid = -1;
#endif
    int _masterFd = -1;
    std::unique_ptr<QSocketNotifier> _readNotifier;
};

/// ConPTY-based backend implementation, used on Windows.
/// Implemented in TerminalBackend.cpp.
class WindowsPty : public TerminalBackend
{
    Q_OBJECT

public:
    explicit WindowsPty(QObject* parent = nullptr) : TerminalBackend(parent) {}
    virtual ~WindowsPty() override;

    virtual bool start(const QString& program,
                       const QStringList& arguments,
                       const QStringList& environment,
                       const QString& workingDirectory,
                       int initialRows,
                       int initialCols,
                       QString& errorMessage) override;
    virtual void write(const char* data, qsizetype len) override;
    virtual void resize(int rows, int cols) override;
    virtual void terminate() override;
    [[nodiscard]] virtual bool isRunning() const override;

private Q_SLOTS:

    /// Handles the process handle becoming signaled (child process exited).
    void onProcessExited();

private:
    /// Background-thread entry point performing blocking ReadFile() calls on the ConPTY output pipe.
    void readerThreadMain();

    /// Tears down the pseudo console, pipes, reader thread and process handles.
    void shutdown();

#if defined(Q_OS_WIN)
    HPCON _hPC = nullptr;
    HANDLE _hPipeIn = nullptr;
    HANDLE _hPipeOut = nullptr;
    HANDLE _hJob = nullptr;
    PROCESS_INFORMATION _procInfo{};
    QThread* _readerThread = nullptr;
    std::unique_ptr<QWinEventNotifier> _exitNotifier;
    std::vector<char> _attrListBuffer;
#endif
    bool _exitFired = false;
};

}  // namespace Ovito
