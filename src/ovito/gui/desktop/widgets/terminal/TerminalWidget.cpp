// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include "TerminalWidget.h"

namespace Ovito {

namespace {

/// Resolved, per-cell visual style used to detect run boundaries while batching draw calls
/// in TerminalWidget::paintGridRow().
struct CellStyle
{
    QColor fg, bg;
    bool bold = false;
    bool underline = false;
    bool italic = false;
    bool strike = false;

    bool operator==(const CellStyle&) const = default;
};

/// Background color used to highlight selected cells, drawn over a cell's normal background.
const QColor kSelectionHighlightColor(58, 97, 140);

/// QSettings key under which the user's terminal font size adjustment is persisted, so that a size
/// chosen once is restored in subsequent program sessions.
constexpr const char* FontSizeOffsetSettingsKey = "terminal/font_size_offset";

/// True if 'key' is a modifier key pressed/released on its own, with no associated character.
/// Qt delivers a standalone key event for these (e.g. the Cmd key-down that precedes the "C"
/// key-down of a Cmd+C chord on macOS); such events must not be treated as "the user is typing"
/// -- otherwise a Cmd+C copy chord clears the very selection it's about to copy before the "C"
/// event even arrives.
bool isModifierOnlyKey(int key)
{
    switch(key) {
        case Qt::Key_Shift:
        case Qt::Key_Control:
        case Qt::Key_Meta:
        case Qt::Key_Alt:
        case Qt::Key_AltGr:
        case Qt::Key_CapsLock:
        case Qt::Key_NumLock:
        case Qt::Key_ScrollLock:
        case Qt::Key_Super_L:
        case Qt::Key_Super_R:
        case Qt::Key_Hyper_L:
        case Qt::Key_Hyper_R:
            return true;
        default:
            return false;
    }
}

/// Maps Qt keyboard modifiers onto libvterm's VTERM_MOD_* bitmask, shared by keyboard and mouse
/// input forwarding so both stay in sync.
VTermModifier qtModifiersToVterm(Qt::KeyboardModifiers mods)
{
    int mod = VTERM_MOD_NONE;
    if(mods & Qt::ShiftModifier) mod |= VTERM_MOD_SHIFT;
    if(mods & Qt::AltModifier) mod |= VTERM_MOD_ALT;
    if(mods & Qt::ControlModifier) mod |= VTERM_MOD_CTRL;
    return static_cast<VTermModifier>(mod);
}

/// Maps a Qt mouse button onto libvterm's 1-based mouse button numbering (1=left, 2=middle,
/// 3=right), or 0 if the button has no libvterm equivalent.
int qtButtonToVterm(Qt::MouseButton button)
{
    switch(button) {
        case Qt::LeftButton: return 1;
        case Qt::MiddleButton: return 2;
        case Qt::RightButton: return 3;
        default: return 0;
    }
}

} // End of anonymous namespace

/******************************************************************************
 * Constructor.
 ******************************************************************************/
TerminalWidget::TerminalWidget(QWidget* parent) : QAbstractScrollArea(parent)
{
    _backend = TerminalBackend::create(this);
    connect(_backend.get(), &TerminalBackend::dataReceived, this, &TerminalWidget::onBackendDataReceived);
    connect(_backend.get(), &TerminalBackend::processExited, this, &TerminalWidget::onBackendProcessExited);
    connect(_backend.get(), &TerminalBackend::errorOccurred, this, &TerminalWidget::onBackendError);

    _baseFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    // Clamped on read as well as on write, so that a stale or hand-edited settings file can never
    // leave the terminal at an unusable size.
    _fontSizeOffset = qBound(MinFontSizeOffset,
                             QSettings().value(QString::fromLatin1(FontSizeOffsetSettingsKey), 0).toInt(),
                             MaxFontSizeOffset);
    applyTerminalFont();

    setFocusPolicy(Qt::StrongFocus);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &TerminalWidget::onScrollBarValueChanged);

    // Pick a dark background, matching the convention used by FrameBufferWidget.
    QPalette pal = viewport()->palette();
    pal.setColor(QPalette::Window, QColor(20, 20, 20));
    viewport()->setPalette(pal);
    viewport()->setAutoFillBackground(false);  // We fill the background in paintEvent().
    viewport()->setBackgroundRole(QPalette::Window);

    // paintEvent() always fully repaints whatever region it's asked to (see the unconditional
    // fillRect() at its top), so tell Qt not to erase-fill newly exposed regions itself during
    // resize -- without this, a flash of the system background color (gray) is visible for a
    // frame or two while live-resizing (especially on macOS, where resize events can arrive
    // faster than Qt's normal deferred repaint scheduling can keep up).
    viewport()->setAttribute(Qt::WA_OpaquePaintEvent, true);
    viewport()->setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);

    // Qt only delivers mouseMoveEvent() while no button is held if mouse tracking is explicitly
    // enabled -- required for VTERM_PROP_MOUSE_MOVE (xterm mode 1003), which reports motion with
    // no button down. Harmless when no app has requested that mode: the extra events fall through
    // to the existing drag-selection check, already a no-op while not dragging.
    viewport()->setMouseTracking(true);

    _vterm = vterm_new(_rows, _cols);
    vterm_set_utf8(_vterm, 1);
    _screen = vterm_obtain_screen(_vterm);

    static const VTermScreenCallbacks callbacks = {
        &TerminalWidget::cb_damage,
        &TerminalWidget::cb_moverect,
        &TerminalWidget::cb_movecursor,
        &TerminalWidget::cb_settermprop,
        &TerminalWidget::cb_bell,
        nullptr,  // resize -- we already know our own size when we call vterm_set_size() ourselves.
        &TerminalWidget::cb_sb_pushline,
        &TerminalWidget::cb_sb_popline,
        nullptr,  // sb_clear
    };
    vterm_screen_set_callbacks(_screen, &callbacks, this);
    vterm_screen_reset(_screen, 1);
    vterm_screen_enable_altscreen(_screen, 1);  // Needed for full-screen TUI apps.
    vterm_output_set_callback(_vterm, &TerminalWidget::cb_output, this);
}

/******************************************************************************
 * Destructor.
 ******************************************************************************/
TerminalWidget::~TerminalWidget()
{
    stop();
    if(_vterm) vterm_free(_vterm);
}

/******************************************************************************
 * Returns the point size of the unmodified system fixed-pitch font.
 ******************************************************************************/
qreal TerminalWidget::baseFontPointSize() const
{
    qreal pointSize = _baseFont.pointSizeF();
    if(pointSize > 0.0) return pointSize;

    // The system's fixed-pitch font is specified in pixels rather than points on this platform.
    // Convert it, so that the user's offset keeps its intended meaning; fall back to a plausible
    // default in the (not expected) case that neither size is set.
    if(_baseFont.pixelSize() > 0 && logicalDpiY() > 0)
        return _baseFont.pixelSize() * 72.0 / logicalDpiY();
    return 10.0;
}

/******************************************************************************
 * Rebuilds the terminal font from the system's fixed-pitch font and the current user size offset,
 * and recomputes the per-cell pixel metrics derived from it.
 ******************************************************************************/
