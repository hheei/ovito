// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/qml/models/QmlAnimationModel.h>
#include <ovito/gui/base/app/WorkbenchUI.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/animation/controller/KeyframeController.h>
#include <ovito/core/dataset/animation/controller/AnimationKeys.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/SceneNode.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/core/oo/PropertyFieldDescriptor.h>
#include <ovito/core/utilities/Exception.h>
#include <QTimer>
#include <algorithm>

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
QmlAnimationModel::QmlAnimationModel(WorkbenchUI& ui, QObject* parent) : QAbstractListModel(parent), _ui(ui)
{
    DataSetContainer& container = _ui.datasetContainer();

    // The animation state lives in the data set's animation settings object, and the data set container already
    // announces the changes the timeline follows, so the model does not have to watch the object for those.
    connect(&container, &DataSetContainer::animationSettingsReplaced, this, &QmlAnimationModel::onAnimationSettingsReplaced);
    connect(&container, &DataSetContainer::dataSetChanged, this, &QmlAnimationModel::onDataSetChanged);
    connect(&container, &DataSetContainer::animationIntervalChanged, this, &QmlAnimationModel::onAnimationIntervalChanged);
    connect(&container, &DataSetContainer::currentFrameChanged, this, [this]() { Q_EMIT animationStateChanged(); });
    connect(&container, &DataSetContainer::framesPerSecondChanged, this, [this]() { Q_EMIT animationStateChanged(); });
    connect(&container, &DataSetContainer::timeFormatChanged, this, [this]() { Q_EMIT animationStateChanged(); });
    connect(&container, &DataSetContainer::playbackChanged, this, [this]() { Q_EMIT animationStateChanged(); });

    // The tracks are the animation controllers of the selected scene nodes, so a different selection is a different
    // timeline; the container coalesces a burst of selection changes into one signal (see its documentation).
    connect(&container, &DataSetContainer::selectionChangeComplete, this, [this]() { rebuildTracks(); });
    connect(&container, &DataSetContainer::sceneReplaced, this, [this]() { rebuildTracks(); });

    // A controller, the objects the controllers hang off and the keys themselves are observed, because a key can be
    // created, moved or deleted by any editor of the frontend, not only through this model.
    _objects.connect(this, &QmlAnimationModel::onObjectNotificationEvent);
    _controllers.connect(this, &QmlAnimationModel::onControllerNotificationEvent);
    _keys.connect(this, &QmlAnimationModel::onKeyNotificationEvent);
    _selectedKeys.connect(this, &QmlAnimationModel::onKeyNotificationEvent);

    // Follow the settings object itself as well: not every property of it has a data set container signal (the playback
    // settings do not), and a session load can replace it under us.
    _animationSettings.connect(this, &QmlAnimationModel::onAnimationSettingsNotificationEvent);
    onAnimationSettingsReplaced(container.activeAnimationSettings());
}

/******************************************************************************
* Returns the animation settings of the current data set.
******************************************************************************/
AnimationSettings* QmlAnimationModel::animationSettings() const
{
    return _ui.datasetContainer().activeAnimationSettings();
}

// ----------------------------------------------------------------------------------------------------------------
// The state of the animation
// ----------------------------------------------------------------------------------------------------------------

int QmlAnimationModel::firstFrame() const
{
    return animationSettings() ? animationSettings()->firstFrame() : 0;
}

int QmlAnimationModel::lastFrame() const
{
    return animationSettings() ? animationSettings()->lastFrame() : 0;
}

int QmlAnimationModel::currentFrame() const
{
    return animationSettings() ? animationSettings()->currentFrame() : 0;
}

int QmlAnimationModel::numberOfFrames() const
{
    return animationSettings() ? animationSettings()->numberOfFrames() : 0;
}

bool QmlAnimationModel::isSingleFrame() const
{
    return animationSettings() ? animationSettings()->isSingleFrame() : true;
}

QString QmlAnimationModel::currentTimeString() const
{
    if(AnimationSettings* settings = animationSettings())
        return settings->timeToString(settings->currentTime());
    return {};
}

