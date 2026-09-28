// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once
#include <ovito/gui/desktop/GUI.h>
#include "TerminalBackend.h"
#include <chrono>

// On Windows, <rpcndr.h> defines the macro 'small' as 'char', which collides with
// the 'small' bit-field member in libvterm's VTermScreenCellAttrs struct. Temporarily
// undefine the macro across the vterm.h include and restore the caller's state afterward.
#pragma push_macro("small")
#undef small
#include <3rdparty/libvterm/include/vterm.h>
#pragma pop_macro("small")

namespace Ovito {

/**
 * \brief A widget that displays and drives an interactive terminal session.
 *
 * The widget is backed by a TerminalBackend (a pseudo-terminal running a child
 * process) and a libvterm VTerm instance, which interprets the VT100/xterm byte
 * stream coming from the child process and drives the on-screen cell grid that
 * this widget paints.
 */
class OVITO_GUI_EXPORT TerminalWidget : public QAbstractScrollArea
{
    Q_OBJECT

public:

    /// Constructor.
    explicit TerminalWidget(QWidget* parent = nullptr);

    /// Destructor.
    virtual ~TerminalWidget();

    /// Starts a new terminal session running the given program.
    /// Returns false and displays an inline error message if the backend could not be started.
    bool start(const QString& program, const QStringList& arguments = {}, const QStringList& environment = {},
              const QString& workingDirectory = {});

    /// Returns true if a child process is currently attached and running.
    bool isSessionActive() const;

    /// Terminates the running child process, if any (best-effort).
    void stop();

    /// Returns the preferred size of the widget (based on a default 80x24 cell grid).
    [[nodiscard]] virtual QSize sizeHint() const override;

    /// Displays an inline error message in place of the terminal grid (e.g. because the
    /// backend reported a fatal I/O error while a session was active).
    void reportError(const QString& message);

    /// Returns the message passed to the most recent reportError() call, or the error
    /// recorded internally by a failed start() call.
    const QString& lastErrorMessage() const { return _errorMessage; }

    /// Returns the current font size adjustment, in points relative to the system's default
    /// fixed-pitch font size.
    [[nodiscard]] int fontSizeOffset() const { return _fontSizeOffset; }

public Q_SLOTS:

    /// Sets the font size adjustment, in points relative to the system's default fixed-pitch font
    /// size. The value is clamped to the supported range and is persisted across program sessions.
    void setFontSizeOffset(int offset);

    /// Enlarges the terminal font by one point, up to the supported maximum.
    void increaseFontSize() { setFontSizeOffset(fontSizeOffset() + 1); }

    /// Shrinks the terminal font by one point, down to the supported minimum.
    void decreaseFontSize() { setFontSizeOffset(fontSizeOffset() - 1); }

    /// Restores the system's default fixed-pitch font size.
    void resetFontSize() { setFontSizeOffset(0); }

Q_SIGNALS:

    /// Emitted once when the child process exits, with its exit code.
    void processFinished(int exitCode);

    /// Emitted when the terminal application sets a new window title (OSC 2 / OSC 0).
    void titleChanged(const QString& title);

protected:

    /// Intercepts Tab/Backtab while the terminal has focus, before Qt's focus navigation can
    /// consume them, so that they reach the child process as terminal input.
    virtual bool event(QEvent* event) override;

    /// This is called by the system to paint the viewport area.
    virtual void paintEvent(QPaintEvent* event) override;

    /// Handles viewport resize events.
    virtual void resizeEvent(QResizeEvent* event) override;

    /// Translates key presses into terminal input bytes.
    virtual void keyPressEvent(QKeyEvent* event) override;

    /// Gives keyboard focus to the widget on mouse click; begins or extends a text selection.
    virtual void mousePressEvent(QMouseEvent* event) override;

    /// Extends the text selection while a mouse button is held down.
    virtual void mouseMoveEvent(QMouseEvent* event) override;

    /// Finalizes (or clears, on a plain click) the text selection.
    virtual void mouseReleaseEvent(QMouseEvent* event) override;

    /// Selects the word under the mouse cursor.
    virtual void mouseDoubleClickEvent(QMouseEvent* event) override;

    /// Forwards wheel input to the child app's mouse-reporting protocol when it has requested
    /// one; otherwise falls through to QAbstractScrollArea's default scrollback-scrolling behavior.
    virtual void wheelEvent(QWheelEvent* event) override;

