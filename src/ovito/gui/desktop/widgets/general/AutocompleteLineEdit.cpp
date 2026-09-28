// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include "AutocompleteLineEdit.h"
#include "AutocompleteEdit.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
AutocompleteLineEdit::AutocompleteLineEdit(QWidget* parent) : EnterLineEdit(parent), _wordSplitter(AutocompleteEdit::wordSplitterExpression)
{
    _wordListModel = new QStringListModel(this);
    _completer = new QCompleter(this);
    _completer->setCompletionMode(QCompleter::PopupCompletion);
    _completer->setCaseSensitivity(Qt::CaseInsensitive);
    _completer->setModel(_wordListModel);
    _completer->setWidget(this);
    connect(_completer, qOverload<const QString&>(&QCompleter::activated), this, &AutocompleteLineEdit::onComplete);
}

/******************************************************************************
* Inserts a complete word into the text field.
******************************************************************************/
void AutocompleteLineEdit::onComplete(const QString& completion)
{
    const auto& [newCursorPos, newText] = AutocompleteEdit::completeExpression(cursorPosition(), text(), _wordSplitter, completion);

    // Set new text
    setText(newText);

    // Set new position
    setCursorPosition((int)newCursorPos);
}

/******************************************************************************
* Handles key-press events.
******************************************************************************/
void AutocompleteLineEdit::keyPressEvent(QKeyEvent* event)
{
    if(_completer->popup()->isVisible()) {
        if(event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return ||
                event->key() == Qt::Key_Escape || event->key() == Qt::Key_Tab) {
                    event->ignore();
                    return;
        }
    }

    QLineEdit::keyPressEvent(event);

    const auto [start, length] = AutocompleteEdit::getToken(cursorPosition(), text(), _wordSplitter);
    QString completionPrefix = text().mid(start, length);

    if(completionPrefix != _completer->completionPrefix()) {
        _completer->setCompletionPrefix(completionPrefix);
        _completer->popup()->setCurrentIndex(_completer->completionModel()->index(0,0));
    }
    if(!completionPrefix.isEmpty() && !_wordListModel->stringList().contains(completionPrefix)
            && !AutocompleteEdit::isInsideRegexPattern(cursorPosition(), text()))
        _completer->complete();
    else
        _completer->popup()->hide();
}

}   // End of namespace