void TerminalWidget::applyTerminalFont()
{
    // Always start over from the pristine base font. _terminalFont carries the letter-spacing
    // correction applied at the end of this function, which would otherwise be folded into the
    // measurement below -- and compound with every subsequent size change.
    _terminalFont = _baseFont;
    _terminalFont.setPointSizeF(std::max(1.0, baseFontPointSize() + _fontSizeOffset));

    QFontMetricsF fm(_terminalFont);
    qreal glyphAdvance = fm.horizontalAdvance(QLatin1Char('M'));
    _cellSize = QSize(std::max(1, qRound(glyphAdvance)), std::max(1, qCeil(fm.height())));
    _cellBaseline = qRound(fm.ascent());

    // The glyph advance of a monospace font is generally fractional (e.g. 6.015625px for Menlo 10pt),
    // whereas the cell grid must be laid out on whole pixels. Left uncorrected, the two disagree by a
    // fraction of a pixel per character, which accumulates across a run of cells drawn in a single
    // drawText() call until the trailing glyph no longer lands within its own cell. Compensating with
    // an absolute letter spacing makes every character advance by exactly _cellSize.width(), so glyph
    // positions stay locked to the grid no matter how long the run is.
    _terminalFont.setLetterSpacing(QFont::AbsoluteSpacing, _cellSize.width() - glyphAdvance);
}

/******************************************************************************
 * Sets the font size adjustment, in points relative to the system's default fixed-pitch font size.
 ******************************************************************************/
void TerminalWidget::setFontSizeOffset(int offset)
{
    offset = qBound(MinFontSizeOffset, offset, MaxFontSizeOffset);
    if(offset == _fontSizeOffset) return;
    _fontSizeOffset = offset;
    QSettings().setValue(QString::fromLatin1(FontSizeOffsetSettingsKey), _fontSizeOffset);

    applyTerminalFont();

    // The widget's size hint is derived from the cell metrics, which have just changed.
    updateGeometry();

    // The selection's view-row coordinates refer to the old cell grid, so they don't survive a
    // change of the cell size -- just as they don't survive a window resize.
    clearSelection();

    // Reflows the grid onto the new cell size and informs libvterm and the child process. Note
    // that this does nothing at all whenever the new row/column count happens to come out the same
    // as before, which is exactly why the scrollbar range and the repaint below are refreshed
    // unconditionally instead of being left to updateGridSize().
    updateGridSize();
    updateScrollBarRange();
    viewport()->update();
}

/******************************************************************************
 * Starts a new terminal session running the given program.
 ******************************************************************************/
bool TerminalWidget::start(const QString& program, const QStringList& arguments, const QStringList& environment,
                           const QString& workingDirectory)
{
    _errorMessage.clear();
    _sessionEnded = false;
    clearSelection();
    updateGridSize();

    QString errorMessage;
    if(!_backend->start(program, arguments, environment, workingDirectory, _rows, _cols, errorMessage)) {
        _errorMessage = errorMessage;
        viewport()->update();
        return false;
    }
    return true;
}

/******************************************************************************
 * Returns true if a child process is currently attached and running.
 ******************************************************************************/
bool TerminalWidget::isSessionActive() const { return _backend && _backend->isRunning() && !_sessionEnded; }

/******************************************************************************
 * Displays an inline error message in place of the terminal grid.
 ******************************************************************************/
void TerminalWidget::reportError(const QString& message)
{
    _errorMessage = message;
    viewport()->update();
}

/******************************************************************************
 * Terminates the running child process, if any (best-effort).
 ******************************************************************************/
void TerminalWidget::stop()
{
    if(_backend) _backend->terminate();
}

/******************************************************************************
 * Returns the preferred size of the widget (based on a default 80x24 cell grid).
 ******************************************************************************/
QSize TerminalWidget::sizeHint() const
{
    QSize cell = _cellSize.isEmpty() ? QSize(8, 16) : _cellSize;
    return QSize(cell.width() * 80, cell.height() * 24) + QSize(2 * frameWidth(), 2 * frameWidth());
}

/******************************************************************************
 * This is called by the system to paint the viewport area.
 ******************************************************************************/
void TerminalWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(viewport());
    painter.setFont(_terminalFont);
    painter.fillRect(event->rect(), viewport()->palette().color(QPalette::Window));

    if(!_errorMessage.isEmpty()) {
        painter.setPen(QColor(220, 80, 80));
        painter.drawText(viewport()->rect().adjusted(8, 8, -8, -8), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, _errorMessage);
        return;
    }
    if(!_vterm || _cellSize.isEmpty()) return;

    // Only repaint the cells that actually overlap the invalidated region, instead of the
    // whole grid -- this matters a lot, since handleDamage()/handleMoveCursor() request
    // small, targeted repaints (a single changed line, or just the cursor cell) far more
    // often than a full-grid repaint is actually needed.
    QRect dirty = dirtyCellRange(event->rect());
    if(dirty.isEmpty()) return;
    const int colBegin = dirty.left();
    const int colEnd = dirty.left() + dirty.width();

    for(int row = dirty.top(); row < dirty.top() + dirty.height(); row++) {
        auto cellAt = [this, row](int col) -> VTermScreenCell { return cellAtViewRow(row, col); };
        int selColBegin = -1;
        int selColEnd = -1;
        (void)selectionRangeForRow(row, selColBegin, selColEnd);
        if(row < _scrollOffset) {
            paintGridRow(painter, row, colBegin, colEnd, cellAt, -1, selColBegin, selColEnd);
            continue;
        }
        const int liveRow = row - _scrollOffset;
        int cursorCol = (_scrollOffset == 0 && _cursorVisible && liveRow == _cursorPos.row) ? _cursorPos.col : -1;
        paintGridRow(painter, row, colBegin, colEnd, cellAt, cursorCol, selColBegin, selColEnd);
    }
}

/******************************************************************************
 * Resolves the cell at the given (viewRow, col) position, transparently accounting for the
 * current scrollback/live-screen split -- the same addressing paintEvent() uses for its row loop.
 ******************************************************************************/
VTermScreenCell TerminalWidget::cellAtViewRow(int viewRow, int col) const
{
    if(viewRow < _scrollOffset) {
        int sbIndex = _scrollOffset - 1 - viewRow;
        if(sbIndex < 0 || sbIndex >= static_cast<int>(_scrollback.size())) return VTermScreenCell{};
        const QVector<VTermScreenCell>& line = _scrollback[sbIndex];
        return col >= 0 && col < line.size() ? line[col] : VTermScreenCell{};
    }
    VTermScreenCell cell{};
    if(_screen) vterm_screen_get_cell(_screen, VTermPos{viewRow - _scrollOffset, col}, &cell);
    return cell;
}

/******************************************************************************
 * Computes the sub-rectangle of the terminal's cell grid -- in grid (column,row) units, not
 * pixels -- that overlaps the given viewport-pixel rectangle, clamped to the current grid
 * dimensions.
 ******************************************************************************/