double QmlAnimationModel::framesPerSecond() const
{
    return animationSettings() ? animationSettings()->framesPerSecond() : 0.0;
}

int QmlAnimationModel::playbackSpeed() const
{
    return animationSettings() ? animationSettings()->playbackSpeed() : 1;
}

bool QmlAnimationModel::loopPlayback() const
{
    return animationSettings() ? animationSettings()->loopPlayback() : false;
}

int QmlAnimationModel::playbackEveryNthFrame() const
{
    return animationSettings() ? animationSettings()->playbackEveryNthFrame() : 1;
}

bool QmlAnimationModel::autoAdjustInterval() const
{
    return animationSettings() ? animationSettings()->autoAdjustInterval() : false;
}

bool QmlAnimationModel::preferSimulationTimeDisplay() const
{
    return animationSettings() ? animationSettings()->preferSimulationTimeDisplay() : false;
}

bool QmlAnimationModel::isPlaying() const
{
    return _ui.datasetContainer().isPlaybackActive();
}

QString QmlAnimationModel::timeStringForFrame(int frame) const
{
    if(AnimationSettings* settings = animationSettings())
        return settings->timeToString(AnimationTime::fromFrame(frame));
    return QString::number(frame);
}

QVariant QmlAnimationModel::frameFromTimeString(const QString& text) const
{
    AnimationSettings* settings = animationSettings();
    if(!settings)
        return {};
    try {
        return QVariant::fromValue(settings->stringToTime(text).frame());
    }
    catch(const Exception&) {
        // A string the animation settings cannot parse is not a frame; the caller decides what to show then.
        return {};
    }
}

void QmlAnimationModel::goToFrame(int frame)
{
    if(AnimationSettings* settings = animationSettings())
        settings->setCurrentFrame(std::clamp(frame, settings->firstFrame(), settings->lastFrame()));
}

bool QmlAnimationModel::setInterval(int firstFrame, int lastFrame)
{
    AnimationSettings* settings = animationSettings();
    if(!settings || firstFrame > lastFrame)
        return false;
    if(settings->firstFrame() == firstFrame && settings->lastFrame() == lastFrame)
        return true;
    return _ui.performTransaction(tr("Change animation interval"), [&]() {
        settings->setFirstFrame(firstFrame);
        settings->setLastFrame(lastFrame);
    });
}

bool QmlAnimationModel::setFramesPerSecond(double framesPerSecond)
{
    AnimationSettings* settings = animationSettings();
    if(!settings || framesPerSecond <= 0)
        return false;
    if(settings->framesPerSecond() == framesPerSecond)
        return true;
    const bool done = _ui.performTransaction(tr("Change animation speed"), [&]() { settings->setFramesPerSecond(framesPerSecond); });
    if(done)
        PROPERTY_FIELD(AnimationSettings::framesPerSecond)->memorizeDefaultValue(settings);
    return done;
}

bool QmlAnimationModel::setPlaybackSpeed(int speed)
{
    AnimationSettings* settings = animationSettings();
    if(!settings || speed == 0)
        return false;
    if(settings->playbackSpeed() == speed)
        return true;
    const bool done = _ui.performTransaction(tr("Change playback speed"), [&]() { settings->setPlaybackSpeed(speed); });
    if(done)
        PROPERTY_FIELD(AnimationSettings::playbackSpeed)->memorizeDefaultValue(settings);
    return done;
}

bool QmlAnimationModel::setLoopPlayback(bool loop)
{
    AnimationSettings* settings = animationSettings();
    if(!settings)
        return false;
    if(settings->loopPlayback() == loop)
        return true;
    const bool done = _ui.performTransaction(tr("Change looped playback"), [&]() { settings->setLoopPlayback(loop); });
    if(done)
        PROPERTY_FIELD(AnimationSettings::loopPlayback)->memorizeDefaultValue(settings);
    return done;
}

bool QmlAnimationModel::setPlaybackEveryNthFrame(int n)
{
    AnimationSettings* settings = animationSettings();
    if(!settings || n < 1)
        return false;
    if(settings->playbackEveryNthFrame() == n)
        return true;
    return _ui.performTransaction(tr("Change playback frame skipping"), [&]() { settings->setPlaybackEveryNthFrame(n); });
}