    /// Shows the Copy/Paste context menu.
    virtual void contextMenuEvent(QContextMenuEvent* event) override;

private Q_SLOTS:

    /// Handles TerminalBackend::dataReceived(): feeds bytes into libvterm.
    void onBackendDataReceived(const QByteArray& data);

    /// Handles TerminalBackend::processExited(): tears down and re-emits processFinished().
    void onBackendProcessExited(int exitCode);

    /// Handles TerminalBackend::errorOccurred(): surfaces a fatal I/O error.
    void onBackendError(const QString& message);

    /// Handles scrollbar value changes triggered by the user (scrollback navigation).
    void onScrollBarValueChanged(int value);

private:

    /// Recomputes the terminal's row/column count from the viewport size and font metrics,
    /// and forwards the new size to libvterm and the backend.
    void updateGridSize();

    /// Rebuilds the terminal font from the system's fixed-pitch font and the current user size
    /// offset, and recomputes the per-cell pixel metrics derived from it.
    void applyTerminalFont();

    /// Returns the point size of the unmodified system fixed-pitch font, converting from pixels
    /// in case the font happens to be specified that way.
    [[nodiscard]] qreal baseFontPointSize() const;

    /// Updates the vertical scrollbar's range to reflect the current scrollback size.
    void updateScrollBarRange();

    /// Converts a VTermColor into a QColor, resolving indexed colors against the standard
    /// xterm 256-color palette.
    [[nodiscard]] QColor cellColor(const VTermColor& color, bool isBackground) const;

    /// Computes the sub-rectangle of the terminal's cell grid — in grid (column,row) units, not
    /// pixels — that overlaps the given viewport-pixel rectangle, clamped to the current grid
    /// dimensions. Used by paintEvent() to avoid repainting cells outside the invalidated region.
    [[nodiscard]] QRect dirtyCellRange(const QRect& pixelRect) const;

    /// Paints one row of the cell grid across the given column range, batching consecutive cells
    /// that share identical resolved foreground/background color and text attributes into a
    /// single fillRect()/drawText() call pair instead of drawing each cell individually.
    /// 'cellAt' supplies the cell contents for a column (abstracting over the live-screen vs.
    /// scrollback-line paint paths); 'cursorCol' is the cursor's column within this row, or -1 if
    /// the cursor is not on this row; 'selColBegin'/'selColEnd' give the selected column range
    /// [selColBegin, selColEnd) within this row, or (-1,-1) if this row has no selection.
    void paintGridRow(QPainter& painter, int viewRow, int colBegin, int colEnd,
                      const std::function<VTermScreenCell(int)>& cellAt, int cursorCol,
                      int selColBegin, int selColEnd) const;

    /// Maps a Qt key event onto libvterm's key/modifier representation and feeds it to libvterm.
    void sendKeyEvent(QKeyEvent* event);

    /// Resolves the cell at the given (viewRow, col) position, transparently accounting for the
    /// current scrollback/live-screen split -- the same addressing paintEvent() uses for its
    /// row loop. Returns a blank cell if the position lies outside the grid or the scrollback.
    [[nodiscard]] VTermScreenCell cellAtViewRow(int viewRow, int col) const;

    /// Converts a viewport-pixel position into a (col, row) grid cell, clamped to the current grid.
    [[nodiscard]] QPoint pixelToCell(const QPoint& pos) const;

    /// Converts a viewport-pixel position into a 0-based (row, col) position in live-grid space
    /// (libvterm's coordinate space, independent of scrollback) -- unlike pixelToCell(), which
    /// returns (col, row) in view-row space, shifted by _scrollOffset. Callers forwarding mouse
    /// events to libvterm must un-scroll first (see sendKeyEvent()'s equivalent handling) and
    /// use this instead of pixelToCell().
    [[nodiscard]] QPoint pixelToLiveGridPos(const QPoint& pos) const;

    /// The modifiers that let the user force a local text selection even while the child app has
    /// grabbed the mouse. Shift is the xterm convention and works everywhere; on macOS the native
    /// terminals (Terminal.app, iTerm2) use Option instead, and TUI apps commonly tell the user so
    /// on screen ("option+click to native select"), so accept that as well rather than leaving
    /// those on-screen instructions broken. The cost is that Option-drag no longer reaches the
    /// child app as a Meta-modified mouse report -- the same tradeoff the native terminals make.
    static constexpr Qt::KeyboardModifiers LocalSelectionOverrideModifiers =
#if defined(Q_OS_MACOS)
        Qt::ShiftModifier | Qt::AltModifier;
#else
        Qt::ShiftModifier;
#endif