QRect TerminalWidget::dirtyCellRange(const QRect& pixelRect) const
{
    if(_cellSize.isEmpty() || pixelRect.isEmpty())
        return QRect();
    // right()/bottom()+1 (rather than just dividing left()/top()) ensures a cell that is only
    // partially covered by pixelRect at its trailing edge is still included, not excluded.
    int colBegin = qBound(0, pixelRect.left() / _cellSize.width(), _cols);
    int colEnd = qBound(0, (pixelRect.right() / _cellSize.width()) + 1, _cols);
    int rowBegin = qBound(0, pixelRect.top() / _cellSize.height(), _rows);
    int rowEnd = qBound(0, (pixelRect.bottom() / _cellSize.height()) + 1, _rows);
    if(colEnd <= colBegin || rowEnd <= rowBegin)
        return QRect();
    return QRect(colBegin, rowBegin, colEnd - colBegin, rowEnd - rowBegin);
}

/******************************************************************************
 * Paints one row of the cell grid across the given column range, batching consecutive cells
 * that share identical resolved foreground/background color and text attributes into a
 * single fillRect()/drawText() call pair instead of drawing each cell individually.
 ******************************************************************************/
void TerminalWidget::paintGridRow(QPainter& painter, int viewRow, int colBegin, int colEnd,
                                  const std::function<VTermScreenCell(int)>& cellAt, int cursorCol,
                                  int selColBegin, int selColEnd) const
{
    QFont appliedFont = painter.font();
    QColor appliedPen;
    bool havePen = false;

    // Applies the given run's pen/font to the painter, but only if they actually differ from
    // what's currently applied -- QPainter::setFont()/setPen() are not free to call thousands
    // of times per repaint, so skipping redundant calls between consecutive runs matters.
    auto applyStyle = [&](const CellStyle& style) {
        if(!havePen || style.fg != appliedPen) {
            painter.setPen(style.fg);
            appliedPen = style.fg;
            havePen = true;
        }
        QFont font = _terminalFont;
        font.setBold(style.bold);
        font.setUnderline(style.underline);
        font.setItalic(style.italic);
        font.setStrikeOut(style.strike);
        if(font != appliedFont) {
            painter.setFont(font);
            appliedFont = font;
        }
    };

    // Resolves column 'col' into its style, display text, and whether it actually has a glyph
    // to draw. Blank cells contribute a single space (rather than nothing) so that concatenating
    // multiple cells' text within a batched run preserves column alignment.
    auto resolveCell = [&](int col, bool isCursorCell, CellStyle& style, QString& text, bool& hasGlyph, bool& hollow) {
        VTermScreenCell cell = cellAt(col);
        QColor fg = cellColor(cell.fg, false);
        QColor bg = cellColor(cell.bg, true);
        if(cell.attrs.reverse) std::swap(fg, bg);
        if(col >= selColBegin && col < selColEnd) bg = kSelectionHighlightColor;
        hollow = false;
        if(isCursorCell) {
            if(hasFocus())
                std::swap(fg, bg);
            else
                hollow = true;
        }
        style = CellStyle{fg, bg, static_cast<bool>(cell.attrs.bold), cell.attrs.underline != VTERM_UNDERLINE_OFF,
                          static_cast<bool>(cell.attrs.italic), static_cast<bool>(cell.attrs.strike)};
        int nchars = 0;
        while(nchars < VTERM_MAX_CHARS_PER_CELL && cell.chars[nchars] != 0) nchars++;
        hasGlyph = nchars > 0;
        text = hasGlyph ? QString::fromUcs4(reinterpret_cast<const char32_t*>(cell.chars), nchars) : QStringLiteral(" ");
    };

    auto flushRun = [&](int runStart, int runEnd, const CellStyle& style, const QString& text, bool hasGlyph) {
        if(runEnd <= runStart) return;
        QRect runRect(runStart * _cellSize.width(), viewRow * _cellSize.height(),
                      (runEnd - runStart) * _cellSize.width(), _cellSize.height());
        painter.fillRect(runRect, style.bg);
        if(hasGlyph) {
            applyStyle(style);
            // Drawn at an explicit baseline point rather than into runRect, because the rect-based
            // overload clips the text to the rect. Glyph ink is allowed to extend slightly beyond the
            // cell it advances by -- italics overhang, and some glyphs are simply wider than the
            // font's nominal advance -- and clipping would shave those off at the run boundary.
            painter.drawText(runRect.left(), runRect.top() + _cellBaseline, text);
        }
    };

    int runStart = colBegin;
    bool runOpen = false;
    CellStyle runStyle;
    QString runText;
    bool runHasGlyph = false;

    for(int col = colBegin; col < colEnd; col++) {
        if(col == cursorCol) {
            // The cursor cell is always its own run: for the focused (fg/bg-swapped) case its
            // style will typically differ from its neighbors anyway, but for the unfocused
            // hollow-outline case colors aren't swapped, so the style might otherwise match --
            // forcing a boundary here guarantees the outline is drawn regardless.
            if(runOpen) {
                flushRun(runStart, col, runStyle, runText, runHasGlyph);
                runOpen = false;
            }
            CellStyle style;
            QString text;
            bool hasGlyph, hollow;
            resolveCell(col, true, style, text, hasGlyph, hollow);
            flushRun(col, col + 1, style, text, hasGlyph);
            if(hollow) {
                painter.setPen(style.fg);
                appliedPen = style.fg;
                havePen = true;
                painter.drawRect(QRect(col * _cellSize.width(), viewRow * _cellSize.height(),
                                       _cellSize.width(), _cellSize.height()).adjusted(0, 0, -1, -1));
            }
            runStart = col + 1;
            continue;
        }

        CellStyle style;
        QString text;
        bool hasGlyph, hollow;
        resolveCell(col, false, style, text, hasGlyph, hollow);

        if(runOpen && style == runStyle) {
            runText += text;
            runHasGlyph = runHasGlyph || hasGlyph;
        }
        else {
            if(runOpen) flushRun(runStart, col, runStyle, runText, runHasGlyph);
            runStart = col;
            runStyle = style;
            runText = text;
            runHasGlyph = hasGlyph;
            runOpen = true;
        }
    }
    if(runOpen) flushRun(runStart, colEnd, runStyle, runText, runHasGlyph);
}

/******************************************************************************
 * Handles viewport resize events.
 ******************************************************************************/
void TerminalWidget::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateGridSize();
}

/******************************************************************************
 * Intercepts Tab/Backtab while the terminal has focus, before Qt's focus navigation can
 * consume them, so that they reach the child process as terminal input. Also claims
 * ShortcutOverride events so that keys bound to application-wide QAction shortcuts (e.g. Space
 * for ACTION_TOGGLE_ANIMATION_PLAYBACK) reach the child process as ordinary input instead of
 * triggering the shortcut.
 ******************************************************************************/
bool TerminalWidget::event(QEvent* event)
{
    // QWidget::event() diverts Tab/Backtab into focus navigation (focusNextPrevChild()) before
    // keyPressEvent() ever sees them. A terminal must forward these keys to the child process
    // instead, so intercept them here -- but only while a session is live and the terminal itself
    // holds keyboard focus, so that a Tab press merely propagating up from another widget still
    // performs normal focus navigation. Ctrl/Alt chords are exempt from Qt's diversion anyway and
    // are left to the standard event dispatch below.
    if(_vterm && hasFocus() && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if(keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Backtab) {
            keyPressEvent(keyEvent);
            return true;
        }
    }
    // Before delivering a key press that matches a registered QAction/QShortcut, Qt sends a
    // ShortcutOverride event to the focus widget first, giving it a chance to claim the key for
    // itself. Accepting it here tells Qt not to consume the subsequent KeyPress as the shortcut,
    // so it reaches keyPressEvent() below and gets forwarded to the child process like any other
    // key -- otherwise, e.g. a plain Space in a running shell would toggle animation playback
    // instead of being typed. keyPressEvent() forwards virtually all keys while a session is
    // live, so claim every key here under the same condition.
    if(_vterm && hasFocus() && event->type() == QEvent::ShortcutOverride) {
        event->accept();
        return true;
    }
    return QAbstractScrollArea::event(event);
}

