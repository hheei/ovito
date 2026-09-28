// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include "TerminalBackend.h"

#if !defined(Q_OS_WIN)
#if defined(Q_OS_MACOS)
#include <util.h>
#else
#include <pty.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <csignal>
#include <poll.h>
#include <cerrno>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <cstring>
#endif

namespace Ovito {

/******************************************************************************
 * Creates the concrete backend implementation appropriate for the host OS.
 ******************************************************************************/
std::unique_ptr<TerminalBackend> TerminalBackend::create(QObject* parent)
{
#if defined(Q_OS_WIN)
    return std::make_unique<WindowsPty>(parent);
#else
    return std::make_unique<UnixPty>(parent);
#endif
}

#if !defined(Q_OS_WIN)

/******************************************************************************
 * Destructor.
 ******************************************************************************/
UnixPty::~UnixPty()
{
    if(_childPid > 0) terminate();
    if(_readNotifier) _readNotifier->setEnabled(false);
    if(_masterFd >= 0) ::close(_masterFd);
}

/******************************************************************************
 * Spawns the child process attached to the pseudo-terminal.
 ******************************************************************************/
bool UnixPty::start(const QString& program,
                    const QStringList& arguments,
                    const QStringList& environment,
                    const QString& workingDirectory,
                    int initialRows,
                    int initialCols,
                    QString& errorMessage)
{
    // Build the argv[] array for execvp(). The QByteArray buffers must outlive the fork()
    // call below (they do: they're stack-local to this function and copied verbatim into
    // the forked child's address space), but must not be reallocated after argv is built.
    std::vector<QByteArray> argBytes;
    argBytes.push_back(program.toUtf8());
    for(const QString& arg : arguments) argBytes.push_back(arg.toUtf8());
    std::vector<char*> argv;
    argv.reserve(argBytes.size() + 1);
    for(QByteArray& a : argBytes) argv.push_back(a.data());
    argv.push_back(nullptr);

    std::vector<QByteArray> envBytes;
    for(const QString& e : environment) envBytes.push_back(e.toUtf8());

    struct winsize ws{};
    ws.ws_row = static_cast<unsigned short>(initialRows);
    ws.ws_col = static_cast<unsigned short>(initialCols);

    int masterFd = -1;
    pid_t pid = forkpty(&masterFd, nullptr, nullptr, &ws);
    if(pid < 0) {
        errorMessage = tr("forkpty() failed: %1").arg(QString::fromLocal8Bit(strerror(errno)));
        return false;
    }

    if(pid == 0) {
        // Child process: apply any custom environment variables, then exec the program.
        // forkpty() has already made the PTY slave our controlling terminal and
        // stdin/stdout/stderr, so there is nothing else to set up here.
        for(const QByteArray& e : envBytes) {
            int eq = (int)e.indexOf('=');
            if(eq > 0) setenv(e.left(eq).constData(), e.mid(eq + 1).constData(), 1);
        }
        if(!workingDirectory.isEmpty() && ::chdir(workingDirectory.toUtf8().constData()) != 0)
            _exit(126);
        ::execvp(argv[0], argv.data());
        // execvp() only returns on failure.
        _exit(127);
    }

    // Parent process.
    _childPid = pid;
    _masterFd = masterFd;
    int flags = fcntl(_masterFd, F_GETFL, 0);
    fcntl(_masterFd, F_SETFL, flags | O_NONBLOCK);

    _readNotifier = std::make_unique<QSocketNotifier>(_masterFd, QSocketNotifier::Read, this);
    connect(_readNotifier.get(), &QSocketNotifier::activated, this, &UnixPty::onMasterFdReadyRead);

    return true;
}

/******************************************************************************
 * Writes raw bytes to the child process' standard input (the PTY master side).
 ******************************************************************************/
void UnixPty::write(const char* data, qsizetype len)
{
    if(_masterFd < 0) return;
    qsizetype written = 0;
    while(written < len) {
        ssize_t n = ::write(_masterFd, data + written, static_cast<size_t>(len - written));
        if(n > 0) {
            written += n;
            continue;
        }
        if(n < 0 && errno == EINTR) continue;
        if(n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // The PTY master's write buffer is full. Keystroke-rate input is tiny, so a
            // short blocking wait here is fine and avoids the complexity of a write notifier.
            struct pollfd pfd{.fd = _masterFd, .events = POLLOUT, .revents = 0};
            ::poll(&pfd, 1, 100);
            continue;
        }
        break;  // Unrecoverable write error; drop the remaining bytes.
    }
}

/******************************************************************************
 * Informs the PTY (and, in turn, the child process) of a new terminal size.
 ******************************************************************************/
void UnixPty::resize(int rows, int cols)
{
    if(_masterFd < 0) return;
    struct winsize ws{};
    ws.ws_row = static_cast<unsigned short>(rows);
    ws.ws_col = static_cast<unsigned short>(cols);
    // The kernel automatically delivers SIGWINCH to the foreground process group.
    ::ioctl(_masterFd, TIOCSWINSZ, &ws);
}

/******************************************************************************
 * Requests termination of the child process (best-effort).
 ******************************************************************************/
void UnixPty::terminate()
{
    if(_childPid > 0) ::kill(_childPid, SIGHUP);
}

/******************************************************************************
 * Returns true if the child process is still believed to be running.
 ******************************************************************************/
bool UnixPty::isRunning() const { return _childPid > 0; }

/******************************************************************************
 * Reads bytes made available on the PTY master file descriptor, or detects child exit.
 ******************************************************************************/
void UnixPty::onMasterFdReadyRead()
{
    char buf[4096];
    for(;;) {
        ssize_t n = ::read(_masterFd, buf, sizeof(buf));
        if(n > 0) {
            Q_EMIT dataReceived(QByteArray(buf, static_cast<int>(n)));
            if(n < static_cast<ssize_t>(sizeof(buf))) break;  // No more data available right now.
            continue;
        }
        if(n == 0) {
            handleChildExit();
            break;
        }
        if(errno == EINTR) continue;
        if(errno == EAGAIN || errno == EWOULDBLOCK) break;
        if(errno == EIO) {
            // The child closed its end of the PTY -- treat like EOF.
            handleChildExit();
            break;
        }
        Q_EMIT errorOccurred(tr("Error reading from terminal: %1").arg(QString::fromLocal8Bit(strerror(errno))));
        break;
    }
}

/******************************************************************************
 * Waits for the child to terminate, closes the master fd, and emits processExited().
 ******************************************************************************/
void UnixPty::handleChildExit()
{
    if(_readNotifier) {
        _readNotifier->setEnabled(false);
        _readNotifier.reset();
    }
    int exitCode = -1;
    if(_childPid > 0) {
        int status = 0;
        if(::waitpid(_childPid, &status, 0) == _childPid) {
            if(WIFEXITED(status))
                exitCode = WEXITSTATUS(status);
            else if(WIFSIGNALED(status))
                exitCode = 128 + WTERMSIG(status);
        }
        _childPid = -1;
    }
    if(_masterFd >= 0) {
        ::close(_masterFd);
        _masterFd = -1;
    }
    Q_EMIT processExited(exitCode);
}

#else  // defined(Q_OS_WIN)

// UnixPty is never instantiated on Windows (TerminalBackend::create() only ever
// constructs WindowsPty there), but moc's generated qt_static_metacall() references
// UnixPty's methods and its vtable unconditionally, so stub bodies still need to be
// defined here to satisfy the linker.

UnixPty::~UnixPty() = default;

bool UnixPty::start(const QString&, const QStringList&, const QStringList&, const QString&, int, int, QString& errorMessage)
{
    errorMessage = tr("Not supported on this platform.");
    return false;
}

void UnixPty::write(const char*, qsizetype) {}
void UnixPty::resize(int, int) {}
void UnixPty::terminate() {}
bool UnixPty::isRunning() const { return false; }
void UnixPty::onMasterFdReadyRead() {}
void UnixPty::handleChildExit() {}

#endif  // !defined(Q_OS_WIN)

#if defined(Q_OS_WIN)

namespace {

/// Quotes a single argument for use in a Windows command-line string, following the
/// escaping rules that CommandLineToArgvW() (and CRT argv parsing) expect.
QString quoteWindowsCommandLineArg(const QString& arg)
{
    if(!arg.isEmpty() && !arg.contains(QLatin1Char(' ')) && !arg.contains(QLatin1Char('\t')) && !arg.contains(QLatin1Char('"'))) return arg;

    QString result = QStringLiteral("\"");
    int backslashes = 0;
    for(QChar ch : arg) {
        if(ch == QLatin1Char('\\')) {
            ++backslashes;
        }
        else if(ch == QLatin1Char('"')) {
            result += QString(backslashes * 2 + 1, QLatin1Char('\\'));
            result += ch;
            backslashes = 0;
            continue;
        }
        else {
            result += QString(backslashes, QLatin1Char('\\'));
            backslashes = 0;
        }
        result += ch;
    }
    result += QString(backslashes * 2, QLatin1Char('\\'));
    result += QLatin1Char('"');
    return result;
}

}  // anonymous namespace

/******************************************************************************
 * Destructor.
 ******************************************************************************/
WindowsPty::~WindowsPty() { shutdown(); }

/******************************************************************************
 * Spawns the child process attached to the pseudo console.
 ******************************************************************************/
bool WindowsPty::start(const QString& program,
                       const QStringList& arguments,
                       const QStringList& environment,
                       const QString& workingDirectory,
                       int initialRows,
                       int initialCols,
                       QString& errorMessage)
{
    // The backend object outlives an individual session -- TerminalWidget creates it once and
    // reuses it for every start() call -- so all per-session state must be reset here. Most
    // notably _exitFired: left set from a previous session, it would make onProcessExited()
    // swallow the next session's exit notification entirely, so the terminal would never report
    // that its child process has finished. shutdown() is idempotent (every handle it touches is
    // already null after a normal session end) and additionally releases anything left behind by
    // a start() call that failed halfway through.
    shutdown();
    _exitFired = false;
    _procInfo = {};

    HANDLE inputReadSide = nullptr;
    HANDLE outputWriteSide = nullptr;
    SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    if(!CreatePipe(&inputReadSide, &_hPipeIn, &sa, 0) || !CreatePipe(&_hPipeOut, &outputWriteSide, &sa, 0)) {
        errorMessage = tr("Failed to create pipes for the pseudo console.");
        return false;
    }

    HRESULT hr =
        CreatePseudoConsole({static_cast<SHORT>(initialCols), static_cast<SHORT>(initialRows)}, inputReadSide, outputWriteSide, 0, &_hPC);
    // ConPTY duplicates the handles it needs internally; close our copies of the ends it now owns.
    CloseHandle(inputReadSide);
    CloseHandle(outputWriteSide);
    if(FAILED(hr)) {
        errorMessage = tr("CreatePseudoConsole() failed (HRESULT 0x%1).").arg(static_cast<uint>(hr), 8, 16, QLatin1Char('0'));
        return false;
    }

    SIZE_T attrListSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrListSize);
    _attrListBuffer.resize(static_cast<size_t>(attrListSize));
    auto* attrList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(_attrListBuffer.data());
    if(!InitializeProcThreadAttributeList(attrList, 1, 0, &attrListSize)) {
        errorMessage = tr("InitializeProcThreadAttributeList() failed.");
        return false;
    }
    if(!UpdateProcThreadAttribute(attrList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, _hPC, sizeof(HPCON), nullptr, nullptr)) {
        errorMessage = tr("UpdateProcThreadAttribute() failed.");
        DeleteProcThreadAttributeList(attrList);
        return false;
    }

