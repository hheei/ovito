// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/oo/OORef.h>
#include <ovito/core/oo/RefTarget.h>

namespace Ovito {

/**
 * \brief This undo record simply generates a TargetChanged event for a RefTarget whenever an operation is undone.
 */
class OVITO_CORE_EXPORT TargetChangedUndoOperation : public UndoableOperation
{
public:

    /// \brief Constructor.
    /// \param target The object that is being changed.
    TargetChangedUndoOperation(RefTarget* target) : _target(target) {}

    virtual void undo() override;
    virtual void redo() override {}

    virtual QString displayName() const override {
        return QStringLiteral("Target changed undo operation");
    }

private:

    /// The object that has been changed.
    OORef<RefTarget> _target;
};

/**
 * \brief This undo record simply generates a TargetChanged event for a RefTarget whenever an operation is redone.
 */
class OVITO_CORE_EXPORT TargetChangedRedoOperation : public UndoableOperation
{
public:

    /// \brief Constructor.
    /// \param target The object that is being changed.
    TargetChangedRedoOperation(RefTarget* target) : _target(target) {}

    virtual void undo() override {}
    virtual void redo() override;

    virtual QString displayName() const override {
        return QStringLiteral("Target changed redo operation");
    }

private:

    /// The object that has been changed.
    OORef<RefTarget> _target;
};

}   // End of namespace