/******************************************************************************
 * Translates key presses into terminal input bytes.
 ******************************************************************************/
void TerminalWidget::keyPressEvent(QKeyEvent* event)
{
    if(!_vterm) {
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }

    // A bare modifier key-down (no attached character) is not "the user typing" -- most notably,
    // it's what Qt delivers for the Cmd key-down that precedes the "C" key-down of a Cmd+C chord
    // on macOS. Ignore it outright so it can neither clear the current selection nor be forwarded
    // to the child process as terminal input.
    if(isModifierOnlyKey(event->key())) {
        event->accept();
        return;
    }

    // Copy/paste shortcuts, deliberately *not* bound to plain Ctrl+C/Ctrl+V (which must keep
    // going to the child process unchanged -- SIGINT and readline's quoted-insert, respectively).
    // On macOS, Qt reports the Cmd key via Qt::ControlModifier (it swaps Ctrl/Cmd for
    // cross-platform code), so "Ctrl+C" here means the physical Cmd+C chord; the physical
    // Control key arrives as Qt::MetaModifier instead and falls through untouched below.
#if defined(Q_OS_MACOS)
    const bool copyChord = (event->modifiers() == Qt::ControlModifier) && event->key() == Qt::Key_C;
    const bool pasteChord = (event->modifiers() == Qt::ControlModifier) && event->key() == Qt::Key_V;
#else
    const bool copyChord = (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) && event->key() == Qt::Key_C;
    const bool pasteChord = (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) && event->key() == Qt::Key_V;
#endif
    if(pasteChord) {
        pasteFromClipboard();
        event->accept();
        return;
    }
    if(copyChord && _hasSelection) {
        copySelectionToClipboard();
        event->accept();
        return;
    }

    // Font size adjustment. Not expressed via QKeySequence::ZoomIn/ZoomOut, because Qt provides no
    // ZoomReset counterpart for the Ctrl/Cmd+0 half of the set, and one explicit test reads better
    // than a mixture of the two styles.
    //
    // Shift is masked out rather than tested, because on most layouts "+" is only reachable as
    // Shift+"=" -- so the chord has to tolerate Shift, but must still be identified by the
    // *unshifted* keys. That is also what keeps Ctrl+_ (Ctrl+Shift+"-" on a US layout, readline's
    // undo) working: it arrives as Qt::Key_Underscore, matches nothing below, and falls through to
    // the child process untouched. The keypad's +/- and 0 carry Qt::KeypadModifier and are masked
    // for the same reason: the chord means the same thing there.
    Qt::KeyboardModifiers fontSizeChord = event->modifiers();
    fontSizeChord.setFlag(Qt::KeypadModifier, false);
    fontSizeChord.setFlag(Qt::ShiftModifier, false);
    if(fontSizeChord == FontSizeChordModifiers) {
        switch(event->key()) {
            case Qt::Key_Plus:
            case Qt::Key_Equal: increaseFontSize(); event->accept(); return;
            case Qt::Key_Minus: decreaseFontSize(); event->accept(); return;
            case Qt::Key_0: resetFontSize(); event->accept(); return;
            default: break;
        }
    }

    // Any other keypress cancels an existing selection, matching normal terminal-emulator
    // behavior (typing replaces the on-screen highlight).
    if(_hasSelection) clearSelection();

    sendKeyEvent(event);
    event->accept();
}

/******************************************************************************
 * Gives keyboard focus to the widget on mouse click; begins or extends a text selection.
 ******************************************************************************/
void TerminalWidget::mousePressEvent(QMouseEvent* event)
{
    setFocus(Qt::MouseFocusReason);

    int vtButton = qtButtonToVterm(event->button());
    if(vtButton != 0 && wantsMouseForward(event->modifiers())) {
        if(_scrollOffset != 0) verticalScrollBar()->setValue(verticalScrollBar()->maximum());
        QPoint rc = pixelToLiveGridPos(event->pos());
        VTermModifier mod = qtModifiersToVterm(event->modifiers());
        vterm_mouse_move(_vterm, rc.x(), rc.y(), mod);
        vterm_mouse_button(_vterm, vtButton, true, mod);
        QAbstractScrollArea::mousePressEvent(event);
        return;
    }

    if(event->button() == Qt::LeftButton) {
        QPoint cell = pixelToCell(event->pos());
        auto now = std::chrono::steady_clock::now();
        bool sameSpot = (cell == _lastClickCell) &&
                        (now - _lastClickTime) <= std::chrono::milliseconds(QApplication::doubleClickInterval());
        if(sameSpot && _clickCount == 2) {
            // Third rapid click in the same cell: Qt has no native triple-click event, so we
            // detect it ourselves as the press immediately following a synthesized double-click.
            _clickCount = 3;
            selectLineAt(cell.y());
        }
        else {
            _clickCount = 1;
            _selecting = true;
            _selAnchor = _selActive = cell;
            _hasSelection = false;
        }
        _lastClickCell = cell;
        _lastClickTime = now;
    }
    QAbstractScrollArea::mousePressEvent(event);
}

/******************************************************************************
 * Extends the text selection while a mouse button is held down, or reports motion to the
 * child app if it has requested mouse-drag/move tracking.
 ******************************************************************************/
void TerminalWidget::mouseMoveEvent(QMouseEvent* event)
{
    if(wantsMouseForward(event->modifiers())) {
        QPoint rc = pixelToLiveGridPos(event->pos());
        vterm_mouse_move(_vterm, rc.x(), rc.y(), qtModifiersToVterm(event->modifiers()));
        QAbstractScrollArea::mouseMoveEvent(event);
        return;
    }

    if(_selecting && (event->buttons() & Qt::LeftButton)) {
        QPoint cell = pixelToCell(event->pos());
        if(cell != _selActive) {
            _selActive = cell;
            _hasSelection = (_selAnchor != _selActive);
            viewport()->update();
        }
    }
    QAbstractScrollArea::mouseMoveEvent(event);
}

/******************************************************************************
 * Finalizes (or clears, on a plain click) the text selection, or reports a button release to
 * the child app if it has requested mouse tracking.
 ******************************************************************************/
void TerminalWidget::mouseReleaseEvent(QMouseEvent* event)
{
    int vtButton = qtButtonToVterm(event->button());
    if(vtButton != 0 && wantsMouseForward(event->modifiers())) {
        QPoint rc = pixelToLiveGridPos(event->pos());
        VTermModifier mod = qtModifiersToVterm(event->modifiers());
        vterm_mouse_move(_vterm, rc.x(), rc.y(), mod);
        vterm_mouse_button(_vterm, vtButton, false, mod);
        QAbstractScrollArea::mouseReleaseEvent(event);
        return;
    }

    if(event->button() == Qt::LeftButton) {
        bool wasDragSelecting = _selecting;
        _selecting = false;
        if(wasDragSelecting && _selAnchor == _selActive) clearSelection();
    }
    QAbstractScrollArea::mouseReleaseEvent(event);
}

