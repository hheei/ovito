// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/qml/spike/SpikeHarness.h>
#include <ovito/gui/qml/models/QmlAnimationModel.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/gui/base/actions/Command.h>
#include <ovito/gui/base/app/GuiTaskScope.h>
#include <ovito/core/app/undo/UndoStack.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/animation/controller/AnimationKeys.h>
#include <ovito/core/dataset/animation/controller/KeyframeController.h>
#include <ovito/core/dataset/animation/controller/PRSTransformationController.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/SceneNode.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/core/oo/PropertyFieldDescriptor.h>

namespace Ovito::Spike {

/******************************************************************************
* Verifies the model side of the animation timeline: the animation state, the keys of the selected objects and the
* operations a timeline performs on them.
*
* This is the check of Phase 3 slice S2 (deliverable 2 of the plan). It verifies the *model API* and not a timeline
* widget - the timeline is Phase 5 (audit decision D53) - so everything it touches is what such a widget binds to:
*
*   * the animation interval, the current time, the frame<->time conversions of the animation settings, and that
*     changing the interval is one undo step;
*   * the tracks: the animated parameters of the selected objects, found through the object graph the way the classic
*     track bar finds them (each key is a row of the model, with the parameter, its frame, its time and its value);
*   * the key selection, which is model state here instead of widget-local state (D56);
*   * the two forms of a key move - the discrete one (one undo step per call) and the continuous one of a drag, which
*     owns one transaction from beginKeyMove() to commitKeyMove() and restores the positions it started from when the
*     gesture is cancelled (D58) - including the clamping to the animation interval;
*   * the deletion of keys and its undo, which returns the keys but not the selection (the classic track bar behaves the
*     same way: the selection of the keys that were deleted is gone);
*   * the visible range as presentation state that follows the interval until it is set explicitly;
*   * the playback state, which is not this model's state but core's SceneAnimationPlayback (D56), and the playback
*     settings, which are undoable data-set edits.
*
* The check imports a data set of its own and edits the animation of its scene node, so it belongs after the checks that
* require the scene they leave behind and before the one that replaces the data set (see the option table in Main.cpp).
*
* Objects are created, transactions are opened and keys are made, so the phases run inside a GuiTaskScope. Between them
* the check waits, because the key rows follow the notifications of the controllers and the current frame is applied to
* the scene asynchronously.
******************************************************************************/

namespace {

/// The state the phases of the check share.
struct AnimationCheckState
{
    /// The frontend under test and the continuation that lets the next check run.
    QmlMainWindowUI* ui = nullptr;
    std::function<void()> continuation;

    /// The scene node the check animates.
    OORef<SceneNode> node;

    /// The indices of the tracks of that node's position, rotation and scaling controllers.
    int positionTrack = -1;
    int rotationTrack = -1;
    int scalingTrack = -1;