    QString cmdLine = quoteWindowsCommandLineArg(program);
    for(const QString& arg : arguments) cmdLine += QLatin1Char(' ') + quoteWindowsCommandLineArg(arg);
    std::wstring cmdLineStd = cmdLine.toStdWString();
    std::vector<wchar_t> cmdLineBuffer(cmdLineStd.begin(), cmdLineStd.end());
    cmdLineBuffer.push_back(L'\0');

    std::wstring workingDirectoryStd = workingDirectory.toStdWString();
    LPCWSTR workingDirectoryPtr = workingDirectory.isEmpty() ? nullptr : workingDirectoryStd.c_str();

    // Build a double-NUL-terminated environment block if a custom environment was given;
    // otherwise pass nullptr so the child inherits our environment.
    std::vector<wchar_t> envBlock;
    LPVOID envBlockPtr = nullptr;
    if(!environment.isEmpty()) {
        for(const QString& e : environment) {
            std::wstring w = e.toStdWString();
            envBlock.insert(envBlock.end(), w.begin(), w.end());
            envBlock.push_back(L'\0');
        }
        envBlock.push_back(L'\0');
        envBlockPtr = envBlock.data();
    }

    // Create a Job Object configured to kill every process in it once the job's last handle
    // is closed. The child (and, recursively, everything it spawns) is assigned to this job, so
    // that closing OVITO -- even abnormally -- terminates the whole agent process tree. This is
    // the Windows counterpart of the Unix backend delivering SIGHUP to the PTY process group;
    // TerminateProcess() alone would only kill the direct child and orphan its descendants.
    _hJob = CreateJobObjectW(nullptr, nullptr);
    if(_hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo{};
        jobInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if(!SetInformationJobObject(_hJob, JobObjectExtendedLimitInformation, &jobInfo, sizeof(jobInfo))) {
            CloseHandle(_hJob);
            _hJob = nullptr;
        }
    }