/******************************************************************************
 * Selects the word under the mouse cursor, or reports a button press to the child app if it
 * has requested mouse tracking (so a rapid second click isn't silently dropped).
 ******************************************************************************/
void TerminalWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton && wantsMouseForward(event->modifiers())) {
        QPoint rc = pixelToLiveGridPos(event->pos());
        VTermModifier mod = qtModifiersToVterm(event->modifiers());
        vterm_mouse_move(_vterm, rc.x(), rc.y(), mod);
        vterm_mouse_button(_vterm, 1, true, mod);
        QAbstractScrollArea::mouseDoubleClickEvent(event);
        return;
    }

    if(event->button() == Qt::LeftButton) {
        QPoint cell = pixelToCell(event->pos());
        selectWordAt(cell);
        _clickCount = 2;
        _lastClickCell = cell;
        _lastClickTime = std::chrono::steady_clock::now();
    }
    QAbstractScrollArea::mouseDoubleClickEvent(event);
}

/******************************************************************************
 * Forwards wheel input to the child app's mouse-reporting protocol (as button 4/5 events) when
 * it has requested one; otherwise scrolls the local scrollback via the base class.
 ******************************************************************************/
void TerminalWidget::wheelEvent(QWheelEvent* event)
{
    // Tested before the mouse-forwarding branch below: a coding agent's full-screen interface
    // normally has mouse reporting turned on, and would otherwise swallow the zoom gesture as an
    // ordinary scroll report. Qt maps the physical Cmd key to Qt::ControlModifier on macOS, so this
    // is Cmd+wheel there and Ctrl+wheel everywhere else -- the native convention on both.
    if(event->modifiers() & Qt::ControlModifier) {
        // Accumulated separately from _wheelAngleAccum for the same reason that one exists at all:
        // trackpads deliver many sub-notch events per gesture. Sharing a single accumulator would
        // let a preceding scroll gesture's leftover fraction spill into the zoom, and vice versa.
        _zoomAngleAccum += event->angleDelta().y();
        int notches = _zoomAngleAccum / 120;
        _zoomAngleAccum -= notches * 120;
        if(notches != 0) setFontSizeOffset(_fontSizeOffset + notches);
        event->accept();
        return;
    }

    if(wantsMouseForward(event->modifiers())) {
        if(_scrollOffset != 0) verticalScrollBar()->setValue(verticalScrollBar()->maximum());
        QPoint rc = pixelToLiveGridPos(event->position().toPoint());
        VTermModifier mod = qtModifiersToVterm(event->modifiers());
        vterm_mouse_move(_vterm, rc.x(), rc.y(), mod);

        // libvterm has no dedicated wheel API in this version -- encode as button 4 (up) / 5
        // (down) press events; wheel buttons never need a matching release (mouse.c). Accumulate
        // angleDelta() across events rather than dividing each one independently: trackpads and
        // other high-resolution scrolling devices (the common case on macOS) deliver many events
        // per gesture with a delta well under the 120-per-notch convention, so truncating each
        // event on its own would silently drop nearly all of them and forward nothing.
        _wheelAngleAccum += event->angleDelta().y();
        int notches = _wheelAngleAccum / 120;
        _wheelAngleAccum -= notches * 120;
        int button = notches > 0 ? 4 : 5;
        for(int i = 0; i < std::abs(notches); i++)
            vterm_mouse_button(_vterm, button, true, mod);

        event->accept();
        return;
    }
    QAbstractScrollArea::wheelEvent(event);
}

/******************************************************************************
 * Shows the Copy/Paste context menu.
 ******************************************************************************/
void TerminalWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    QAction* copyAction = menu.addAction(tr("Copy"));
    copyAction->setEnabled(_hasSelection);
    if(!_hasSelection && childAppGrabsMouse()) {
        // A full-screen TUI app (an interactive coding agent, an editor, ...) has requested mouse
        // reporting, so a plain drag went to it rather than creating a selection here -- and the
        // highlight the user sees is the app's own, which we have no access to. Without a hint,
        // the permanently greyed-out "Copy" looks broken; name the modifier that does work.
#if defined(Q_OS_MACOS)
        copyAction->setText(tr("Copy (hold ⌥ Option while dragging to select)"));
#else
        copyAction->setText(tr("Copy (hold Shift while dragging to select)"));
#endif
    }
    QAction* pasteAction = menu.addAction(tr("Paste"));
    pasteAction->setEnabled(isSessionActive());

    // The font size commands. This menu is the only place they are discoverable, which is why each
    // one also advertises its keyboard chord -- and why the chords have to be made explicitly
    // visible: Qt omits shortcut text from context menus by default. They are labels only. These
    // actions live and die with this menu and are never registered anywhere, and event() claims
    // every ShortcutOverride while the terminal has focus, so the chords are in fact acted upon by
    // keyPressEvent() alone.
    menu.addSeparator();
    auto addFontSizeAction = [&](const QString& text, Qt::Key key, bool enabled) {
        QAction* action = menu.addAction(text);
        action->setShortcut(QKeySequence(FontSizeChordModifiers | key));
        action->setShortcutVisibleInContextMenu(true);
        action->setEnabled(enabled);
        return action;
    };
    QAction* increaseFontAction = addFontSizeAction(tr("Increase Font Size"), Qt::Key_Plus, _fontSizeOffset < MaxFontSizeOffset);
    QAction* decreaseFontAction = addFontSizeAction(tr("Decrease Font Size"), Qt::Key_Minus, _fontSizeOffset > MinFontSizeOffset);
    QAction* resetFontAction = addFontSizeAction(tr("Reset Font Size"), Qt::Key_0, _fontSizeOffset != 0);

    QAction* chosen = menu.exec(event->globalPos());
    if(chosen == copyAction) copySelectionToClipboard();
    else if(chosen == pasteAction) pasteFromClipboard();
    else if(chosen == increaseFontAction) increaseFontSize();
    else if(chosen == decreaseFontAction) decreaseFontSize();
    else if(chosen == resetFontAction) resetFontSize();
}

/******************************************************************************
 * Maps a Qt key event onto libvterm's key/modifier representation and feeds it to libvterm.
 ******************************************************************************/