bool QmlAnimationModel::setAutoAdjustInterval(bool adjust)
{
    AnimationSettings* settings = animationSettings();
    if(!settings)
        return false;
    if(settings->autoAdjustInterval() == adjust)
        return true;
    return _ui.performTransaction(tr("Change interval adjustment"), [&]() { settings->setAutoAdjustInterval(adjust); });
}

bool QmlAnimationModel::setPreferSimulationTimeDisplay(bool prefer)
{
    AnimationSettings* settings = animationSettings();
    if(!settings)
        return false;
    if(settings->preferSimulationTimeDisplay() == prefer)
        return true;
    const bool done = _ui.performTransaction(tr("Change time display"), [&]() { settings->setPreferSimulationTimeDisplay(prefer); });
    if(done)
        PROPERTY_FIELD(AnimationSettings::preferSimulationTimeDisplay)->memorizeDefaultValue(settings);
    return done;
}

// ----------------------------------------------------------------------------------------------------------------
// The tracks
// ----------------------------------------------------------------------------------------------------------------

QString QmlAnimationModel::trackParameterName(int track) const
{
    if(track < 0 || track >= _parameterNames.size())
        return {};
    return _parameterNames[track];
}

int QmlAnimationModel::trackKeyCount(int track) const
{
    if(KeyframeController* ctrl = trackController(track))
        return ctrl->keys().size();
    return 0;
}

int QmlAnimationModel::trackKeyFrame(int track, int key) const
{
    if(KeyframeController* ctrl = trackController(track)) {
        if(key >= 0 && key < ctrl->keys().size())
            return ctrl->keys()[key]->time().frame();
    }
    return -1;
}

KeyframeController* QmlAnimationModel::trackController(int track) const
{
    if(track < 0 || track >= _controllers.targets().size())
        return nullptr;
    return _controllers.targets()[track];
}

// ----------------------------------------------------------------------------------------------------------------
// The keys
// ----------------------------------------------------------------------------------------------------------------

int QmlAnimationModel::rowCount(const QModelIndex& parent) const
{
    // A list model has no child rows; a view that asks for one gets nothing.
    if(parent.isValid())
        return 0;
    return keyCount();
}

int QmlAnimationModel::keyCount() const
{
    int count = 0;
    for(KeyframeController* ctrl : _controllers.targets())
        count += ctrl->keys().size();
    return count;
}

AnimationKey* QmlAnimationModel::keyAt(int row) const
{
    if(row < 0)
        return nullptr;
    for(KeyframeController* ctrl : _controllers.targets()) {
        if(row < ctrl->keys().size())
            return ctrl->keys()[row];
        row -= ctrl->keys().size();
    }
    return nullptr;
}

int QmlAnimationModel::trackOfKey(const AnimationKey* key) const
{
    if(!key)
        return -1;
    const auto& controllers = _controllers.targets();
    for(int index = 0; index < controllers.size(); index++) {
        for(const OORef<AnimationKey>& candidate : controllers[index]->keys()) {
            if(candidate.get() == key)
                return index;
        }
    }
    return -1;
}

bool QmlAnimationModel::isSelected(const AnimationKey* key) const
{
    if(!key)
        return false;
    for(const OORef<AnimationKey>& selected : _selectedKeys.targets()) {
        if(selected.get() == key)
            return true;
    }
    return false;
}

QVariant QmlAnimationModel::data(const QModelIndex& index, int role) const
{
    const AnimationKey* key = keyAt(index.row());
    if(!key)
        return {};
    // The rows were filled from the controllers, so the track of a row is known without walking them again: a view
    // asks for this on every repaint, and a walk per role made a repaint cost one scan per row.
    const int track = (index.row() >= 0 && index.row() < _rowTracks.size()) ? _rowTracks[index.row()] : trackOfKey(key);
    switch(role) {
    case Qt::DisplayRole:
    case ValueRole:
        return keyValueString(key);
    case TrackRole:
        return track;
    case ParameterRole:
        return trackParameterName(track);
    case FrameRole:
        return key->time().frame();
    case TimeRole:
        return timeStringForFrame(key->time().frame());
    case SelectedRole:
        return isSelected(key);
    default:
        return {};
    }
}

