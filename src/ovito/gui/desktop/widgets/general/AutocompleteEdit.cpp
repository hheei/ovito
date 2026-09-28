// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>

namespace Ovito::AutocompleteEdit {

/******************************************************************************
 * Get the current token from the text string.
 * This will fail for strings with nested quotes.
 * Returns the starting index and the length of the token from the original string
 ******************************************************************************/
std::tuple<qsizetype, qsizetype> getToken(int curserPosition, const QString& expression, const QRegularExpression& splitExpression)
{
    OVITO_ASSERT(curserPosition <= expression.size());

    // '\0' used to delimit no (open) quote char
    QChar currentQuote = QChar::Null;

    // Find out what is the most recently opened quote char
    for(qsizetype i = 0; i < curserPosition; ++i) {
        if(expression[i] == '\'' || expression[i] == '\"') {
            currentQuote = (expression[i] == currentQuote) ? QChar::Null : expression[i];
        }
    }

    qsizetype start = 0;
    qsizetype end = expression.size();
    if(currentQuote != QChar::Null) {
        // Go through the expression from pos (exclusive) to 0
        // Determine the first currentQuote position
        for(qsizetype i = curserPosition - 1; i >= 0; --i) {
            if(expression[i] == currentQuote) {
                start = i;
                break;
            }
        }
        // Go through the expression from pos (inclusive) to the end
        // Determine the first currentQuote position
        for(qsizetype i = curserPosition; i < expression.size(); ++i) {
            if(expression[i] == currentQuote) {
                end = i;
                break;
            }
        }
    }
    else {
        OVITO_ASSERT(splitExpression.isValid());
        // Go through the expression from pos (exclusive) to 0
        // Determine the character that matches _wordSplitter
        for(qsizetype i = curserPosition - 1; i >= 0; --i) {
            if(!splitExpression.match(expression[i]).hasMatch()) {
                start = i + 1;
                break;
            }
        }
        // Go through the expression from pos (inclusive) to the end
        // Determine the character that matches _wordSplitter
        for(qsizetype i = curserPosition; i < expression.size(); ++i) {
            if(!splitExpression.match(expression[i]).hasMatch()) {
                end = i - 1;
                break;
            }
        }
    }
    OVITO_ASSERT(start >= 0 && end <= expression.size());
    // Return the text segment position
    return {start, end - start + 1};
}

/******************************************************************************
 * Calculates the new cursor position and the new string after successful insertion of completion into expression at the current position.
 ******************************************************************************/
std::tuple<qsizetype, QString> completeExpression(int curserPosition, const QString& expression, const QRegularExpression& splitExpression,
                                                  const QString& completion)
{
    // Get current token
    const auto [start, length] = getToken(curserPosition, expression, splitExpression);

    // Assemble new text
    QString newExpression = expression.first(start) + completion + expression.mid(start + length, expression.size() - (start + length));

    return {start + completion.size(), newExpression};
}

/******************************************************************************
 * Returns true if the cursor is positioned inside a regex pattern literal /.../
 * following a =~ operator.
 ******************************************************************************/
bool isInsideRegexPattern(int cursorPosition, const QString& expression)
{
    bool insideRegex = false;
    bool insideSingleQuote = false;
    bool insideDoubleQuote = false;

    for(int i = 0; i < cursorPosition && i < expression.size(); ++i) {
        const QChar ch = expression[i];
        if(insideRegex) {
            if(ch == '/')
                insideRegex = false;
        }
        else if(insideSingleQuote) {
            if(ch == '\'')
                insideSingleQuote = false;
        }
        else if(insideDoubleQuote) {
            if(ch == '"')
                insideDoubleQuote = false;
        }
        else {
            if(ch == '\'')
                insideSingleQuote = true;
            else if(ch == '"')
                insideDoubleQuote = true;
            else if(ch == '/') {
                // A '/' opens a regex only when preceded (ignoring whitespace) by '=~'.
                int j = i - 1;
                while(j >= 0 && expression[j].isSpace())
                    --j;
                if(j >= 1 && expression[j] == '~' && expression[j-1] == '=')
                    insideRegex = true;
            }
        }
    }
    return insideRegex;
}

}  // namespace Ovito::AutocompleteEdit