    /// The number of undo steps before an edit, so that "one edit is one undo step" can be asserted.
    int undoCount = 0;
};

using StatePtr = std::shared_ptr<AnimationCheckState>;

/// Runs a phase of the check once the model and the scene had time to settle.
template<class Function>
void afterSettle(const StatePtr& state, Function&& phase)
{
    scheduleDelayed(state->ui, 400, [state, phase]() { phase(state); });
}

/// Returns the model under test, or reports that the frontend does not offer one.
QmlAnimationModel* animationModel(const StatePtr& state)
{
    if(QmlAnimationModel* model = state->ui->animationModel())
        return model;
    reportVerificationFailure(QStringLiteral("the frontend offers no animation model, so nothing can be verified"));
    return nullptr;
}

/// Returns the animation settings of the current data set, or reports that there is none.
AnimationSettings* animationSettings(const StatePtr& state)
{
    if(AnimationSettings* settings = state->ui->datasetContainer().activeAnimationSettings())
        return settings;
    reportVerificationFailure(QStringLiteral("the current data set has no animation settings"));
    return nullptr;
}

/// Returns the index of the first track whose parameter name contains the given text, or -1.
int trackContaining(QmlAnimationModel* model, const QString& text)
{
    for(int track = 0; track < model->trackCount(); track++) {
        if(model->trackParameterName(track).contains(text, Qt::CaseInsensitive))
            return track;
    }
    return -1;
}

/// Returns the row of the key of a track at the given frame, or -1.
int keyRowAt(QmlAnimationModel* model, int track, int frame)
{
    for(int row = 0; row < model->rowCount(); row++) {
        const QModelIndex index = model->index(row);
        if(index.data(QmlAnimationModel::TrackRole).toInt() == track && index.data(QmlAnimationModel::FrameRole).toInt() == frame)
            return row;
    }
    return -1;
}

/// Returns the keyframe controller of one sub-controller of a scene node's transformation, or null.
KeyframeController* transformationController(SceneNode* node, const QString& which)
{
    if(!node)
        return nullptr;
    PRSTransformationController* prs = dynamic_object_cast<PRSTransformationController>(node->transformationController());
    if(!prs)
        return nullptr;
    if(which == QLatin1String("position"))
        return dynamic_object_cast<KeyframeController>(prs->positionController());
    if(which == QLatin1String("rotation"))
        return dynamic_object_cast<KeyframeController>(prs->rotationController());
    if(which == QLatin1String("scaling"))
        return dynamic_object_cast<KeyframeController>(prs->scalingController());
    return nullptr;
}

/// Returns the number of undo steps that can be taken. This is the depth of the undo stack and not its size: undoing
/// keeps the undone operation on the stack for redo, so a new edit that truncates the redo part would not change the
/// size while it does add an edit.
int undoDepth(UndoStack* stack)
{
    return stack->canUndo() ? stack->index() + 1 : 0;
}

/// Creates a key at the given frame and reports whether it could be created. This is what an editor of an animatable
/// parameter does; a timeline cannot create keys, and neither can this model.
bool createKey(KeyframeController* controller, int frame)
{
    if(!controller) {
        reportVerificationFailure(QStringLiteral("the scene node has no controller to create a key in"));
        return false;
    }
    controller->createKey(AnimationTime::fromFrame(frame));
    return true;
}

// ----------------------------------------------------------------------------------------------------------------
// The phases, in the order in which they run.
// ----------------------------------------------------------------------------------------------------------------

void checkIntervalAndTime(const StatePtr& state);
void checkTracksAndKeys(const StatePtr& state);
void checkKeySelection(const StatePtr& state);
void checkContinuousKeyMove(const StatePtr& state);
void checkDiscreteKeyMove(const StatePtr& state);
void checkKeyDeletion(const StatePtr& state);
void checkVisibleRange(const StatePtr& state);
void checkPlaybackAndSettings(const StatePtr& state);

/// Verifies the interval, the current time and the frame<->time conversion, and that the interval is one undo step.
void checkIntervalAndTime(const StatePtr& state)
{
    GuiTaskScope taskScope(*state->ui);
    QmlAnimationModel* model = animationModel(state);
    AnimationSettings* settings = animationSettings(state);
    if(!model || !settings) {
        state->continuation();
        return;
    }

    // A freshly imported, static lattice has a one-frame animation.
    if(!model->isSingleFrame())
        reportVerificationFailure(QStringLiteral("a static scene reports a %1-frame animation").arg(model->numberOfFrames()));
    if(model->numberOfFrames() != 1)
        reportVerificationFailure(QStringLiteral("a one-frame interval reports %1 frames").arg(model->numberOfFrames()));

    // Setting the interval is one undo step of the animation settings, and the model follows it.
    state->undoCount = undoDepth(state->ui->undoStack());
    if(!model->setInterval(0, 20)) {
        reportVerificationFailure(QStringLiteral("setting the animation interval to 0..20 was refused"));
        state->continuation();
        return;
    }
    if(undoDepth(state->ui->undoStack()) != state->undoCount + 1)
        reportVerificationFailure(QStringLiteral("setting the interval changed the undo stack by %1 steps instead of one")
            .arg(undoDepth(state->ui->undoStack()) - state->undoCount));
    if(model->firstFrame() != 0 || model->lastFrame() != 20)
        reportVerificationFailure(QStringLiteral("the model reports the interval %1..%2 instead of 0..20").arg(model->firstFrame()).arg(model->lastFrame()));
    if(model->numberOfFrames() != 21 || model->isSingleFrame())
        reportVerificationFailure(QStringLiteral("the model reports %1 frames (single frame: %2) for the interval 0..20")
            .arg(model->numberOfFrames()).arg(model->isSingleFrame()));
    if(settings->firstFrame() != 0 || settings->lastFrame() != 20)
        reportVerificationFailure(QStringLiteral("the animation settings report the interval %1..%2 instead of 0..20").arg(settings->firstFrame()).arg(settings->lastFrame()));
    if(!model->setInterval(20, 0))
        qInfo() << "ANIMATION_TEST an empty interval (20..0) was refused as expected";

    // The current time: the model moves it and clamps it to the interval.
    model->goToFrame(7);
    if(model->currentFrame() != 7)
        reportVerificationFailure(QStringLiteral("goToFrame(7) left the current frame at %1").arg(model->currentFrame()));
    model->goToFrame(100);
    if(model->currentFrame() != 20)
        reportVerificationFailure(QStringLiteral("goToFrame(100) was not clamped to the interval: frame %1").arg(model->currentFrame()));
    model->goToFrame(-5);
    if(model->currentFrame() != 0)
        reportVerificationFailure(QStringLiteral("goToFrame(-5) was not clamped to the interval: frame %1").arg(model->currentFrame()));

    // The frame<->time conversion is the one of the animation settings, and a string it cannot parse has no frame.
    const QString timeText = model->timeStringForFrame(5);
    if(timeText.isEmpty())
        reportVerificationFailure(QStringLiteral("the model produced no time string for frame 5"));
    const QVariant frame = model->frameFromTimeString(timeText);
    if(!frame.isValid() || frame.toInt() != 5)
        reportVerificationFailure(QStringLiteral("the time string \"%1\" of frame 5 converts back to %2").arg(timeText, frame.toString()));
    if(model->frameFromTimeString(QStringLiteral("no-such-time-value")).isValid())
        reportVerificationFailure(QStringLiteral("a string the animation settings cannot parse produced a frame"));
    if(model->currentTimeString().isEmpty())
        reportVerificationFailure(QStringLiteral("the model produced no time string for the current frame"));

    qInfo() << "ANIMATION_TEST interval: 1..1 -> 0..20 as one undo step, frame 7 -> 20 -> 0, time string of frame 5 is" << timeText;

    // Undo and redo of the interval change, which is what the timeline's "undo" does.
    state->ui->undoStack()->undo();
    afterSettle(state, [](const StatePtr& state) {
        GuiTaskScope taskScope(*state->ui);
        QmlAnimationModel* model = animationModel(state);
        if(!model) {
            state->continuation();
            return;
        }
        if(model->numberOfFrames() != 1)
            reportVerificationFailure(QStringLiteral("undoing the interval change left %1 frames").arg(model->numberOfFrames()));
        state->ui->undoStack()->redo();
        afterSettle(state, [](const StatePtr& state) {
            GuiTaskScope taskScope(*state->ui);
            QmlAnimationModel* model = animationModel(state);
            if(!model) {
                state->continuation();
                return;
            }
            if(model->firstFrame() != 0 || model->lastFrame() != 20)
                reportVerificationFailure(QStringLiteral("redoing the interval change produced %1..%2 instead of 0..20")
                    .arg(model->firstFrame()).arg(model->lastFrame()));
            reportCheckPhase("interval");
            checkTracksAndKeys(state);
        });
    });
}

/// Verifies the tracks: the animated parameters of the selected object and the keys as rows of the model.
void checkTracksAndKeys(const StatePtr& state)
{
    GuiTaskScope taskScope(*state->ui);
    QmlAnimationModel* model = animationModel(state);
    if(!model) {
        state->continuation();
        return;
    }

    // A scene node carries a transformation controller with a position, a rotation and a scaling sub-controller, and the
    // walk of the model finds all three.
    if(model->trackCount() == 0) {
        reportVerificationFailure(QStringLiteral("the selected scene node offers no animated parameter, although it has a transformation controller"));
        state->continuation();
        return;
    }
    for(int track = 0; track < model->trackCount(); track++) {
        if(model->trackParameterName(track).isEmpty())
            reportVerificationFailure(QStringLiteral("the track %1 has no parameter name").arg(track));
    }
    state->positionTrack = trackContaining(model, QStringLiteral("position"));
    state->rotationTrack = trackContaining(model, QStringLiteral("rotation"));
    state->scalingTrack = trackContaining(model, QStringLiteral("scaling"));
    if(state->positionTrack < 0 || state->rotationTrack < 0 || state->scalingTrack < 0) {
        QStringList names;
        for(int track = 0; track < model->trackCount(); track++)
            names << model->trackParameterName(track);
        reportVerificationFailure(QStringLiteral("the tracks of the scene node are %1, which does not name its position, rotation and scaling controllers")
            .arg(names.join(QStringLiteral(", "))));
        state->continuation();
        return;
    }

    qInfo() << "ANIMATION_TEST tracks:" << model->trackCount() << "tracks of the selected scene node, position is track"
            << state->positionTrack << model->trackParameterName(state->positionTrack);

    // A parameter without keys contributes no row, and its track reports none.
    if(model->keyCount() != 0 || model->trackKeyCount(state->positionTrack) != 0)
        reportVerificationFailure(QStringLiteral("a parameter without keys already reports %1 keys").arg(model->keyCount()));

    // Create two keys on the position controller and one on the rotation controller, the way an editor (auto-key mode
    // or the key editor of the property inspector) creates them.
    KeyframeController* position = transformationController(state->node, QStringLiteral("position"));
    KeyframeController* rotation = transformationController(state->node, QStringLiteral("rotation"));
    if(!createKey(position, 0) || !createKey(position, 20) || !createKey(rotation, 10)) {
        state->continuation();
        return;
    }

    afterSettle(state, [](const StatePtr& state) {
        GuiTaskScope taskScope(*state->ui);
        QmlAnimationModel* model = animationModel(state);
        if(!model) {
            state->continuation();
            return;
        }
        if(model->keyCount() != 3 || model->rowCount() != 3) {
            reportVerificationFailure(QStringLiteral("the model shows %1 key rows after three keys were created").arg(model->rowCount()));
            state->continuation();
            return;
        }
        if(model->trackKeyCount(state->positionTrack) != 2 || model->trackKeyCount(state->rotationTrack) != 1)
            reportVerificationFailure(QStringLiteral("the tracks report %1 and %2 keys instead of 2 and 1")
                .arg(model->trackKeyCount(state->positionTrack)).arg(model->trackKeyCount(state->rotationTrack)));

        // Every row has to name its parameter, its frame, the time string of that frame and a value; the position keys
        // have to be the ones that were created.
        for(int row = 0; row < model->rowCount(); row++) {
            const QModelIndex index = model->index(row);
            const int track = index.data(QmlAnimationModel::TrackRole).toInt();
            const int frame = index.data(QmlAnimationModel::FrameRole).toInt();
            const QString parameter = index.data(QmlAnimationModel::ParameterRole).toString();
            const QString time = index.data(QmlAnimationModel::TimeRole).toString();
            const QString value = index.data(QmlAnimationModel::ValueRole).toString();
            if(parameter.isEmpty() || time.isEmpty() || value.isEmpty())
                reportVerificationFailure(QStringLiteral("the key row %1 is incomplete: parameter \"%2\", time \"%3\", value \"%4\"")
                    .arg(row).arg(parameter, time, value));
            if(track < 0 || !parameter.contains(model->trackParameterName(track), Qt::CaseSensitive))
                reportVerificationFailure(QStringLiteral("the key row %1 names the track %2, whose parameter is \"%3\"")
                    .arg(row).arg(track).arg(model->trackParameterName(track)));
            qInfo() << "ANIMATION_TEST key row" << row << ":" << parameter << "frame" << frame << "value" << value;
        }
        if(keyRowAt(model, state->positionTrack, 0) < 0 || keyRowAt(model, state->positionTrack, 20) < 0 || keyRowAt(model, state->rotationTrack, 10) < 0)
            reportVerificationFailure(QStringLiteral("the keys of the position and rotation tracks are not at the frames they were created at"));

        reportCheckPhase("tracks and keys");

        checkKeySelection(state);
    });
}

/// Verifies selecting keys: by row, by frame, all of them, and clearing the selection.
void checkKeySelection(const StatePtr& state)
{
    GuiTaskScope taskScope(*state->ui);
    QmlAnimationModel* model = animationModel(state);
    if(!model) {
        state->continuation();
        return;
    }

    const int rowAtZero = keyRowAt(model, state->positionTrack, 0);
    const int rowAtTwenty = keyRowAt(model, state->positionTrack, 20);
    const int rowAtTen = keyRowAt(model, state->rotationTrack, 10);
    if(rowAtZero < 0 || rowAtTwenty < 0 || rowAtTen < 0) {
        reportVerificationFailure(QStringLiteral("the keys of the check are gone before the selection could be verified"));
        state->continuation();
        return;
    }

    // Selecting one key replaces the selection; two keys of different tracks can be at the same frame, so selecting a
    // frame selects all of them, which is what clicking the timeline does.
    if(!model->selectKey(rowAtTen))
        reportVerificationFailure(QStringLiteral("selecting the key in row %1 was refused").arg(rowAtTen));
    if(model->selectedKeyCount() != 1)
        reportVerificationFailure(QStringLiteral("%1 keys are selected after selecting one").arg(model->selectedKeyCount()));
    if(!model->index(rowAtTen).data(QmlAnimationModel::SelectedRole).toBool())
        reportVerificationFailure(QStringLiteral("the selected key does not report itself as selected"));
    if(model->selectedKeys().size() != 1 || model->selectedKeys().front().toMap().value(QStringLiteral("frame")).toInt() != 10)
        reportVerificationFailure(QStringLiteral("the selected key list does not describe the key at frame 10"));

    model->selectAllKeys();
    if(model->selectedKeyCount() != 3)
        reportVerificationFailure(QStringLiteral("%1 keys are selected after selecting all three").arg(model->selectedKeyCount()));
    model->clearKeySelection();
    if(model->selectedKeyCount() != 0)
        reportVerificationFailure(QStringLiteral("%1 keys are still selected after clearing the selection").arg(model->selectedKeyCount()));

    if(!model->selectKeysAtFrame(10))
        reportVerificationFailure(QStringLiteral("selecting the keys at frame 10 was refused"));
    if(model->selectedKeyCount() != 1)
        reportVerificationFailure(QStringLiteral("selecting the keys at frame 10 selected %1 keys").arg(model->selectedKeyCount()));
    // Adding the keys of another frame keeps the first selection.
    if(!model->selectKeysAtFrame(20, true) || model->selectedKeyCount() != 2)
        reportVerificationFailure(QStringLiteral("adding the keys at frame 20 produced %1 selected keys").arg(model->selectedKeyCount()));

    // Jumping to a selected key moves the current time there, which is what the timeline's "jump to key" does.
    model->goToFrame(0);
    if(!model->goToSelectedKey() || model->currentFrame() != 10)
        reportVerificationFailure(QStringLiteral("jumping to the selected key moved the current frame to %1").arg(model->currentFrame()));

    qInfo() << "ANIMATION_TEST selection: single, frame based, all and empty selections verified";

    model->clearKeySelection();
    reportCheckPhase("key selection");
    checkContinuousKeyMove(state);
}

/// Verifies the continuous key drag of D58: one transaction per gesture, cancel restoring the start.
void checkContinuousKeyMove(const StatePtr& state)
{
    GuiTaskScope taskScope(*state->ui);
    QmlAnimationModel* model = animationModel(state);
    if(!model) {
        state->continuation();
        return;
    }
    if(model->isKeyMoveActive())
        reportVerificationFailure(QStringLiteral("a key drag is active before the check started one"));

    const int rowAtTen = keyRowAt(model, state->rotationTrack, 10);
    if(rowAtTen < 0 || !model->selectKey(rowAtTen)) {
        reportVerificationFailure(QStringLiteral("the key at frame 10 could not be selected for the drag"));
        state->continuation();
        return;
    }
    state->undoCount = undoDepth(state->ui->undoStack());

    model->beginKeyMove();
    if(!model->isKeyMoveActive())
        reportVerificationFailure(QStringLiteral("beginKeyMove() did not open a drag"));
    model->updateKeyMove(4);
    if(keyRowAt(model, state->rotationTrack, 14) < 0)
        reportVerificationFailure(QStringLiteral("the drag did not move the key to frame 14"));
    // An update reports where the keys are now, not how far they moved since the last one.
    model->updateKeyMove(7);
    if(keyRowAt(model, state->rotationTrack, 17) < 0)
        reportVerificationFailure(QStringLiteral("a second drag update did not replace the first one (the key is not at frame 17)"));
    if(keyRowAt(model, state->positionTrack, 0) < 0)
        reportVerificationFailure(QStringLiteral("the drag moved a key that was not selected"));

    // The keys may not leave the animation interval: a shift beyond the end is reduced.
    model->updateKeyMove(40);
    if(keyRowAt(model, state->rotationTrack, 20) < 0)
        reportVerificationFailure(QStringLiteral("a shift beyond the animation interval did not clamp the key to frame 20"));

    // Cancelling restores the position the gesture started from, and leaves nothing on the undo stack.
    model->cancelKeyMove();
    if(model->isKeyMoveActive())
        reportVerificationFailure(QStringLiteral("the cancelled drag is still active"));
    if(keyRowAt(model, state->rotationTrack, 10) < 0)
        reportVerificationFailure(QStringLiteral("cancelling the drag did not restore the key at frame 10"));
    if(undoDepth(state->ui->undoStack()) != state->undoCount)
        reportVerificationFailure(QStringLiteral("cancelling the drag left %1 undo steps behind").arg(undoDepth(state->ui->undoStack()) - state->undoCount));

    // An accepted drag is exactly one undo step, and the undo restores the start of the gesture.
    model->beginKeyMove();
    model->updateKeyMove(3);
    model->updateKeyMove(5);
    model->commitKeyMove();
    if(model->isKeyMoveActive())
        reportVerificationFailure(QStringLiteral("the committed drag is still active"));
    if(undoDepth(state->ui->undoStack()) != state->undoCount + 1)
        reportVerificationFailure(QStringLiteral("an accepted drag produced %1 undo steps instead of one")
            .arg(undoDepth(state->ui->undoStack()) - state->undoCount));
    if(keyRowAt(model, state->rotationTrack, 15) < 0) {
        reportVerificationFailure(QStringLiteral("the accepted drag left the key at another frame than 15"));
        state->continuation();
        return;
    }
    qInfo() << "ANIMATION_TEST drag: 10 -> 14 -> 17, clamped to 20, cancelled back to 10, then 10 -> 15 as one undo step";

    afterSettle(state, [](const StatePtr& state) {
        GuiTaskScope taskScope(*state->ui);
        QmlAnimationModel* model = animationModel(state);
        if(!model) {
            state->continuation();
            return;
        }
        state->ui->undoStack()->undo();
        afterSettle(state, [](const StatePtr& state) {
            GuiTaskScope taskScope(*state->ui);
            QmlAnimationModel* model = animationModel(state);
            if(!model) {
                state->continuation();
                return;
            }
            if(keyRowAt(model, state->rotationTrack, 10) < 0)
                reportVerificationFailure(QStringLiteral("undoing the accepted drag did not restore the key at frame 10"));
            state->ui->undoStack()->redo();
            afterSettle(state, [](const StatePtr& state) {
                GuiTaskScope taskScope(*state->ui);
                QmlAnimationModel* model = animationModel(state);
                if(!model) {
                    state->continuation();
                    return;
                }
                if(keyRowAt(model, state->rotationTrack, 15) < 0)
                    reportVerificationFailure(QStringLiteral("redoing the accepted drag did not move the key back to frame 15"));
                reportCheckPhase("continuous move");
                checkDiscreteKeyMove(state);
            });
        });
    });
}

/// Verifies the discrete key move and its clamping to the animation interval.
void checkDiscreteKeyMove(const StatePtr& state)
{
    GuiTaskScope taskScope(*state->ui);
    QmlAnimationModel* model = animationModel(state);
    if(!model) {
        state->continuation();
        return;
    }

    // Move the key of the rotation track, which is at frame 15, back to 10 with one undoable call.
    int row = keyRowAt(model, state->rotationTrack, 15);
    if(row < 0 || !model->selectKey(row)) {
        reportVerificationFailure(QStringLiteral("the key at frame 15 could not be selected"));
        state->continuation();
        return;
    }
    state->undoCount = undoDepth(state->ui->undoStack());
    if(!model->moveSelectedKeys(-5))
        reportVerificationFailure(QStringLiteral("moving the selected key by five frames was refused"));
    if(undoDepth(state->ui->undoStack()) != state->undoCount + 1)
        reportVerificationFailure(QStringLiteral("a discrete key move produced %1 undo steps instead of one")
            .arg(undoDepth(state->ui->undoStack()) - state->undoCount));
    if(keyRowAt(model, state->rotationTrack, 10) < 0) {
        reportVerificationFailure(QStringLiteral("the discrete move did not place the key at frame 10"));
        state->continuation();
        return;
    }

    // A selection that already touches the end of the interval cannot move further, and moving back returns it.
    row = keyRowAt(model, state->rotationTrack, 10);
    model->selectKey(row);
    model->selectKey(keyRowAt(model, state->positionTrack, 20), true);
    state->undoCount = undoDepth(state->ui->undoStack());
    model->moveSelectedKeys(5);
    if(keyRowAt(model, state->rotationTrack, 10) < 0 || keyRowAt(model, state->positionTrack, 20) < 0)
        reportVerificationFailure(QStringLiteral("a shift beyond the end of the interval moved keys that could not move"));
    if(undoDepth(state->ui->undoStack()) != state->undoCount)
        reportVerificationFailure(QStringLiteral("a shift that could not move anything still recorded an undo step"));
    model->clearKeySelection();

    qInfo() << "ANIMATION_TEST discrete move: 15 -> 10 as one undo step, a blocked shift recorded nothing";

    afterSettle(state, [](const StatePtr& state) {
        GuiTaskScope taskScope(*state->ui);
        QmlAnimationModel* model = animationModel(state);
        if(!model) {
            state->continuation();
            return;
        }
        state->ui->undoStack()->undo();
        afterSettle(state, [](const StatePtr& state) {
            GuiTaskScope taskScope(*state->ui);
            QmlAnimationModel* model = animationModel(state);
            if(!model) {
                state->continuation();
                return;
            }
            if(model->keyCount() != 3 || keyRowAt(model, state->rotationTrack, 15) < 0)
                reportVerificationFailure(QStringLiteral("undoing the discrete move did not restore the key at frame 15"));
            reportCheckPhase("discrete move");
            checkKeyDeletion(state);
        });
    });
}

/// Verifies deleting keys, its undo, and what happens to the selection.
void checkKeyDeletion(const StatePtr& state)
{
    GuiTaskScope taskScope(*state->ui);
    QmlAnimationModel* model = animationModel(state);
    if(!model) {
        state->continuation();
        return;
    }

    // Delete the two keys of the position track, keeping the rotation key.
    model->clearKeySelection();
    const int first = keyRowAt(model, state->positionTrack, 0);
    const int second = keyRowAt(model, state->positionTrack, 20);
    if(first < 0 || second < 0) {
        reportVerificationFailure(QStringLiteral("the position keys are gone before the deletion"));
        state->continuation();
        return;
    }
    model->selectKey(first);
    model->selectKey(second, true);
    state->undoCount = undoDepth(state->ui->undoStack());
    if(!model->deleteSelectedKeys()) {
        reportVerificationFailure(QStringLiteral("deleting the selected keys was refused"));
        state->continuation();
        return;
    }
    if(undoDepth(state->ui->undoStack()) != state->undoCount + 1)
        reportVerificationFailure(QStringLiteral("deleting two keys produced %1 undo steps instead of one")
            .arg(undoDepth(state->ui->undoStack()) - state->undoCount));

    afterSettle(state, [](const StatePtr& state) {
        GuiTaskScope taskScope(*state->ui);
        QmlAnimationModel* model = animationModel(state);
        if(!model) {
            state->continuation();
            return;
        }
        if(model->keyCount() != 1 || model->rowCount() != 1)
            reportVerificationFailure(QStringLiteral("%1 keys are left after deleting two of three").arg(model->keyCount()));
        if(model->trackKeyCount(state->positionTrack) != 0)
            reportVerificationFailure(QStringLiteral("the position track still reports %1 keys").arg(model->trackKeyCount(state->positionTrack)));
        if(model->selectedKeyCount() != 0)
            reportVerificationFailure(QStringLiteral("the keys that were deleted are still selected"));
        qInfo() << "ANIMATION_TEST deletion: 2 of 3 keys deleted as one undo step, the deleted keys left the selection";

        state->ui->undoStack()->undo();
        afterSettle(state, [](const StatePtr& state) {
            GuiTaskScope taskScope(*state->ui);
            QmlAnimationModel* model = animationModel(state);
            if(!model) {
                state->continuation();
                return;
            }
            // The undo returns the keys, at the frames they were deleted at - but not the selection they had, which is
            // the behaviour of the classic track bar as well.
            if(model->keyCount() != 3)
                reportVerificationFailure(QStringLiteral("undoing the deletion restored %1 of 3 keys").arg(model->keyCount()));
            if(keyRowAt(model, state->positionTrack, 0) < 0 || keyRowAt(model, state->positionTrack, 20) < 0)
                reportVerificationFailure(QStringLiteral("the restored keys are not at the frames they were deleted at"));
            if(model->selectedKeyCount() != 0)
                reportVerificationFailure(QStringLiteral("undoing the deletion restored a selection of %1 keys, which the classic frontend does not do either")
                    .arg(model->selectedKeyCount()));
            qInfo() << "ANIMATION_TEST deletion undo: the three keys are back, the selection stays empty";

            reportCheckPhase("deletion");

            checkVisibleRange(state);
        });
    });
}

/// Verifies the visible range as presentation state that follows the interval until it is set explicitly.
void checkVisibleRange(const StatePtr& state)
{
    GuiTaskScope taskScope(*state->ui);
    QmlAnimationModel* model = animationModel(state);
    if(!model) {
        state->continuation();
        return;
    }

    if(model->rangeStart() != 0 || model->rangeEnd() != 20)
        reportVerificationFailure(QStringLiteral("the timeline shows %1..%2 although the interval is 0..20")
            .arg(model->rangeStart()).arg(model->rangeEnd()));
    if(!model->setRange(5, 15) || model->rangeStart() != 5 || model->rangeEnd() != 15)
        reportVerificationFailure(QStringLiteral("setting the visible range to 5..15 produced %1..%2")
            .arg(model->rangeStart()).arg(model->rangeEnd()));
    if(model->setRange(15, 5))
        reportVerificationFailure(QStringLiteral("an empty visible range (15..5) was accepted"));
    if(model->rangeStart() != 5 || model->rangeEnd() != 15)
        reportVerificationFailure(QStringLiteral("the refused range changed the visible range to %1..%2")
            .arg(model->rangeStart()).arg(model->rangeEnd()));
    // A range beyond the interval is clamped to it.
    model->setRange(-10, 100);
    if(model->rangeStart() != 0 || model->rangeEnd() != 20)
        reportVerificationFailure(QStringLiteral("a range beyond the interval produced %1..%2 instead of 0..20")
            .arg(model->rangeStart()).arg(model->rangeEnd()));
    // An explicit range is the user's choice and does not follow a later interval change; resetting it makes it follow.
    model->setRange(5, 15);
    afterSettle(state, [](const StatePtr& state) {
        GuiTaskScope taskScope(*state->ui);
        QmlAnimationModel* model = animationModel(state);
        if(!model) {
            state->continuation();
            return;
        }
        if(!model->setInterval(0, 30)) {
            reportVerificationFailure(QStringLiteral("changing the interval to 0..30 was refused"));
            state->continuation();
            return;
        }
        afterSettle(state, [](const StatePtr& state) {
            GuiTaskScope taskScope(*state->ui);
            QmlAnimationModel* model = animationModel(state);
            if(!model) {
                state->continuation();
                return;
            }
            if(model->rangeStart() != 5 || model->rangeEnd() != 15)
                reportVerificationFailure(QStringLiteral("an explicit range followed the interval change to %1..%2")
                    .arg(model->rangeStart()).arg(model->rangeEnd()));
            model->resetRange();
            if(model->rangeStart() != 0 || model->rangeEnd() != 30)
                reportVerificationFailure(QStringLiteral("a reset range is %1..%2 instead of the interval 0..30")
                    .arg(model->rangeStart()).arg(model->rangeEnd()));
            // Restore the interval the other phases worked with.
            model->setInterval(0, 20);
            qInfo() << "ANIMATION_TEST range: follows the interval, is clamped, and an explicit range survives an interval change";
            reportCheckPhase("visible range");
            afterSettle(state, checkPlaybackAndSettings);
        });
    });
}

/// Verifies the playback state (core's, not the model's) and the undoable playback settings.
void checkPlaybackAndSettings(const StatePtr& state)
{
    GuiTaskScope taskScope(*state->ui);
    QmlAnimationModel* model = animationModel(state);
    AnimationSettings* settings = animationSettings(state);
    if(!model || !settings) {
        state->continuation();
        return;
    }
    DataSetContainer& container = state->ui->datasetContainer();

    // The playback state is core's SceneAnimationPlayback: the model mirrors it, and the shared command drives it.
    if(model->isPlaying() != container.isPlaybackActive())
        reportVerificationFailure(QStringLiteral("the model reports a playback state of %1 while the data set container reports %2")
            .arg(model->isPlaying()).arg(container.isPlaybackActive()));
    Command* playbackCommand = state->ui->actionManager()->findCommand(QStringLiteral("AnimationTogglePlayback"));
    if(!playbackCommand) {
        reportVerificationFailure(QStringLiteral("the shared playback command is not available"));
        state->continuation();
        return;
    }
    playbackCommand->setChecked(true);
    if(!model->isPlaying())
        reportVerificationFailure(QStringLiteral("the model did not follow the started playback"));
    playbackCommand->setChecked(false);
    if(model->isPlaying())
        reportVerificationFailure(QStringLiteral("the model did not follow the stopped playback"));

    // The playback settings are data-set edits, so they are undoable like any other edit of the animation.
    state->undoCount = undoDepth(state->ui->undoStack());
    const double originalFps = model->framesPerSecond();
    if(!model->setFramesPerSecond(24.0) || model->framesPerSecond() != 24.0)
        reportVerificationFailure(QStringLiteral("setting the playback rate to 24 produced %1").arg(model->framesPerSecond()));
    if(undoDepth(state->ui->undoStack()) != state->undoCount + 1)
        reportVerificationFailure(QStringLiteral("changing the playback rate produced %1 undo steps instead of one")
            .arg(undoDepth(state->ui->undoStack()) - state->undoCount));
    if(!model->setFramesPerSecond(0.0))
        qInfo() << "ANIMATION_TEST a non-positive playback rate was refused as expected";
    if(!model->setLoopPlayback(!model->loopPlayback()))
        reportVerificationFailure(QStringLiteral("toggling the looped playback was refused"));
    if(!model->setPreferSimulationTimeDisplay(true))
        reportVerificationFailure(QStringLiteral("enabling the simulation-time display was refused"));

    state->ui->undoStack()->undo();
    if(model->preferSimulationTimeDisplay())
        reportVerificationFailure(QStringLiteral("undoing the time display change did not restore the setting"));
    model->setFramesPerSecond(originalFps);

    qInfo() << "ANIMATION_TEST playback: the model mirrors the shared playback, and the playback settings are undoable";

    reportCheckPhase("playback");
    state->continuation();
}

}   // namespace

void runAnimationTest(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    // The phases this check walks through, in the order the check functions hand over to each other.
    declareCheckPhases({ "import", "node selected", "interval", "tracks and keys", "key selection", "continuous move",
                         "discrete move", "deletion", "visible range", "playback" });

    auto state = std::make_shared<AnimationCheckState>();
    state->ui = ui;
    state->continuation = std::move(continuation);

    if(!ui->animationModel()) {
        reportVerificationFailure(QStringLiteral("the frontend offers no animation model, so the check cannot run"));
        state->continuation();
        return;
    }

    // The tracks are the animated parameters of the *selected* objects, so the check needs a scene node of its own: the
    // scene the earlier checks leave behind may be empty (the import check ends with a cancelled import).
    const QString dataFile = writeLatticeFile(QDir::tempPath() + QStringLiteral("/ovito-qml-animation-check.xyz"), 6, 3.6);
    if(dataFile.isEmpty()) {
        state->continuation();
        return;
    }
    if(QmlWorkbenchController* controller = ui->workbenchController())
        controller->importFiles({QUrl::fromLocalFile(dataFile)});
    else {
        reportVerificationFailure(QStringLiteral("the workbench has no controller that could import the data file"));
        state->continuation();
        return;
    }

    pollUntil(ui, 50, 20000,
        [ui]() {
            const FileSource* fileSource = firstFileSource(ui);
            return fileSource && fileSource->numberOfSourceFrames() >= 1;
        },
        [state](bool ready) {
            if(!ready) {
                reportVerificationFailure(QStringLiteral("the data file of the animation check was not imported"));
                state->continuation();
                return;
            }
            reportCheckPhase("import");
            scheduleDelayed(state->ui, 600, [state]() {
                GuiTaskScope taskScope(*state->ui);
                // Select the node the import created: the timeline of the frontend shows the objects the scene selection
                // holds, exactly as the classic track bar shows the controllers of the selected nodes.
                Scene* scene = state->ui->datasetContainer().activeScene();
                SceneNode* node = scene && !scene->children().empty() ? scene->children().front().get() : nullptr;
                if(!node) {
                    reportVerificationFailure(QStringLiteral("the imported data set has no scene node to animate"));
                    state->continuation();
                    return;
                }
                state->node = node;
                scene->selection()->setNode(node);
                // The selection change is announced after a timer turn of the data set container, so wait for it.
                reportCheckPhase("node selected");
                afterSettle(state, checkIntervalAndTime);
            });
        });
}

}   // namespace Ovito::Spike