QHash<int, QByteArray> QmlAnimationModel::roleNames() const
{
    return {
        { Qt::DisplayRole, "display" },
        { TrackRole, "track" },
        { ParameterRole, "parameter" },
        { FrameRole, "frame" },
        { TimeRole, "time" },
        { ValueRole, "value" },
        { SelectedRole, "selected" }
    };
}

QVariantList QmlAnimationModel::selectedKeys() const
{
    QVariantList list;
    for(const OORef<AnimationKey>& key : _selectedKeys.targets()) {
        const int track = trackOfKey(key);
        QVariantMap entry;
        entry.insert(QStringLiteral("track"), track);
        entry.insert(QStringLiteral("parameter"), trackParameterName(track));
        entry.insert(QStringLiteral("frame"), key->time().frame());
        entry.insert(QStringLiteral("time"), timeStringForFrame(key->time().frame()));
        entry.insert(QStringLiteral("value"), keyValueString(key));
        list.push_back(entry);
    }
    return list;
}

bool QmlAnimationModel::selectKey(int row, bool toggle)
{
    AnimationKey* key = keyAt(row);
    if(!key)
        return false;
    if(toggle) {
        if(isSelected(key))
            _selectedKeys.remove(key);
        else
            _selectedKeys.push_back(key);
    }
    else {
        _selectedKeys.setTargets({key});
    }
    Q_EMIT selectionChanged();
    // The selected state is a role of every row that shows this key, and a key is shown in exactly one row.
    const QModelIndex changed = index(row);
    Q_EMIT dataChanged(changed, changed, { SelectedRole, Qt::DisplayRole });
    return true;
}

bool QmlAnimationModel::selectKeysAtFrame(int frame, bool toggle)
{
    QVector<AnimationKey*> keys;
    for(KeyframeController* ctrl : _controllers.targets()) {
        for(const OORef<AnimationKey>& key : ctrl->keys()) {
            if(key->time().frame() == frame)
                keys.push_back(key.get());
        }
    }
    if(keys.empty())
        return false;
    if(toggle) {
        for(AnimationKey* key : keys) {
            if(isSelected(key))
                _selectedKeys.remove(key);
            else
                _selectedKeys.push_back(key);
        }
    }
    else {
        _selectedKeys.setTargets(keys);
    }
    Q_EMIT selectionChanged();
    Q_EMIT dataChanged(index(0), index(rowCount() - 1), { SelectedRole, Qt::DisplayRole });
    return true;
}

void QmlAnimationModel::selectAllKeys()
{
    QVector<AnimationKey*> keys;
    for(KeyframeController* ctrl : _controllers.targets()) {
        for(const OORef<AnimationKey>& key : ctrl->keys())
            keys.push_back(key.get());
    }
    _selectedKeys.setTargets(keys);
    Q_EMIT selectionChanged();
    // An empty model has no row to report a change for, and the selection is the only thing that changed.
    if(keyCount() != 0)
        Q_EMIT dataChanged(index(0), index(keyCount() - 1), { SelectedRole });
}

void QmlAnimationModel::clearKeySelection()
{
    if(_selectedKeys.targets().empty())
        return;
    _selectedKeys.clear();
    Q_EMIT selectionChanged();
    Q_EMIT dataChanged(index(0), index(rowCount() - 1), { SelectedRole, Qt::DisplayRole });
}

bool QmlAnimationModel::goToSelectedKey()
{
    if(_selectedKeys.targets().empty())
        return false;
    goToFrame(_selectedKeys.targets().front()->time().frame());
    return true;
}

bool QmlAnimationModel::moveSelectedKeys(int frameDelta)
{
    if(_selectedKeys.targets().empty())
        return false;
    return _ui.performTransaction(tr("Move animation keys"), [&]() { applyKeyShift(frameDelta); });
}