    /// The modifiers of the font size keyboard chords: Cmd on macOS (where Qt reports the physical
    /// Cmd key as Qt::ControlModifier), Ctrl everywhere else -- the native convention on each.
    ///
    /// Unlike the copy/paste chords in keyPressEvent(), these deliberately do *not* add Shift on
    /// Windows and Linux. Ctrl+C and Ctrl+V have to stay clear of the child process because they
    /// are SIGINT and readline's quoted-insert, but Ctrl with "+", "-" or "0" encodes no terminal
    /// control code, so there is nothing to stay clear of. Adding Shift would in fact be the
    /// riskier choice: on a US layout Ctrl+Shift+"-" is precisely Ctrl+_, readline's undo.
    static constexpr Qt::KeyboardModifiers FontSizeChordModifiers = Qt::ControlModifier;

    /// True if the child app currently wants mouse events forwarded to it rather than handled
    /// as local text selection: it has requested some mouse-reporting mode and the given
    /// modifiers don't include one of the local-selection override modifiers.
    [[nodiscard]] bool wantsMouseForward(Qt::KeyboardModifiers mods) const
    {
        return _vterm && _mouseReportMode != VTERM_PROP_MOUSE_NONE && !(mods & LocalSelectionOverrideModifiers);
    }

    /// True if the child app has grabbed the mouse, i.e. a plain drag is reported to it instead of
    /// starting a local text selection. Used to explain a disabled "Copy" action to the user.
    [[nodiscard]] bool childAppGrabsMouse() const
    {
        return _vterm && _mouseReportMode != VTERM_PROP_MOUSE_NONE;
    }

    /// Returns the selection endpoints (_selAnchor/_selActive) reordered so the first one precedes
    /// the second in reading order (row, then column).
    [[nodiscard]] std::pair<QPoint, QPoint> normalizedSelection() const;

    /// If the given row (in view-row space) intersects the current selection, returns true and
    /// sets [colBegin, colEnd) to the selected column range on that row.
    [[nodiscard]] bool selectionRangeForRow(int viewRow, int& colBegin, int& colEnd) const;

    /// Clears the current text selection, if any, and repaints the rows it used to cover.
    void clearSelection();

    /// Extends the selection to cover the word at the given cell.
    void selectWordAt(const QPoint& cell);

    /// Extends the selection to cover the entire given row (trimmed of trailing blanks on copy).
    void selectLineAt(int row);

    /// Returns the currently selected text (empty string if there is no selection), joining
    /// selected rows with '\n' and right-trimming each line of trailing blank cells.
    [[nodiscard]] QString selectedText() const;

    /// Copies the current selection to the system clipboard (no-op if there is no selection).
    void copySelectionToClipboard() const;

    /// Feeds the system clipboard's text content into the terminal session as if it had been
    /// typed, wrapped in libvterm's bracketed-paste markers.
    void pasteFromClipboard();

    // libvterm screen callback trampolines. Each looks up the owning TerminalWidget via the
    // 'user' pointer and forwards to a same-named private member function.
    static int cb_damage(VTermRect rect, void* user);
    static int cb_moverect(VTermRect dest, VTermRect src, void* user);
    static int cb_movecursor(VTermPos pos, VTermPos oldPos, int visible, void* user);
    static int cb_settermprop(VTermProp prop, VTermValue* val, void* user);
    static int cb_bell(void* user);
    static int cb_sb_pushline(int cols, const VTermScreenCell* cells, void* user);
    static int cb_sb_popline(int cols, VTermScreenCell* cells, void* user);

    /// libvterm output callback trampoline: forwards already VT100-encoded bytes to the backend.
    static void cb_output(const char* s, size_t len, void* user);

    void handleDamage(const VTermRect& rect);
    void handleMoveCursor(VTermPos pos, VTermPos oldPos, bool visible);
    void handleSetTermProp(VTermProp prop, const VTermValue& val);
    void handleBell();
    void pushScrollbackLine(int cols, const VTermScreenCell* cells);
    int popScrollbackLine(int cols, VTermScreenCell* cells);

