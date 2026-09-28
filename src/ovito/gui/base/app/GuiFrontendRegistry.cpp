// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/base/GUIBase.h>
#include "GuiFrontendRegistry.h"

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

}   // End of namespace