void TerminalWidget::sendKeyEvent(QKeyEvent* event)
{
    // Any keypress while scrolled back into history jumps back to the live view,
    // matching standard terminal-emulator behavior.
    if(_scrollOffset != 0) verticalScrollBar()->setValue(verticalScrollBar()->maximum());

    int mod = qtModifiersToVterm(event->modifiers());

    switch(event->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter: vterm_keyboard_key(_vterm, VTERM_KEY_ENTER, VTermModifier(mod)); return;
        case Qt::Key_Backspace: vterm_keyboard_key(_vterm, VTERM_KEY_BACKSPACE, VTermModifier(mod)); return;
        case Qt::Key_Tab: vterm_keyboard_key(_vterm, VTERM_KEY_TAB, VTermModifier(mod)); return;
        case Qt::Key_Backtab: vterm_keyboard_key(_vterm, VTERM_KEY_TAB, VTermModifier(mod | VTERM_MOD_SHIFT)); return;
        case Qt::Key_Escape: vterm_keyboard_key(_vterm, VTERM_KEY_ESCAPE, VTermModifier(mod)); return;
        case Qt::Key_Up: vterm_keyboard_key(_vterm, VTERM_KEY_UP, VTermModifier(mod)); return;
        case Qt::Key_Down: vterm_keyboard_key(_vterm, VTERM_KEY_DOWN, VTermModifier(mod)); return;
        case Qt::Key_Left: vterm_keyboard_key(_vterm, VTERM_KEY_LEFT, VTermModifier(mod)); return;
        case Qt::Key_Right: vterm_keyboard_key(_vterm, VTERM_KEY_RIGHT, VTermModifier(mod)); return;
        case Qt::Key_Insert: vterm_keyboard_key(_vterm, VTERM_KEY_INS, VTermModifier(mod)); return;
        case Qt::Key_Delete: vterm_keyboard_key(_vterm, VTERM_KEY_DEL, VTermModifier(mod)); return;
        case Qt::Key_Home: vterm_keyboard_key(_vterm, VTERM_KEY_HOME, VTermModifier(mod)); return;
        case Qt::Key_End: vterm_keyboard_key(_vterm, VTERM_KEY_END, VTermModifier(mod)); return;
        case Qt::Key_PageUp: vterm_keyboard_key(_vterm, VTERM_KEY_PAGEUP, VTermModifier(mod)); return;
        case Qt::Key_PageDown: vterm_keyboard_key(_vterm, VTERM_KEY_PAGEDOWN, VTermModifier(mod)); return;
        default: break;
    }

    if(event->key() >= Qt::Key_F1 && event->key() <= Qt::Key_F35) {
        int n = event->key() - Qt::Key_F1 + 1;
        vterm_keyboard_key(_vterm, static_cast<VTermKey>(VTERM_KEY_FUNCTION(n)), VTermModifier(mod));
        return;
    }

    // For plain text input, rely on Qt/the OS to have already applied any Ctrl-transformation
    // to event->text() (standard behavior on all desktop platforms), so we pass VTERM_MOD_NONE
    // here rather than asking libvterm to apply its own ctrl-masking on top.
    QString text = event->text();
    if(!text.isEmpty()) {
        for(QChar ch : text) vterm_keyboard_unichar(_vterm, ch.unicode(), VTERM_MOD_NONE);
    }
    else if((mod & VTERM_MOD_CTRL) && event->key() >= Qt::Key_A && event->key() <= Qt::Key_Z) {
        // Some platforms report a bare Ctrl+letter combo with an empty text(); compute the
        // control character ourselves (Ctrl+A=0x01 .. Ctrl+Z=0x1A) in that case.
        uint32_t ctrlChar = static_cast<uint32_t>(event->key() - Qt::Key_A + 1);
        vterm_keyboard_unichar(_vterm, ctrlChar, VTERM_MOD_NONE);
    }
}

/******************************************************************************
 * Converts a viewport-pixel position into a (col, row) grid cell, clamped to the current grid.
 ******************************************************************************/
QPoint TerminalWidget::pixelToCell(const QPoint& pos) const
{
    if(_cellSize.isEmpty()) return {0, 0};
    int col = qBound(0, pos.x() / _cellSize.width(), std::max(0, _cols - 1));
    int row = qBound(0, pos.y() / _cellSize.height(), std::max(0, _rows - 1));
    return {col, row};
}

/******************************************************************************
 * Converts a viewport-pixel position into a 0-based (row, col) position in live-grid space,
 * the coordinate space libvterm's mouse-injection functions expect.
 ******************************************************************************/
QPoint TerminalWidget::pixelToLiveGridPos(const QPoint& pos) const
{
    QPoint cell = pixelToCell(pos);
    int liveRow = qBound(0, cell.y() - _scrollOffset, std::max(0, _rows - 1));
    return {liveRow, cell.x()};
}

/******************************************************************************
 * Returns the selection endpoints reordered so the first one precedes the second in reading order.
 ******************************************************************************/
std::pair<QPoint, QPoint> TerminalWidget::normalizedSelection() const
{
    QPoint start = _selAnchor;
    QPoint end = _selActive;
    if(start.y() > end.y() || (start.y() == end.y() && start.x() > end.x())) std::swap(start, end);
    return {start, end};
}

/******************************************************************************
 * If the given row intersects the current selection, returns true and sets [colBegin, colEnd)
 * to the selected column range on that row.
 ******************************************************************************/
bool TerminalWidget::selectionRangeForRow(int viewRow, int& colBegin, int& colEnd) const
{
    colBegin = colEnd = -1;
    if(!_hasSelection) return false;
    auto [start, end] = normalizedSelection();
    if(viewRow < start.y() || viewRow > end.y()) return false;
    colBegin = (viewRow == start.y()) ? start.x() : 0;
    colEnd = (viewRow == end.y()) ? end.x() + 1 : _cols;
    return colBegin < colEnd;
}

/******************************************************************************
 * Clears the current text selection, if any, and repaints the rows it used to cover.
 ******************************************************************************/
void TerminalWidget::clearSelection()
{
    if(!_hasSelection && !_selecting) return;
    auto [start, end] = normalizedSelection();
    _hasSelection = false;
    _selecting = false;
    if(!_cellSize.isEmpty())
        viewport()->update(QRect(0, start.y() * _cellSize.height(), viewport()->width(), (end.y() - start.y() + 1) * _cellSize.height()));
    else
        viewport()->update();
}

/******************************************************************************
 * Extends the selection to cover the word at the given cell.
 ******************************************************************************/
void TerminalWidget::selectWordAt(const QPoint& cell)
{
    int row = qBound(0, cell.y(), std::max(0, _rows - 1));
    auto isWordChar = [this, row](int col) -> bool {
        if(col < 0 || col >= _cols) return false;
        VTermScreenCell c = cellAtViewRow(row, col);
        if(c.chars[0] == 0 || c.chars[0] > 0xFFFF) return false;
        QChar ch(static_cast<char16_t>(c.chars[0]));
        return ch.isLetterOrNumber() || ch == QLatin1Char('_');
    };
    int col = qBound(0, cell.x(), std::max(0, _cols - 1));
    if(!isWordChar(col)) {
        clearSelection();
        return;
    }
    int begin = col;
    while(begin > 0 && isWordChar(begin - 1)) begin--;
    int end = col;
    while(end < _cols - 1 && isWordChar(end + 1)) end++;
    _selAnchor = QPoint(begin, row);
    _selActive = QPoint(end, row);
    _hasSelection = true;
    _selecting = false;
    viewport()->update();
}

/******************************************************************************
 * Extends the selection to cover the entire given row.
 ******************************************************************************/
void TerminalWidget::selectLineAt(int row)
{
    row = qBound(0, row, std::max(0, _rows - 1));
    _selAnchor = QPoint(0, row);
    _selActive = QPoint(std::max(0, _cols - 1), row);
    _hasSelection = _cols > 0;
    _selecting = false;
    viewport()->update();
}

/******************************************************************************
 * Returns the currently selected text, joining selected rows with '\n' and right-trimming each
 * line of trailing blank cells.
 ******************************************************************************/