    STARTUPINFOEXW startupInfo{};
    startupInfo.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    startupInfo.lpAttributeList = attrList;

    // Start the process suspended so it can be assigned to the job before it runs any code and
    // spawns grandchildren that would otherwise escape the job.
    DWORD creationFlags = EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED | (envBlockPtr ? CREATE_UNICODE_ENVIRONMENT : 0);
    BOOL ok = CreateProcessW(nullptr, cmdLineBuffer.data(), nullptr, nullptr, FALSE, creationFlags, envBlockPtr,
                             workingDirectoryPtr, &startupInfo.StartupInfo, &_procInfo);
    DeleteProcThreadAttributeList(attrList);
    if(!ok) {
        errorMessage = tr("CreateProcess() failed (error %1).").arg(GetLastError());
        if(_hJob) {
            CloseHandle(_hJob);
            _hJob = nullptr;
        }
        return false;
    }

    // Assign the child to the job before resuming it. If assignment fails (e.g. an outer job that
    // forbids nesting -- rare on Windows 8+), drop the job and continue: the terminal still works,
    // just without the guaranteed process-tree kill.
    if(_hJob && !AssignProcessToJobObject(_hJob, _procInfo.hProcess)) {
        CloseHandle(_hJob);
        _hJob = nullptr;
    }
    ResumeThread(_procInfo.hThread);