bool QmlAnimationModel::deleteSelectedKeys()
{
    if(_selectedKeys.targets().empty())
        return false;
    // The keys of the whole selection are handed to every controller, which removes the ones it owns.
    const QVector<OORef<AnimationKey>> keys = _selectedKeys.targets();
    return _ui.performTransaction(tr("Delete animation keys"), [&]() {
        for(KeyframeController* ctrl : _controllers.targets())
            ctrl->deleteKeys(keys);
    });
}

void QmlAnimationModel::beginKeyMove()
{
    if(isKeyMoveActive() || _selectedKeys.targets().empty())
        return;
    _keyMoveTransaction.begin(_ui, tr("Move animation keys"));
}

bool QmlAnimationModel::updateKeyMove(int frameDelta)
{
    OVITO_ASSERT(isKeyMoveActive());
    if(!isKeyMoveActive())
        return false;
    // An update replaces the position of the previous one: the gesture reports where the keys are now, not how far they
    // moved since the last mouse event (D58).
    _keyMoveTransaction.revert();
    return _ui.performActions(_keyMoveTransaction, [&]() { applyKeyShift(frameDelta); });
}

void QmlAnimationModel::commitKeyMove()
{
    if(isKeyMoveActive())
        _keyMoveTransaction.commit();
}

void QmlAnimationModel::cancelKeyMove()
{
    if(isKeyMoveActive())
        _keyMoveTransaction.cancel();
}

bool QmlAnimationModel::applyKeyShift(int frameDelta)
{
    if(_selectedKeys.targets().empty() || frameDelta == 0)
        return false;
    // Reduce the shift as far as the animation interval requires it, so that the selection stays together and no key
    // leaves the interval (the same rule the classic track bar applies while dragging).
    if(AnimationSettings* settings = animationSettings()) {
        for(const OORef<AnimationKey>& key : _selectedKeys.targets()) {
            const int newFrame = key->time().frame() + frameDelta;
            if(newFrame < settings->firstFrame())
                frameDelta += settings->firstFrame() - newFrame;
            if(newFrame > settings->lastFrame())
                frameDelta -= newFrame - settings->lastFrame();
        }
    }
    if(frameDelta == 0)
        return false;
    for(KeyframeController* ctrl : _controllers.targets())
        ctrl->moveKeys(_selectedKeys.targets(), AnimationTime::TicksPerFrame * frameDelta);
    return true;
}

// ----------------------------------------------------------------------------------------------------------------
// The visible range
// ----------------------------------------------------------------------------------------------------------------

bool QmlAnimationModel::setRange(int firstFrame, int lastFrame)
{
    AnimationSettings* settings = animationSettings();
    if(!settings || firstFrame > lastFrame)
        return false;
    firstFrame = std::clamp(firstFrame, settings->firstFrame(), settings->lastFrame());
    lastFrame = std::clamp(lastFrame, firstFrame, settings->lastFrame());
    if(firstFrame > lastFrame)
        return false;
    if(_rangeStart == firstFrame && _rangeEnd == lastFrame && _rangeIsExplicit)
        return true;
    _rangeIsExplicit = true;
    if(_rangeStart != firstFrame || _rangeEnd != lastFrame) {
        _rangeStart = firstFrame;
        _rangeEnd = lastFrame;
        Q_EMIT rangeChanged();
    }
    return true;
}

void QmlAnimationModel::resetRange()
{
    _rangeIsExplicit = false;
    syncRangeWithInterval();
}

void QmlAnimationModel::syncRangeWithInterval()
{
    if(_rangeIsExplicit)
        return;
    AnimationSettings* settings = animationSettings();
    const int first = settings ? settings->firstFrame() : 0;
    const int last = settings ? settings->lastFrame() : 0;
    if(_rangeStart != first || _rangeEnd != last) {
        _rangeStart = first;
        _rangeEnd = last;
        Q_EMIT rangeChanged();
    }
}

// ----------------------------------------------------------------------------------------------------------------
// Following the data set
// ----------------------------------------------------------------------------------------------------------------