    /// The pseudo-terminal backend (owns the child process).
    std::unique_ptr<TerminalBackend> _backend;

    /// The libvterm terminal emulator state machine. Owns _screen internally.
    VTerm* _vterm = nullptr;
    VTermScreen* _screen = nullptr;

    /// Scrollback buffer: lines pushed off the top of the live screen by libvterm's
    /// sb_pushline callback, popped back by sb_popline when the user scrolls down again.
    std::deque<QVector<VTermScreenCell>> _scrollback;
    static constexpr int MaxScrollbackLines = 512;

    /// Number of scrollback lines currently scrolled into view (0 = showing the live screen).
    int _scrollOffset = 0;

    /// Current grid geometry.
    int _rows = 24;
    int _cols = 80;

    /// The unmodified system fixed-pitch font, kept as the pristine base from which
    /// applyTerminalFont() derives _terminalFont. The derived font must never be measured to
    /// produce the next one, because it has a letter-spacing correction baked into it.
    QFont _baseFont;

    /// The user's font size adjustment, in points relative to the base font's own size.
    int _fontSizeOffset = 0;

    /// The range within which _fontSizeOffset is kept. Chosen so that a typical 11pt system fixed
    /// font can be taken down to ~7pt and up to ~25pt, which spans everything from a cramped panel
    /// to a high-resolution display without permitting a grid so fine or so coarse as to be
    /// unusable.
    static constexpr int MinFontSizeOffset = -4;
    static constexpr int MaxFontSizeOffset = 14;

    /// Cached monospace font and per-cell pixel metrics.
    QFont _terminalFont;
    QSize _cellSize;

    /// Distance from the top of a cell to the text baseline, used to position glyphs vertically.
    int _cellBaseline = 0;

    /// Current cursor position and visibility, as reported by libvterm.
    VTermPos _cursorPos{0, 0};
    bool _cursorVisible = true;

    /// Set to true once the backend has reported that the child process has exited.
    bool _sessionEnded = false;

    /// Error message to display in place of the terminal grid, set by a failed start() call
    /// or by reportError() (e.g. a fatal backend I/O error).
    QString _errorMessage;

    /// Accumulates OSC title-string fragments until the final fragment arrives.
    QString _pendingTitle;

    /// True while a left-button drag is actively extending the selection.
    bool _selecting = false;

    /// True if there is a non-empty text selection.
    bool _hasSelection = false;

    /// Selection endpoints as (col, row) in view-row space (see cellAtViewRow()). Not
    /// necessarily in reading order; selectionRangeForRow() normalizes them.
    QPoint _selAnchor;
    QPoint _selActive;

    /// State for manually detecting triple-clicks (Qt has no native triple-click event): the
    /// number of rapid successive clicks seen so far in the same cell (capped at 3), and when/where
    /// the most recent one landed.
    int _clickCount = 0;
    QPoint _lastClickCell;
    std::chrono::steady_clock::time_point _lastClickTime;

    /// Mouse-reporting mode last requested by the child app via DECSET 1000/1002/1003 (delivered
    /// through handleSetTermProp(VTERM_PROP_MOUSE, ...)); VTERM_PROP_MOUSE_NONE means the app
    /// hasn't asked for mouse events. Note: vterm_screen_reset() clears libvterm's internal
    /// mouse-tracking state directly, without going through settermprop, so this field is also
    /// reset by hand in onBackendProcessExited().
    int _mouseReportMode = VTERM_PROP_MOUSE_NONE;

    /// Leftover fractional wheel delta (in QWheelEvent::angleDelta() units) not yet large enough
    /// to form a whole "notch" (120 units), carried over between wheelEvent() calls so that
    /// high-resolution scrolling devices (trackpads) -- which deliver many small-delta events per
    /// gesture -- still eventually forward whole notches to the child app instead of being
    /// truncated to zero on every individual event.
    int _wheelAngleAccum = 0;

    /// The same leftover-delta bookkeeping as _wheelAngleAccum, but for the Ctrl/Cmd+wheel font
    /// size gesture. Kept separate so that a zoom gesture and a scroll gesture can never consume
    /// each other's accumulated fraction of a notch.
    int _zoomAngleAccum = 0;
};

}   // End of namespace
