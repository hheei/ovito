// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/base/GUIBase.h>
#include "GuiFrontendRegistry.h"

#include <numeric>
#include <vector>

namespace Ovito {

/******************************************************************************
* Returns the global registry instance.
******************************************************************************/
GuiFrontendRegistry& GuiFrontendRegistry::instance()
{
    static GuiFrontendRegistry registry;
    return registry;
}

/******************************************************************************
* Registers a frontend, replacing any frontend previously registered under the same name.
******************************************************************************/
void GuiFrontendRegistry::registerFrontend(std::unique_ptr<GuiFrontend> frontend)
{
    OVITO_ASSERT(frontend);
    OVITO_ASSERT(this_task::isMainThread());

    const QString name = frontend->name();
    OVITO_ASSERT(!name.isEmpty());
    _frontends.insert_or_assign(name, std::move(frontend));
}

/******************************************************************************
* Returns the frontend registered under the given name, or null if there is no such frontend.
******************************************************************************/
const GuiFrontend* GuiFrontendRegistry::findFrontend(const QString& name) const
{
    auto iter = _frontends.find(name);
    return iter != _frontends.end() ? iter->second.get() : nullptr;
}

/******************************************************************************
* Returns the names of all registered frontends, in alphabetical order.
******************************************************************************/
QStringList GuiFrontendRegistry::frontendNames() const
{
    QStringList names;
    for(const auto& [name, frontend] : _frontends)
        names.push_back(name);
    return names;
}

/******************************************************************************
* Returns the registered frontends and their descriptions, ready to be shown to the user.
******************************************************************************/
QString GuiFrontendRegistry::frontendList() const
{
    QStringList lines;
    for(const auto& [name, frontend] : _frontends) {
        const QString description = frontend->description();
        lines.push_back(description.isEmpty() ? name : QStringLiteral("%1 - %2").arg(name, description));
    }
    return lines.join(QStringLiteral("\n"));
}

/******************************************************************************
* Returns the name of the registered frontend that resembles the given name most, or an empty string if none does.
******************************************************************************/
QString GuiFrontendRegistry::suggestFrontendName(const QString& name) const
{
    if(name.isEmpty())
        return {};

    // The edit distance between two names, so that a typo like "qt-widegts" is still recognized.
    const auto editDistance = [](const QString& a, const QString& b) {
        std::vector<int> previous(b.size() + 1), current(b.size() + 1);
        std::iota(previous.begin(), previous.end(), 0);
        for(qsizetype i = 0; i < a.size(); i++) {
            current[0] = int(i) + 1;
            for(qsizetype j = 0; j < b.size(); j++) {
                const int substitution = previous[j] + (a[i].toLower() == b[j].toLower() ? 0 : 1);
                current[j + 1] = std::min({ previous[j + 1] + 1, current[j] + 1, substitution });
            }
            previous.swap(current);
        }
        return previous.back();
    };

    QString best;
    int bestDistance = 3;
    for(const auto& [registeredName, frontend] : _frontends) {
        const int distance = editDistance(name, registeredName);
        if(distance < bestDistance) {
            bestDistance = distance;
            best = registeredName;
        }
    }

    // A name that is a strict prefix of exactly one registered name ("qt" for "qt-widgets") is a stronger hint than a
    // small edit distance, which can point at an unrelated name ("qt" is two edits away from "qml" as well).
    QString prefixMatch;
    for(const auto& [registeredName, frontend] : _frontends) {
        if(registeredName.startsWith(name, Qt::CaseInsensitive)) {
            if(!prefixMatch.isEmpty())
                return bestDistance <= 2 ? best : QString{};   // Ambiguous: two names start with the given text.
            prefixMatch = registeredName;
        }
    }
    if(!prefixMatch.isEmpty())
        return prefixMatch;
    return bestDistance <= 2 ? best : QString{};
}

}   // End of namespace