void QmlAnimationModel::onAnimationSettingsReplaced(AnimationSettings* settings)
{
    _animationSettings.setTarget(settings);
    rebuildTracks();
    syncRangeWithInterval();
    Q_EMIT animationStateChanged();
}

void QmlAnimationModel::onDataSetChanged()
{
    rebuildTracks();
    syncRangeWithInterval();
    Q_EMIT animationStateChanged();
}

void QmlAnimationModel::onAnimationIntervalChanged(int firstFrame, int lastFrame)
{
    syncRangeWithInterval();
    Q_EMIT animationStateChanged();
}

void QmlAnimationModel::onAnimationSettingsNotificationEvent(RefTarget* source, const ReferenceEvent& event)
{
    // The interval has its own container signal, but the playback settings and the time display preference do not.
    if(event.type() == ReferenceEvent::TargetChanged)
        Q_EMIT animationStateChanged();
}

void QmlAnimationModel::onControllerNotificationEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(_updatingTracks)
        return;
    switch(event.type()) {
    case ReferenceEvent::TargetChanged:
    case ReferenceEvent::ReferenceChanged:
    case ReferenceEvent::ReferenceAdded:
    case ReferenceEvent::ReferenceRemoved:
        // A key was created, moved or removed, which changes the rows of this model.
        refreshKeys();
        break;
    case ReferenceEvent::TargetDeleted:
        // A controller disappeared, so the walk over the object graph has to be redone.
        scheduleRebuild();
        break;
    default:
        break;
    }
}

void QmlAnimationModel::onKeyNotificationEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(_updatingTracks)
        return;
    if(event.type() == ReferenceEvent::TargetChanged || event.type() == ReferenceEvent::TargetDeleted) {
        // A key moved (its position is a role of its row) or the key that was selected is gone.
        const bool selectionLost = event.type() == ReferenceEvent::TargetDeleted;
        refreshKeys();
        if(selectionLost)
            Q_EMIT this->selectionChanged();
    }
}

void QmlAnimationModel::onObjectNotificationEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(_updatingTracks)
        return;
    // A change of the object graph of a selected object means the set of animatable parameters may be a different one.
    // The objects are walked again from scratch, because the listeners of the objects that left the graph are stale.
    if(event.type() == ReferenceEvent::ReferenceChanged || event.type() == ReferenceEvent::ReferenceAdded || event.type() == ReferenceEvent::ReferenceRemoved) {
        if(!static_cast<const PropertyFieldEvent&>(event).field()->flags().testFlag(PROPERTY_FIELD_NO_SUB_ANIM)) {
            _objects.clear();
            scheduleRebuild();
        }
    }
}

void QmlAnimationModel::scheduleRebuild()
{
    if(_rebuildScheduled)
        return;
    _rebuildScheduled = true;
    QTimer::singleShot(0, this, [this]() {
        _rebuildScheduled = false;
        rebuildTracks();
    });
}

void QmlAnimationModel::rebuildTracks()
{
    if(_updatingTracks)
        return;
    _updatingTracks = true;

    _controllers.clear();
    _objects.clear();
    _parameterNames.clear();

    // The selection is a selection of keys of the tracks that exist now; a different set of tracks means a different
    // timeline, so the selection does not survive the rebuild (finding 5 of the Phase 3 audit describes the same
    // behaviour of the classic track bar).
    const bool hadSelection = !_selectedKeys.targets().empty();
    _selectedKeys.clear();

    // Traverse the object graphs of the selected scene nodes to find all animation controllers, exactly as the classic
    // track bar does.
    if(SelectionSet* selection = _ui.datasetContainer().activeSelectionSet()) {
        for(SceneNode* node : selection->nodes()) {
            findControllers(node);
            if(Pipeline* pipeline = node->pipeline())
                findControllers(pipeline);
        }
    }

    _updatingTracks = false;

    refreshKeys();
    if(hadSelection)
        Q_EMIT selectionChanged();
}

