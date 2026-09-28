// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

/**
 * \file AutocompleteLineEdit.h
 * \brief Contains the definition of the Ovito::AutocompleteLineEdit class.
 */

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/widgets/general/EnterLineEdit.h>

namespace Ovito {

/**
 * \brief A text editor widget that provides auto-completion of words.
 */
class OVITO_GUI_EXPORT AutocompleteLineEdit : public EnterLineEdit
{
    Q_OBJECT

public:

    /// Constructor.
    explicit AutocompleteLineEdit(QWidget* parent = nullptr);

    /// Sets the list of words that can be completed.
    void setWordList(const QStringList& words) { _wordListModel->setStringList(words); }

protected Q_SLOTS:

    /// Inserts a complete word into the text field.
    void onComplete(const QString& completion);

protected:

    /// Handles key-press events.
    virtual void keyPressEvent(QKeyEvent* event) override;

    /// Get the current token from the text string.
    /// This will fail for strings with nested quotes!
    // Returns the starting index and the length of the token from the original string
    std::tuple<qsizetype, qsizetype> getToken() const;

protected:
    /// The completer object used by the widget.
    QCompleter* _completer;

    /// The list model storing the words that can be completed.
    QStringListModel* _wordListModel;

    /// Regular expression used to split a text into words.
    QRegularExpression _wordSplitter;
};

}   // End of namespace