    // Background thread performing blocking ReadFile() calls on the ConPTY output pipe,
    // since QSocketNotifier does not support Windows pipe HANDLEs.
    _readerThread = QThread::create([this]() { readerThreadMain(); });
    _readerThread->start();

    // Detect process exit via its process handle becoming signaled.
    _exitNotifier = std::make_unique<QWinEventNotifier>(_procInfo.hProcess, this);
    connect(_exitNotifier.get(), &QWinEventNotifier::activated, this, &WindowsPty::onProcessExited);

    return true;
}

/******************************************************************************
 * Writes raw bytes to the child process' standard input (the ConPTY input pipe).
 ******************************************************************************/
void WindowsPty::write(const char* data, qsizetype len)
{
    if(!_hPipeIn) return;
    qsizetype offset = 0;
    while(offset < len) {
        DWORD written = 0;
        if(!WriteFile(_hPipeIn, data + offset, static_cast<DWORD>(len - offset), &written, nullptr)) break;
        offset += written;
    }
}

/******************************************************************************
 * Informs the pseudo console of a new terminal size.
 ******************************************************************************/
void WindowsPty::resize(int rows, int cols)
{
    if(_hPC) ResizePseudoConsole(_hPC, {static_cast<SHORT>(cols), static_cast<SHORT>(rows)});
}

/******************************************************************************
 * Requests termination of the child process (best-effort).
 ******************************************************************************/
void WindowsPty::terminate()
{
    // Killing the job terminates the whole process tree; fall back to killing just the direct
    // child if the job could not be set up (see start()).
    if(_hJob) TerminateJobObject(_hJob, 1);
    else if(_procInfo.hProcess) TerminateProcess(_procInfo.hProcess, 1);
}

/******************************************************************************
 * Returns true if the child process is still believed to be running.
 ******************************************************************************/