QString TerminalWidget::selectedText() const
{
    if(!_hasSelection) return {};
    auto [start, end] = normalizedSelection();
    QStringList lines;
    for(int row = start.y(); row <= end.y(); row++) {
        int colBegin = -1;
        int colEnd = -1;
        if(!selectionRangeForRow(row, colBegin, colEnd)) continue;
        QString line;
        for(int col = colBegin; col < colEnd; col++) {
            VTermScreenCell cell = cellAtViewRow(row, col);
            int nchars = 0;
            while(nchars < VTERM_MAX_CHARS_PER_CELL && cell.chars[nchars] != 0) nchars++;
            line += nchars > 0 ? QString::fromUcs4(reinterpret_cast<const char32_t*>(cell.chars), nchars) : QStringLiteral(" ");
        }
        while(line.endsWith(QLatin1Char(' '))) line.chop(1);
        lines << line;
    }
    return lines.join(QLatin1Char('\n'));
}

/******************************************************************************
 * Copies the current selection to the system clipboard (no-op if there is no selection).
 ******************************************************************************/
void TerminalWidget::copySelectionToClipboard() const
{
    QString text = selectedText();
    if(!text.isEmpty()) QApplication::clipboard()->setText(text);
}

/******************************************************************************
 * Feeds the system clipboard's text content into the terminal session as if it had been typed,
 * wrapped in libvterm's bracketed-paste markers.
 ******************************************************************************/
void TerminalWidget::pasteFromClipboard()
{
    if(!_vterm || !isSessionActive()) return;
    QString text = QApplication::clipboard()->text();
    if(text.isEmpty()) return;
    clearSelection();

    // Normalize line breaks to '\r', which is what Enter sends in raw terminal mode (the pasted
    // text is fed through the same per-character path as regular typed input, not VTERM_KEY_ENTER).
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\r"));
    text.replace(QLatin1Char('\n'), QLatin1Char('\r'));

    vterm_keyboard_start_paste(_vterm);
    for(QChar ch : text) vterm_keyboard_unichar(_vterm, ch.unicode(), VTERM_MOD_NONE);
    vterm_keyboard_end_paste(_vterm);
}

/******************************************************************************
 * Recomputes the terminal's row/column count from the viewport size and font metrics,
 * and forwards the new size to libvterm and the backend.
 ******************************************************************************/
void TerminalWidget::updateGridSize()
{
    if(_cellSize.isEmpty()) return;
    int newCols = std::max(1, viewport()->width() / _cellSize.width());
    int newRows = std::max(1, viewport()->height() / _cellSize.height());
    if(newRows == _rows && newCols == _cols) return;
    clearSelection();
    _rows = newRows;
    _cols = newCols;
    if(_vterm) vterm_set_size(_vterm, _rows, _cols);
    if(_backend) _backend->resize(_rows, _cols);
    updateScrollBarRange();
    // Synchronous repaint (not update()): this only fires when the resize actually crosses a
    // whole-cell boundary, so it's cheap, and forcing it immediately avoids a stale/erased frame
    // slipping through during a live resize drag (see the WA_OpaquePaintEvent comment above).
    viewport()->repaint();
}

/******************************************************************************
 * Updates the vertical scrollbar's range to reflect the current scrollback size.
 ******************************************************************************/
void TerminalWidget::updateScrollBarRange()
{
    verticalScrollBar()->setRange(0, static_cast<int>(_scrollback.size()));
    verticalScrollBar()->setPageStep(_rows);
}

/******************************************************************************
 * Converts a VTermColor into a QColor, resolving indexed colors against the standard
 * xterm 256-color palette.
 ******************************************************************************/
QColor TerminalWidget::cellColor(const VTermColor& colorIn, bool isBackground) const
{
    if(isBackground && VTERM_COLOR_IS_DEFAULT_BG(&colorIn)) return viewport()->palette().color(QPalette::Window);
    if(!isBackground && VTERM_COLOR_IS_DEFAULT_FG(&colorIn)) return QColor(220, 220, 220);

    VTermColor color = colorIn;
    vterm_screen_convert_color_to_rgb(_screen, &color);
    return QColor(color.rgb.red, color.rgb.green, color.rgb.blue);
}

/******************************************************************************
 * Handles TerminalBackend::dataReceived(): feeds bytes into libvterm.
 ******************************************************************************/
void TerminalWidget::onBackendDataReceived(const QByteArray& data)
{
    if(_vterm) vterm_input_write(_vterm, data.constData(), static_cast<size_t>(data.size()));
}

/******************************************************************************
 * Handles TerminalBackend::processExited(): tears down and re-emits processFinished().
 ******************************************************************************/
void TerminalWidget::onBackendProcessExited(int exitCode)
{
    _sessionEnded = true;
    clearSelection();

    // Wipe the grid and scrollback so the tab returns to a blank slate the next time a session
    // is started, instead of leaving the previous session's final screen content visible.
    // Hiding the cursor too, since vterm_screen_reset() doesn't fire the movecursor callback that would
    // otherwise clear the stale cursor cell drawn at the old (pre-reset) position.
    _scrollback.clear();
    _scrollOffset = 0;
    if(_screen) vterm_screen_reset(_screen, 1);
    // vterm_screen_reset() synchronously re-initializes terminal properties, including firing
    // VTERM_PROP_CURSORVISIBLE=1 via cb_settermprop/handleSetTermProp() -- which would otherwise
    // clobber a "hide the cursor" override applied beforehand. Apply it after the reset instead,
    // so it's the assignment that actually sticks.
    _cursorPos = VTermPos{0, 0};
    _cursorVisible = false;
    // vterm_screen_reset() clears libvterm's internal mouse-tracking state directly, bypassing
    // settermprop/handleSetTermProp(), so the cached mode here would otherwise go stale and keep
    // routing clicks away from local selection after a mouse-reporting app exits.
    _mouseReportMode = VTERM_PROP_MOUSE_NONE;
    updateScrollBarRange();
    viewport()->update();

    Q_EMIT processFinished(exitCode);
}

/******************************************************************************
 * Handles TerminalBackend::errorOccurred(): surfaces a fatal I/O error.
 ******************************************************************************/
void TerminalWidget::onBackendError(const QString& message)
{
    _errorMessage = message;
    viewport()->update();
}

/******************************************************************************
 * Handles scrollbar value changes triggered by the user (scrollback navigation).
 ******************************************************************************/
void TerminalWidget::onScrollBarValueChanged(int value)
{
    clearSelection();
    _scrollOffset = verticalScrollBar()->maximum() - value;
    viewport()->update();
}

/******************************************************************************
 * libvterm screen callback trampoline: forwards damage notifications.
 ******************************************************************************/
int TerminalWidget::cb_damage(VTermRect rect, void* user)
{
    static_cast<TerminalWidget*>(user)->handleDamage(rect);
    return 1;
}

/******************************************************************************
 * libvterm screen callback trampoline: a block of the screen was moved (e.g. scrolled).
 ******************************************************************************/
