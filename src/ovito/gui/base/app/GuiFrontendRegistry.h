// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/base/GUIBase.h>
#include "GuiFrontend.h"

namespace Ovito {

/**
 * \brief The list of the graphical user interface frontends that are available in this process.
 *
 * The registry is populated by the frontends themselves during the application initialization (see GuiFrontend) and is
 * queried when the application starts the frontend the user selected with the \c --gui command line option.
 */
class OVITO_GUIBASE_EXPORT GuiFrontendRegistry
{
public:

    /// The registry is a process-global singleton and owns the frontends; it must not be copied. Declaring this
    /// explicitly also keeps MSVC from instantiating the implicitly declared copy constructor, which cannot work
    /// with the map of unique_ptrs the registry holds.
    GuiFrontendRegistry() = default;
    GuiFrontendRegistry(const GuiFrontendRegistry&) = delete;
    GuiFrontendRegistry& operator=(const GuiFrontendRegistry&) = delete;

    /// Returns the global registry instance.
    static GuiFrontendRegistry& instance();

    /// Registers a frontend, replacing any frontend previously registered under the same name.
    void registerFrontend(std::unique_ptr<GuiFrontend> frontend);

    /// Returns the frontend registered under the given name, or null if there is no such frontend.
    const GuiFrontend* findFrontend(const QString& name) const;

    /// Returns the names of all registered frontends, in alphabetical order.
    QStringList frontendNames() const;

    /// Returns the registered frontends and their descriptions, ready to be shown to the user.
    QString frontendList() const;

    /**
     * Returns the name of the registered frontend that resembles \a name most, or an empty string if none does.
     *
     * A user who mistypes the name (`--gui=qm`) should not have to compare a list of names character by character. The
     * match is deliberately conservative - a name counts as "meant" only if it differs in at most two characters, or if
     * the typo is a prefix of it and no other name is - because a wrong suggestion is worse than none.
     */
    QString suggestFrontendName(const QString& name) const;

private:

    /// The registered frontends, ordered by name.
    std::map<QString, std::unique_ptr<GuiFrontend>> _frontends;
};

}   // End of namespace
