// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/utilities/concurrent/MainThreadOperation.h>

namespace Ovito {

/**
 * \brief Opens a task context for work that a callback of the presentation layer starts.
 *
 * Most of OVITO's core - creating objects, preparing the scene, reading the current animation time, changing properties
 * through the undo system - expects to run inside a task that knows the UserInterface to report progress and errors to.
 * The presentation layer, however, runs callbacks of the windowing toolkit: a Qt signal handler, a QML file that is
 * being instantiated, or a timer. Those have no task context, and code that creates an object from such a callback -
 * an interactive SceneRenderer, for instance - fails or crashes without one.
 *
 * Such a callback therefore opens a task context for the duration of its work:
 *
 * \code
 * void SomeWidget::somethingHappened() {
 *     GuiTaskScope taskScope(ui());
 *     ...     // core calls that need a task context
 * }
 * \endcode
 *
 * The scope is bound to the task that is current when the callback runs, if there is one, so nesting it inside an
 * operation that already has a context is harmless.
 *
 * The task of the scope can be handed to a cancellation UI (see task()), so that the user can abort the operation while
 * it is running - which is only useful if the operation is long enough to be interrupted, so it is up to the caller to
 * offer that.
 *
 * See item O8 in docs/design/UI_PHASE0_AUDIT.md.
 */
class OVITO_GUIBASE_EXPORT GuiTaskScope
{
public:

    /// Opens a task context for the given user interface.
    explicit GuiTaskScope(UserInterface& userInterface, bool isInteractive = true);

    /// Closes the task context. Defined out of line, because MSVC needs the symbol of a class that is imported from
    /// another DLL even when it would be trivial to generate it in every translation unit (see defect F13).
    ~GuiTaskScope();

    /// Returns the task that provides the context. Cancelling it cancels the work the callback started.
    const TaskPtr& task() const { return _operation.task(); }

private:

    /// The main-thread operation that provides the task context.
    MainThreadOperation _operation;
};

}   // End of namespace