int TerminalWidget::cb_moverect(VTermRect dest, VTermRect src, void* user)
{
    Q_UNUSED(dest);
    Q_UNUSED(src);
    // A full repaint is now cheap (paintGridRow() batches same-style runs instead of drawing
    // cell by cell), so this doesn't bother detecting the common uniform-vertical-scroll case
    // and re-blitting via QWidget::scroll() -- that optimization was considered and deferred;
    // revisit only if profiling scroll-heavy workloads still shows a bottleneck here.
    static_cast<TerminalWidget*>(user)->viewport()->update();
    return 1;
}

/******************************************************************************
 * libvterm screen callback trampoline: the cursor moved.
 ******************************************************************************/
int TerminalWidget::cb_movecursor(VTermPos pos, VTermPos oldPos, int visible, void* user)
{
    static_cast<TerminalWidget*>(user)->handleMoveCursor(pos, oldPos, visible != 0);
    return 1;
}

/******************************************************************************
 * libvterm screen callback trampoline: a terminal property changed (title, cursor visibility, ...).
 ******************************************************************************/
int TerminalWidget::cb_settermprop(VTermProp prop, VTermValue* val, void* user)
{
    static_cast<TerminalWidget*>(user)->handleSetTermProp(prop, *val);
    return 1;
}

/******************************************************************************
 * libvterm screen callback trampoline: the terminal bell was triggered.
 ******************************************************************************/
int TerminalWidget::cb_bell(void* user)
{
    static_cast<TerminalWidget*>(user)->handleBell();
    return 1;
}

/******************************************************************************
 * libvterm screen callback trampoline: a line scrolled off the top of the live screen.
 ******************************************************************************/
int TerminalWidget::cb_sb_pushline(int cols, const VTermScreenCell* cells, void* user)
{
    static_cast<TerminalWidget*>(user)->pushScrollbackLine(cols, cells);
    return 1;
}

/******************************************************************************
 * libvterm screen callback trampoline: a line is being scrolled back onto the live screen.
 ******************************************************************************/
int TerminalWidget::cb_sb_popline(int cols, VTermScreenCell* cells, void* user)
{
    return static_cast<TerminalWidget*>(user)->popScrollbackLine(cols, cells);
}

/******************************************************************************
 * libvterm output callback trampoline: forwards already VT100-encoded bytes to the backend.
 ******************************************************************************/
void TerminalWidget::cb_output(const char* s, size_t len, void* user)
{
    TerminalWidget* self = static_cast<TerminalWidget*>(user);
    if(self->_backend) self->_backend->write(s, static_cast<qsizetype>(len));
}

/******************************************************************************
 * Repaints the region affected by a damage notification.
 ******************************************************************************/
void TerminalWidget::handleDamage(const VTermRect& rect)
{
    if(_scrollOffset != 0 || _cellSize.isEmpty()) {
        viewport()->update();
        return;
    }
    QRect pixelRect(rect.start_col * _cellSize.width(),
                    rect.start_row * _cellSize.height(),
                    (rect.end_col - rect.start_col) * _cellSize.width(),
                    (rect.end_row - rect.start_row) * _cellSize.height());
    viewport()->update(pixelRect);
}

/******************************************************************************
 * Updates the cached cursor position/visibility and repaints the affected cells.
 ******************************************************************************/
void TerminalWidget::handleMoveCursor(VTermPos pos, VTermPos oldPos, bool visible)
{
    _cursorPos = pos;
    _cursorVisible = visible;
    if(_scrollOffset == 0 && !_cellSize.isEmpty()) {
        viewport()->update(QRect(oldPos.col * _cellSize.width(), oldPos.row * _cellSize.height(), _cellSize.width(), _cellSize.height()));
        viewport()->update(QRect(pos.col * _cellSize.width(), pos.row * _cellSize.height(), _cellSize.width(), _cellSize.height()));
    }
}

/******************************************************************************
 * Handles a terminal property change (title, cursor visibility, ...).
 ******************************************************************************/
void TerminalWidget::handleSetTermProp(VTermProp prop, const VTermValue& val)
{
    switch(prop) {
        case VTERM_PROP_TITLE:
            if(val.string.initial) _pendingTitle.clear();
            _pendingTitle += QString::fromUtf8(val.string.str, static_cast<int>(val.string.len));
            if(val.string.final) Q_EMIT titleChanged(_pendingTitle);
            break;
        case VTERM_PROP_CURSORVISIBLE:
            _cursorVisible = val.boolean;
            viewport()->update();
            break;
        case VTERM_PROP_MOUSE:
            _mouseReportMode = val.number;
            // The app now owns clicks going forward; an in-progress local drag-selection would
            // otherwise linger on screen despite no longer being extendable by further drags.
            if(_mouseReportMode != VTERM_PROP_MOUSE_NONE && (_selecting || _hasSelection))
                clearSelection();
            break;
        default: break;
    }
}

/******************************************************************************
 * Shows a brief visual flash in response to the terminal bell.
 ******************************************************************************/
void TerminalWidget::handleBell()
{
    QWidget* vp = viewport();
    QColor original = vp->palette().color(QPalette::Window);
    QPalette flashPalette = vp->palette();
    flashPalette.setColor(QPalette::Window, original.lighter(200));
    vp->setPalette(flashPalette);
    vp->update();
    QTimer::singleShot(80, this, [vp, original]() {
        QPalette pal = vp->palette();
        pal.setColor(QPalette::Window, original);
        vp->setPalette(pal);
        vp->update();
    });
}

/******************************************************************************
 * Stores a line that scrolled off the top of the live screen into the scrollback buffer.
 ******************************************************************************/
void TerminalWidget::pushScrollbackLine(int cols, const VTermScreenCell* cells)
{
    // The live grid rows shift up by one as a result, which would silently misalign an existing
    // selection's view-row coordinates with what's actually on screen -- and this can happen
    // without the scrollbar's value ever changing (once scrollback is capped at MaxScrollbackLines,
    // pushing and popping balance out and updateScrollBarRange()'s range stops moving), so this
    // can't rely solely on onScrollBarValueChanged() to invalidate the selection.
    clearSelection();

    QVector<VTermScreenCell> line(cols);
    std::copy(cells, cells + cols, line.begin());
    _scrollback.push_front(std::move(line));
    if(static_cast<int>(_scrollback.size()) > MaxScrollbackLines) _scrollback.pop_back();

    bool wasAtBottom = (_scrollOffset == 0);
    int oldMax = verticalScrollBar()->maximum();
    updateScrollBarRange();
    int newMax = verticalScrollBar()->maximum();
    if(wasAtBottom)
        verticalScrollBar()->setValue(newMax);
    else
        verticalScrollBar()->setValue(verticalScrollBar()->value() + (newMax - oldMax));
}

/******************************************************************************
 * Retrieves the most recent scrollback line so libvterm can scroll it back onto the live screen.
 ******************************************************************************/
int TerminalWidget::popScrollbackLine(int cols, VTermScreenCell* cells)
{
    if(_scrollback.empty()) return 0;
    clearSelection();
    const QVector<VTermScreenCell>& line = _scrollback.front();
    int n = std::min(cols, static_cast<int>(line.size()));
    std::copy(line.begin(), line.begin() + n, cells);
    for(int i = n; i < cols; i++) cells[i] = VTermScreenCell{};
    _scrollback.pop_front();
    updateScrollBarRange();
    return 1;
}

}  // namespace Ovito
