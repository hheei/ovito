// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/core/oo/RefTargetListener.h>
#include <ovito/core/dataset/animation/TimeInterval.h>
#include <QAbstractListModel>
#include <QStringList>

namespace Ovito {

class WorkbenchUI;
class AnimationSettings;
class KeyframeController;
class AnimationKey;

/**
 * \brief The model side of the animation timeline: the animation state, the animation keys of the selected objects and
 *        the operations a timeline performs on them.
 *
 * The timeline itself belongs to Phase 5 of the migration plan; what this class provides is everything such a widget
 * binds to, without presenting anything (audit decision D53). It is the model of two things the classic frontend keeps
 * apart - the animation settings plus the playback state (the `AnimationTimeSlider` and the animation toolbar) and the
 * animation keys (`AnimationTrackBar`) - and it adds the two things neither offers a QML view:
 *
 * 1. **The keys are rows.** The model is a list model whose row is one animation key of one animated parameter, with
 *    `track`, `parameter`, `frame`, `time`, `value` and `selected` roles, so a QML view can draw and hit-test them
 *    without a custom-painted widget. `trackCount`/`trackParameterName()`/`trackKeyCount()` describe the tracks those
 *    rows belong to. Every key is listed, including the first key of a parameter that the classic track bar hides while
 *    a controller has fewer than two of them (a drawing decision of that widget, not a property of the data).
 * 2. **Selection and visible range are model state.** The classic track bar keeps the selected keys in a widget-local,
 *    non-undoable list that is rebuilt whenever the object selection changes (finding 5 of the Phase 3 audit). Here the
 *    selection and the visible range are presentation state of the model (D56): not part of the data set, not undoable,
 *    and dropped when the underlying keys are.
 *
 * What it deliberately does *not* own:
 *
 * * **The data-set edits' undo semantics.** Moving and deleting keys and changing the interval or the playback settings
 *   are one named undo transaction each, exactly like the commands the classic frontend uses; a *continuous* key drag
 *   uses one transaction for the whole gesture through beginKeyMove()/updateKeyMove()/commitKeyMove()/cancelKeyMove()
 *   (D58).
 * * **Playback.** Whether the animation is playing is core's `SceneAnimationPlayback`, reached through
 *   `DataSetContainer::isPlaybackActive()`/`setAnimationPlayback()`; this model only mirrors it, and the shared
 *   `AnimationTogglePlayback` command is what starts and stops it (D56).
 * * **Creating keys.** A key is created by editing an animatable parameter (auto-key mode) or by the key editor of the
 *   property inspector, not by a timeline - the classic track bar cannot create one either.
 *
 * \note The walk that finds the animation controllers of the selected objects is the classic track bar's
 *       (`AnimationTrackBar::findControllers()`): it recurses over the reference fields of the selected scene nodes and
 *       keeps every `KeyframeController` it finds, with the field it belongs to as the parameter name. Unifying the two
 *       walks is the work of the phase that replaces the track bar with a QML timeline (audit open item O24).
 */
class OVITO_GUIQML_EXPORT QmlAnimationModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int firstFrame READ firstFrame NOTIFY animationStateChanged)
    Q_PROPERTY(int lastFrame READ lastFrame NOTIFY animationStateChanged)
    Q_PROPERTY(int currentFrame READ currentFrame NOTIFY animationStateChanged)
    Q_PROPERTY(int numberOfFrames READ numberOfFrames NOTIFY animationStateChanged)
    Q_PROPERTY(bool isSingleFrame READ isSingleFrame NOTIFY animationStateChanged)
    Q_PROPERTY(QString currentTimeString READ currentTimeString NOTIFY animationStateChanged)
    Q_PROPERTY(double framesPerSecond READ framesPerSecond NOTIFY animationStateChanged)
    Q_PROPERTY(int playbackSpeed READ playbackSpeed NOTIFY animationStateChanged)
    Q_PROPERTY(bool loopPlayback READ loopPlayback NOTIFY animationStateChanged)
    Q_PROPERTY(int playbackEveryNthFrame READ playbackEveryNthFrame NOTIFY animationStateChanged)
    Q_PROPERTY(bool autoAdjustInterval READ autoAdjustInterval NOTIFY animationStateChanged)
    Q_PROPERTY(bool preferSimulationTimeDisplay READ preferSimulationTimeDisplay NOTIFY animationStateChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY animationStateChanged)

    Q_PROPERTY(int trackCount READ trackCount NOTIFY keysChanged)
    Q_PROPERTY(int keyCount READ keyCount NOTIFY keysChanged)
    Q_PROPERTY(int selectedKeyCount READ selectedKeyCount NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList selectedKeys READ selectedKeys NOTIFY selectionChanged)

    Q_PROPERTY(int rangeStart READ rangeStart NOTIFY rangeChanged)
    Q_PROPERTY(int rangeEnd READ rangeEnd NOTIFY rangeChanged)

