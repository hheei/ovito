// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/app/UserInterface.h>
#include "UndoStack.h"
#include "UndoableOperation.h"
#include "RefTargetOperations.h"

namespace Ovito {

/******************************************************************************
* Returns the currently active compound operation (either while recording
* operations or while undoing/redoing an operation).
******************************************************************************/
CompoundOperation*& CompoundOperation::current()
{
    // The active operation in the current thread.
    static thread_local CompoundOperation* _current = nullptr;

    return _current;
}

/******************************************************************************
* Indicates whether undo recording is currently active.
******************************************************************************/
bool CompoundOperation::isUndoRecording()
{
    if(CompoundOperation* op = CompoundOperation::current())
        return !op->_isUndoingOrRedoing;
    return false;
}

/******************************************************************************
* Indicates whether previously recorded operations are currently
* being undo or redone.
******************************************************************************/
bool CompoundOperation::isUndoingOrRedoing()
{
    if(CompoundOperation* op = CompoundOperation::current())
        return op->_isUndoingOrRedoing;
    return false;
}

/******************************************************************************
* Commits the recorded operations by placing them on the undo stack.
******************************************************************************/
void UndoableTransaction::commit()
{
    OVITO_ASSERT(_operation);

    if(_operation->isSignificant()) {
        if(CompoundOperation* parent = CompoundOperation::current()) {
            OVITO_ASSERT(parent != _operation.get());
            parent->addOperation(std::move(_operation));
        }
        else {
            if(UndoStack* undoStack = userInterface().undoStack()) {
                OVITO_ASSERT(QThread::currentThread() == undoStack->thread());
                undoStack->push(std::move(_operation));
            }
        }
    }
    _operation.reset();
    _userInterface.reset();
}

/******************************************************************************
* Undo all actions recorded so far and keep the current transaction open.
******************************************************************************/
bool UndoableTransaction::revert()
{
    OVITO_ASSERT(_operation);
    OVITO_ASSERT(_userInterface);

    return userInterface().handleExceptions([&] {
        _operation->undo();
        _operation->clear();
    });
}

/******************************************************************************
* Undo all actions recorded after the given snapshot and keep the current transaction open.
******************************************************************************/
bool UndoableTransaction::revertTo(int snapshot)
{
    OVITO_ASSERT(_operation);
    OVITO_ASSERT(_userInterface);

    return userInterface().handleExceptions([&] {
        _operation->revertTo(snapshot);
        OVITO_ASSERT(_operation->count() == snapshot);
    });
}

/******************************************************************************
* Undo all actions recorded so far and close the current transaction.
******************************************************************************/
bool UndoableTransaction::cancel()
{
    OVITO_ASSERT(_operation);

    bool success = true;
    if(_operation->isSignificant()) {
        // Perform the undo operations in an isolated transaction.
        success = userInterface().handleExceptions<true>([&] {
            _operation->undo();
        });
    }
    _operation.reset();
    _userInterface.reset();

    return success;
}

/******************************************************************************
* Undo the compound edit operation that was made.
******************************************************************************/
void CompoundOperation::undo()
{
    OVITO_ASSERT(!_isUndoingOrRedoing);
    if(!isSignificant())
        return;

    UndoSuspender u(this);
    _isUndoingOrRedoing = true;
    try {
        for(int i = (int)_subOperations.size() - 1; i >= 0; --i) {
            OVITO_CHECK_POINTER(_subOperations[i]);
            _subOperations[i]->undo();
        }
        _isUndoingOrRedoing = false;
    }
    catch(...) {
        _isUndoingOrRedoing = false;
        throw;
    }
}

/******************************************************************************
* Re-apply the compound change, assuming that it has been undone.
******************************************************************************/
void CompoundOperation::redo()
{
    OVITO_ASSERT(!_isUndoingOrRedoing);
    if(!isSignificant())
        return;

    UndoSuspender u(this);
    _isUndoingOrRedoing = true;
    try {
        for(const auto& op : _subOperations) {
            OVITO_CHECK_POINTER(op.get());
            op->redo();
        }
        _isUndoingOrRedoing = false;
    }
    catch(...) {
        _isUndoingOrRedoing = false;
        throw;
    }
}

/******************************************************************************
* Undo all operations up to the given position and the remove those operations from the container.
******************************************************************************/
void CompoundOperation::revertTo(int position)
{
    OVITO_ASSERT(!_isUndoingOrRedoing);
    OVITO_ASSERT(position >= 0);
    OVITO_ASSERT(position <= count());

    UndoSuspender u(this);
    _isUndoingOrRedoing = true;
    try {
        for(int i = (int)_subOperations.size() - 1; i >= position; --i) {
            OVITO_CHECK_POINTER(_subOperations[i]);
            _subOperations[i]->undo();
        }
        _subOperations.resize(position);
        _isUndoingOrRedoing = false;
    }
    catch(...) {
        _isUndoingOrRedoing = false;
        throw;
    }
}

/******************************************************************************
* Prints a text representation of the compound operation to the console.
* This is for debugging purposes only.
******************************************************************************/
void CompoundOperation::debugPrint(int level)
{
    int index = 0;
    for(const auto& op : _subOperations) {
        UndoableOperation& opRef = *op;
        qDebug() << QByteArray(level*2, ' ').constData() << index << ":" << qPrintable(op->displayName()) << "(" << typeid(opRef).name() << ")";
        if(CompoundOperation* compOp = dynamic_cast<CompoundOperation*>(op.get())) {
            compOp->debugPrint(level+1);
        }
        index++;
    }
}

/******************************************************************************
* Is called to undo an operation.
******************************************************************************/
void TargetChangedUndoOperation::undo()
{
    _target->notifyTargetChanged();
}

/******************************************************************************
* Is called to redo an operation.
******************************************************************************/
void TargetChangedRedoOperation::redo()
{
    _target->notifyTargetChanged();
}

}   // End of namespace
