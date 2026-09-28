// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * \brief A text editor widget that provides auto-completion of words.
 */
class OVITO_GUI_EXPORT AutocompleteTextEdit : public QPlainTextEdit
{
    Q_OBJECT

public:

    /// \brief Constructs the widget.
    AutocompleteTextEdit(QWidget* parent = nullptr);

    /// Sets the list of words that can be completed.
    void setWordList(const QStringList& words) { _wordListModel->setStringList(words); }

    /// Returns the preferred size of the widget.
    virtual QSize sizeHint() const override;

    /// Sets whether the editingFinished() signal is emitted when the return key is pressed.
    void setCommitOnReturn(bool on) { _commitOnReturn = on; }

    /// Returns whether the editingFinished() signal is emitted when the return key is pressed.
    bool commitOnReturn() const { return _commitOnReturn; }

Q_SIGNALS:

    /// This signal is emitted when the Return or Enter key is pressed or the widget loses focus.
    void editingFinished();

protected Q_SLOTS:

    /// Inserts a complete word into the text field.
    void onComplete(const QString& completion);

protected:

    /// Handles key-press events.
    virtual void keyPressEvent(QKeyEvent* event) override;

    /// Handles keyboard focus lost events.
    virtual void focusOutEvent(QFocusEvent* event) override;

protected:

    /// The completer object used by the widget.
    QCompleter* _completer;

    /// The list model storing the words that can be completed.
    QStringListModel* _wordListModel;

    /// Regular expression used to split a text into words.
    QRegularExpression _wordSplitter;

    /// Controls whether the editingFinished() signal is emitted when the return key is pressed.
    bool _commitOnReturn = true;
};

}   // End of namespace