bool WindowsPty::isRunning() const { return _procInfo.hProcess != nullptr && !_exitFired; }

/******************************************************************************
 * Handles the process handle becoming signaled (child process exited).
 ******************************************************************************/
void WindowsPty::onProcessExited()
{
    if(_exitFired) return;
    _exitFired = true;
    if(_exitNotifier) _exitNotifier->setEnabled(false);
    DWORD code = 0;
    GetExitCodeProcess(_procInfo.hProcess, &code);
    shutdown();
    Q_EMIT processExited(static_cast<int>(code));
}

/******************************************************************************
 * Background-thread entry point performing blocking ReadFile() calls on the ConPTY output pipe.
 ******************************************************************************/
void WindowsPty::readerThreadMain()
{
    char buf[4096];
    for(;;) {
        DWORD bytesRead = 0;
        BOOL ok = ReadFile(_hPipeOut, buf, sizeof(buf), &bytesRead, nullptr);
        if(!ok || bytesRead == 0) break;
        QByteArray chunk(buf, static_cast<int>(bytesRead));
        QMetaObject::invokeMethod(this, [this, chunk]() { Q_EMIT dataReceived(chunk); }, Qt::QueuedConnection);
    }
}

/******************************************************************************
 * Tears down the pseudo console, pipes, reader thread and process handles.
 ******************************************************************************/
void WindowsPty::shutdown()
{
    // Per Microsoft's ConPTY sample, close the pseudo console before the pipes -- this also
    // terminates the conhost process backing it, which unblocks the reader thread's pending
    // ReadFile() by closing its end of the output pipe.
    if(_hPC) {
        ClosePseudoConsole(_hPC);
        _hPC = nullptr;
    }
    if(_readerThread) {
        if(!_readerThread->wait(200)) {
            // ReadFile() still hasn't returned; force it to unblock. Closing a HANDLE that
            // another thread is blocked on in ReadFile() isn't officially sanctioned by the
            // Win32 API, but is the commonly used, reliable way to abort a pending
            // synchronous pipe read in practice.
            if(_hPipeOut) {
                CloseHandle(_hPipeOut);
                _hPipeOut = nullptr;
            }
            _readerThread->wait();
        }
        _readerThread->deleteLater();
        _readerThread = nullptr;
    }
    if(_hPipeOut) {
        CloseHandle(_hPipeOut);
        _hPipeOut = nullptr;
    }
    if(_hPipeIn) {
        CloseHandle(_hPipeIn);
        _hPipeIn = nullptr;
    }
    if(_procInfo.hThread) {
        CloseHandle(_procInfo.hThread);
        _procInfo.hThread = nullptr;
    }
    if(_procInfo.hProcess) {
        CloseHandle(_procInfo.hProcess);
        _procInfo.hProcess = nullptr;
    }
    // Terminate any remaining processes in the job (grandchildren the direct child may have
    // orphaned) and release the job handle. This mirrors ~UnixPty calling terminate(): closing the
    // handle would kill the tree anyway via JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE, but doing it
    // explicitly makes teardown deterministic on both normal exit and window close.
    if(_hJob) {
        TerminateJobObject(_hJob, 1);
        CloseHandle(_hJob);
        _hJob = nullptr;
    }
}

#else  // !defined(Q_OS_WIN)

// WindowsPty is never instantiated on Linux/macOS (TerminalBackend::create() only ever
// constructs UnixPty there), but moc's generated qt_static_metacall() references
// WindowsPty's methods and its vtable unconditionally, so stub bodies still need to be
// defined here to satisfy the linker.

WindowsPty::~WindowsPty() = default;

bool WindowsPty::start(const QString&, const QStringList&, const QStringList&, const QString&, int, int, QString& errorMessage)
{
    errorMessage = tr("Not supported on this platform.");
    return false;
}

void WindowsPty::write(const char*, qsizetype) {}
void WindowsPty::resize(int, int) {}
void WindowsPty::terminate() {}
bool WindowsPty::isRunning() const { return false; }
void WindowsPty::onProcessExited() {}
void WindowsPty::readerThreadMain() {}
void WindowsPty::shutdown() {}

#endif  // defined(Q_OS_WIN)

}  // namespace Ovito