void QmlAnimationModel::findControllers(RefTarget* target)
{
    OVITO_CHECK_OBJECT_POINTER(target);

    bool hasSubAnimatables = false;

    for(const PropertyFieldDescriptor* field : target->getOOMetaClass().propertyFields()) {
        // A reference field that does not take part in the sub-animation of its owner cannot contribute a track.
        if(field->isReferenceField() && !field->flags().testFlag(PROPERTY_FIELD_NO_SUB_ANIM)) {
            hasSubAnimatables = true;
            if(!field->isVector()) {
                if(RefTarget* subTarget = target->getReferenceFieldTarget(field)) {
                    findControllers(subTarget);
                    addController(subTarget, target, field);
                }
            }
            else {
                const int count = target->getVectorReferenceFieldSize(field);
                for(int index = 0; index < count; index++) {
                    if(RefTarget* subTarget = target->getVectorReferenceFieldTarget(field, index)) {
                        findControllers(subTarget);
                        addController(subTarget, target, field);
                    }
                }
            }
        }
    }

    if(hasSubAnimatables)
        _objects.push_back(target);
}

void QmlAnimationModel::addController(RefTarget* target, RefTarget* owner, const PropertyFieldDescriptor* field)
{
    KeyframeController* ctrl = dynamic_object_cast<KeyframeController>(target);
    if(!ctrl)
        return;
    const QString parameterName = owner->objectTitle() + QStringLiteral(" - ") + field->displayName();
    const int index = _controllers.targets().indexOf(ctrl);
    if(index == -1) {
        _controllers.push_back(ctrl);
        _parameterNames.push_back(parameterName);
    }
    else if(!_parameterNames[index].contains(parameterName)) {
        // The same controller can be attached to several objects (a shared modifier, a shared visual element), and the
        // track is then the parameter of all of them.
        _parameterNames[index] += QStringLiteral(",") + parameterName;
    }
}

void QmlAnimationModel::refreshKeys()
{
    if(_updatingTracks)
        return;
    _updatingTracks = true;

    beginResetModel();
    // The keys of the tracks are observed as well, so that an edit of a single key reaches the rows that show it. The
    // rows are built in the order the tracks were collected in, which is what tells a row its track without a walk.
    QVector<AnimationKey*> keys;
    QVector<int> rowTracks;
    const auto& controllers = _controllers.targets();
    for(int index = 0; index < controllers.size(); index++) {
        for(const OORef<AnimationKey>& key : controllers[index]->keys()) {
            keys.push_back(key.get());
            rowTracks.push_back(index);
        }
    }
    _rowTracks = std::move(rowTracks);
    const auto& observed = _keys.targets();
    if(observed.size() != keys.size() || !std::equal(keys.begin(), keys.end(), observed.begin(), [](AnimationKey* key, const OORef<AnimationKey>& observedKey) { return key == observedKey.get(); })) {
        _keys.clear();
        for(AnimationKey* key : keys)
            _keys.push_back(key);
    }
    endResetModel();

    _updatingTracks = false;
    Q_EMIT keysChanged();
}

QString QmlAnimationModel::keyValueString(const AnimationKey* key)
{
    if(!key)
        return {};
    const QVariant value = key->valueQVariant();
    if(value.userType() == qMetaTypeId<FloatType>())
        return QString::number(value.value<FloatType>());
    if(value.userType() == qMetaTypeId<int>())
        return QString::number(value.value<int>());
    if(value.userType() == qMetaTypeId<Vector3>()) {
        const Vector3 vec = value.value<Vector3>();
        return QStringLiteral("(%1, %2, %3)").arg(vec.x()).arg(vec.y()).arg(vec.z());
    }
    if(value.userType() == qMetaTypeId<Rotation>()) {
        const Rotation rot = value.value<Rotation>();
        return QStringLiteral("axis (%1, %2, %3), angle: %4°").arg(rot.axis().x()).arg(rot.axis().y()).arg(rot.axis().z()).arg(qRadiansToDegrees(rot.angle()));
    }
    if(value.userType() == qMetaTypeId<Scaling>()) {
        const Scaling scaling = value.value<Scaling>();
        return QStringLiteral("(%1, %2, %3)").arg(scaling.S.x()).arg(scaling.S.y()).arg(scaling.S.z());
    }
    return value.toString();
}

}   // End of namespace Ovito