public:

    /// The roles of a key row.
    enum Role {
        TrackRole = Qt::UserRole + 1,   ///< The index of the track (the animated parameter) the key belongs to.
        ParameterRole,                  ///< The name of that track, e.g. "Simulation cell - Transformation".
        FrameRole,                      ///< The animation frame the key is positioned at.
        TimeRole,                       ///< That position as the animation time string the animation settings format it as.
        ValueRole,                      ///< The value of the key, formatted.
        SelectedRole                    ///< Whether the key is selected.
    };
    Q_ENUM(Role)

public:

    /// Constructor. \a ui supplies the data set container the animation state comes from, the undo stack the edits are
    /// recorded on and the machine-facing session the object IDs of the scene belong to.
    explicit QmlAnimationModel(WorkbenchUI& ui, QObject* parent = nullptr);

    /// Returns the number of key rows.
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    /// Returns the data of a key row.
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    /// Returns the names of the roles, so that a QML delegate can read them.
    virtual QHash<int, QByteArray> roleNames() const override;

    // ---- The animation state ----

    int firstFrame() const;
    int lastFrame() const;
    int currentFrame() const;
    int numberOfFrames() const;
    bool isSingleFrame() const;
    QString currentTimeString() const;
    double framesPerSecond() const;
    int playbackSpeed() const;
    bool loopPlayback() const;
    int playbackEveryNthFrame() const;
    bool autoAdjustInterval() const;
    bool preferSimulationTimeDisplay() const;

    /// Returns whether the animation is being played back in the viewports (core's playback state, not this model's).
    bool isPlaying() const;

    /// Converts a frame number to the string the animation settings display it as, e.g. "3.5 ns" for a trajectory that
    /// carries simulation times.
    Q_INVOKABLE QString timeStringForFrame(int frame) const;

    /// Converts such a string back to a frame number.
    /// \return The frame, or an invalid value when the string cannot be parsed.
    Q_INVOKABLE QVariant frameFromTimeString(const QString& text) const;

    /// Moves the current time to the given frame, clamped to the animation interval.
    Q_INVOKABLE void goToFrame(int frame);

    /// Sets the animation interval to the given frames, as one undo step.
    /// \return \c false when the interval is empty (first > last) or cannot be set.
    Q_INVOKABLE bool setInterval(int firstFrame, int lastFrame);

    /// Sets the number of frames the animation is played back per second, as one undo step.
    Q_INVOKABLE bool setFramesPerSecond(double framesPerSecond);

    /// Sets the playback speed factor, as one undo step.
    Q_INVOKABLE bool setPlaybackSpeed(int speed);

    /// Sets whether the animation is played back in a loop, as one undo step.
    Q_INVOKABLE bool setLoopPlayback(bool loop);

    /// Sets how many frames are skipped during playback, as one undo step.
    Q_INVOKABLE bool setPlaybackEveryNthFrame(int n);

    /// Sets whether the animation interval follows the loaded source animations, as one undo step.
    Q_INVOKABLE bool setAutoAdjustInterval(bool adjust);

    /// Sets whether the timeline prefers simulation time over frame numbers, as one undo step.
    Q_INVOKABLE bool setPreferSimulationTimeDisplay(bool prefer);

    // ---- The tracks ----

    /// Returns the number of animated parameters of the selected objects.
    int trackCount() const { return (int)_controllers.targets().size(); }

    /// Returns the name of a track, or an empty string when it does not exist.
    Q_INVOKABLE QString trackParameterName(int track) const;

    /// Returns the number of keys a track has.
    Q_INVOKABLE int trackKeyCount(int track) const;

    /// Returns the frame of a track's key, or -1 when there is no such key.
    Q_INVOKABLE int trackKeyFrame(int track, int key) const;

    // ---- The keys ----

    /// Returns the number of key rows.
    int keyCount() const;

    /// Returns the number of selected keys.
    int selectedKeyCount() const { return (int)_selectedKeys.targets().size(); }

    /// Returns the selected keys as a list of maps with the same fields as the roles of a row.
    QVariantList selectedKeys() const;

    /// Selects the key of a row, optionally in addition to the current selection (a view with Ctrl held).
    Q_INVOKABLE bool selectKey(int row, bool toggle = false);

    /// Selects every key that lies at the given frame, optionally in addition to the current selection, which is what
    /// clicking a position of the timeline does: several parameters can have a key at the same frame.
    Q_INVOKABLE bool selectKeysAtFrame(int frame, bool toggle = false);

    /// Selects every key.
    Q_INVOKABLE void selectAllKeys();

    /// Clears the key selection.
    Q_INVOKABLE void clearKeySelection();

    /// Moves the current time to the frame of the first selected key.
    Q_INVOKABLE bool goToSelectedKey();

    /// Moves the selected keys by the given number of frames, as one undo step. The shift is reduced as far as the
    /// animation interval requires, so no key leaves the interval.
    /// \return \c false when nothing is selected or the keys cannot move.
    Q_INVOKABLE bool moveSelectedKeys(int frameDelta);

    /// Deletes the selected keys, as one undo step.
    Q_INVOKABLE bool deleteSelectedKeys();

    /// Begins a continuous key drag (D58): the gesture owns one undo transaction until commitKeyMove() accepts it or
    /// cancelKeyMove() restores the positions the gesture started from.
    Q_INVOKABLE void beginKeyMove();

    /// Moves the selected keys by the given number of frames relative to where the gesture started, which is what a
    /// mouse move of a drag does. Intermediate calls replace the previous position instead of adding to it.
    Q_INVOKABLE bool updateKeyMove(int frameDelta);

    /// Accepts the key drag as one undo step.
    Q_INVOKABLE void commitKeyMove();

    /// Cancels the key drag and restores the positions the gesture started from.
    Q_INVOKABLE void cancelKeyMove();

    /// Returns whether a key drag is in progress.
    bool isKeyMoveActive() const { return _keyMoveTransaction.operation() != nullptr; }

    // ---- The visible range (presentation state, not part of the data set) ----

    /// Returns the first frame the timeline shows. Unless it was set explicitly, this is the first frame of the
    /// animation interval.
    int rangeStart() const { return _rangeStart; }

    /// Returns the last frame the timeline shows.
    int rangeEnd() const { return _rangeEnd; }

    /// Sets the range the timeline shows, clamped to the animation interval.
    /// \return \c false when the range would be empty.
    Q_INVOKABLE bool setRange(int firstFrame, int lastFrame);

    /// Makes the timeline show the whole animation interval again.
    Q_INVOKABLE void resetRange();

