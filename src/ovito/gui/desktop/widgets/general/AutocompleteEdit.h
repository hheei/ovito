// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/gui/desktop/GUI.h>

namespace Ovito::AutocompleteEdit {

constexpr static char wordSplitterExpression[] = R"([0-9a-zA-Z\.@\[\]])";

/// Get the current token from the text string.
/// This will fail for strings with nested quotes!
/// Returns the starting index and the length of the token from the original string.
OVITO_GUI_EXPORT std::tuple<qsizetype, qsizetype> getToken(int curserPosition, const QString& expression, const QRegularExpression& splitExpression);

/// Calculates the new cursor position and the new string after successful insertion of completion into expression at the current position.
OVITO_GUI_EXPORT std::tuple<qsizetype, QString> completeExpression(int curserPosition, const QString& expression, const QRegularExpression& splitExpression,
                                                  const QString& completion);

/// Returns true if the cursor is currently positioned inside a regex pattern literal /.../.
OVITO_GUI_EXPORT bool isInsideRegexPattern(int cursorPosition, const QString& expression);

}  // namespace Ovito::AutocompleteEdit
