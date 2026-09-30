// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/automation/AutomationObjectId.h>
#include <ovito/core/oo/OORef.h>

#include <QHash>
#include <QSet>

#include <unordered_map>

namespace Ovito {

class OvitoObject;

/**
 * \brief Hands out the stable object IDs of one session and resolves them back to objects.
 *
 * The registry is the only place that knows which object is `modifier:m108`. It hands a number out the first time it
 * sees an object and returns the same ID for as long as the object lives, which is what makes an ID survive a
 * presentation refresh, a model rebuild, or an undo/redo cycle that takes the object out of the scene and puts it
 * back: OVITO keeps the object alive while the undo stack references it, so the registry keeps its ID.
 *
 * Objects are held weakly. An ID whose object has been destroyed resolves to nothing, which the gateway reports as
 * `unknown_object` (never seen) or `invalidated_object` (seen, but the identity is gone). Numbers are never reused,
 * not even by invalidateAll(), so an ID from before a data set replacement can never silently name a different object
 * afterwards; the session revision is what tells a client that its snapshot is outdated.
 *
 * The registry is not thread-safe: it is owned by AutomationSession and used from the thread that owns the session.
 */
class OVITO_CORE_EXPORT AutomationObjectRegistry
{
public:

    AutomationObjectRegistry() = default;
    ~AutomationObjectRegistry();

    /// See the MSVC note in RendererService: an exported class must not rely on an implicitly generated copy operation
    /// being available, and this type is not copyable anyway.
    AutomationObjectRegistry(const AutomationObjectRegistry&) = delete;
    AutomationObjectRegistry& operator=(const AutomationObjectRegistry&) = delete;
    AutomationObjectRegistry(AutomationObjectRegistry&&) = delete;
    AutomationObjectRegistry& operator=(AutomationObjectRegistry&&) = delete;

    /**
     * \brief Returns the ID of an object, allocating one the first time the object is seen.
     * \param object The object to name. May be null, which yields an empty string. It is not const, because the
     *               registry holds the object weakly and a caller has to be able to act on what it resolves.
     * \param kind The kind of object this is; the ID's prefix and letter come from it.
     */
    QString idFor(OvitoObject* object, AutomationObjectId::Kind kind);

    /// Returns the ID an object already has, without allocating one.
    std::optional<QString> existingId(const OvitoObject* object) const;

    /// Returns the ID of a property field of an object; see AutomationObjectId::forProperty().
    QString propertyIdFor(OvitoObject* owner, AutomationObjectId::Kind ownerKind, const QString& fieldName);

    /// Resolves an ID back to its object. Nothing when the ID is malformed, was never handed out, or is invalidated.
    OORef<OvitoObject> resolve(const QString& id) const;

    /// Resolves a property ID back to its owner object. Nothing for the same reasons as resolve().
    OORef<OvitoObject> resolveOwner(const AutomationObjectId& id) const;

    /// Reports whether an object was ever named by this registry. A true answer does not mean the ID still resolves.
    bool wasAssigned(const QString& id) const;

    /// Forgets one object without reusing its number.
    void invalidate(const OvitoObject* object);

    /**
     * \brief Forgets every object, which is what a data set replacement has to do.
     *
     * The number counters keep running, so nothing after the replacement can be mistaken for a pre-replacement
     * object.
     */
    void invalidateAll();

    /// The number of objects the registry currently holds an ID for, expired ones excluded.
    qsizetype objectCount() const;

private:

    struct Entry {
        /// A weak reference, because naming an object must not keep it alive. It is typed to a mutable object: a
        /// client that resolves an ID is a client that means to act on it.
        OOWeakRef<OvitoObject> object;
        QString id;
        AutomationObjectId::Kind kind;
    };

    /// IDs by object, the direction used when an object is named.
    std::unordered_map<const OvitoObject*, Entry> _entries;
    /// The reverse direction, used when a client resolves an ID. It holds the raw pointer, which stays valid while the
    /// weak reference in _entries does; resolve() checks that it is still alive.
    QHash<QString, OvitoObject*> _idsByText;
    /// Every ID handed out since the last invalidateAll(), including the ones whose object has died. This is what
    /// separates "unknown object" from "invalidated object".
    QSet<QString> _assignedIds;
    quint64 _nextSceneNode = 1;
    quint64 _nextPipeline = 1;
    quint64 _nextModifier = 1;
    quint64 _nextViewport = 1;
};

}   // End of namespace