Q_SIGNALS:

    /// Is emitted when the animation interval, the current time, a playback setting or the playback state changed.
    void animationStateChanged();

    /// Is emitted when the set of keys changed, i.e. when a key was created, deleted or moved.
    void keysChanged();

    /// Is emitted when the key selection changed.
    void selectionChanged();

    /// Is emitted when the visible range changed.
    void rangeChanged();

private:

    /// Returns the active animation settings of the current data set, or null when there is none.
    AnimationSettings* animationSettings() const;

    /// Walks the object graphs of the selected scene nodes and collects the animation controllers, as the classic
    /// track bar does.
    void rebuildTracks();

    /// Recurses over the reference fields of an object, collecting the controllers among them.
    void findControllers(RefTarget* target);

    /// Adds a controller to the list of tracks if it is one, naming it after the field it belongs to.
    void addController(RefTarget* target, RefTarget* owner, const PropertyFieldDescriptor* field);

    /// Rebuilds the key rows from the controllers, keeping the current selection.
    void refreshKeys();

    /// Returns the key of a row, or null when the row does not exist.
    AnimationKey* keyAt(int row) const;

    /// Returns the index of the track a key belongs to, or -1.
    int trackOfKey(const AnimationKey* key) const;

    /// Returns whether a key is part of the current selection.
    bool isSelected(const AnimationKey* key) const;

    /// Reads the visible range from the animation interval again, unless it was set explicitly.
    void syncRangeWithInterval();

    /// Returns the controller of a track, or null.
    KeyframeController* trackController(int track) const;

    /// Applies a shift to the selected keys inside the given transaction, clamped to the animation interval.
    /// \return \c false when nothing could move.
    bool applyKeyShift(int frameDelta);

    /// Formats the value of a key for display, as the classic track bar does it.
    static QString keyValueString(const AnimationKey* key);

    // ---- Following the data set ----

    /// Is called when the current data set has a different animation settings object.
    void onAnimationSettingsReplaced(AnimationSettings* settings);

    /// Is called when a different data set became the current one.
    void onDataSetChanged();

    /// Is called when the length of the animation interval changed.
    void onAnimationIntervalChanged(int firstFrame, int lastFrame);

    /// Is called when a property of the animation settings object changed.
    void onAnimationSettingsNotificationEvent(RefTarget* source, const ReferenceEvent& event);

    /// Is called when an animation controller changed, i.e. when a key was created, moved or deleted.
    void onControllerNotificationEvent(RefTarget* source, const ReferenceEvent& event);

    /// Is called when one of the observed keys changed or was deleted.
    void onKeyNotificationEvent(RefTarget* source, const ReferenceEvent& event);

    /// Is called when an object the walk found changed its reference fields, which may change the set of tracks.
    void onObjectNotificationEvent(RefTarget* source, const ReferenceEvent& event);

    /// Schedules a rebuild of the tracks on the next event loop iteration, coalescing a burst of changes.
    void scheduleRebuild();

private:

    /// The user interface this model belongs to.
    WorkbenchUI& _ui;

    /// The animation controllers of the selected objects, and their parameter names.
    VectorRefTargetListener<KeyframeController> _controllers;
    QStringList _parameterNames;

    /// The objects whose reference fields were walked, so that a structural change can be noticed and the walk redone.
    VectorRefTargetListener<RefTarget> _objects;

    /// The animation keys of the controllers, so that a moved or edited key is noticed.
    VectorRefTargetListener<AnimationKey> _keys;

    /// The selected keys.
    VectorRefTargetListener<AnimationKey> _selectedKeys;

    /// The undo transaction of a continuous key drag, if one is in progress.
    UndoableTransaction _keyMoveTransaction;

    /// The animation settings object being observed, so that a playback setting changed by another editor is noticed.
    RefTargetListener<AnimationSettings> _animationSettings;

    /// Whether a rebuild of the tracks has been scheduled (a structural change arrives in a burst).
    bool _rebuildScheduled = false;

    /// True while the model updates its own listeners, so that the notifications that causes do not re-enter.
    bool _updatingTracks = false;

    /// The visible range of the timeline. Kept in sync with the animation interval until it is set explicitly.
    int _rangeStart = 0;
    int _rangeEnd = 0;
    bool _rangeIsExplicit = false;
};

}   // End of namespace Ovito
