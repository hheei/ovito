// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/automation/AutomationObjectRegistry.h>
#include <ovito/core/oo/OvitoObject.h>

namespace Ovito {

AutomationObjectRegistry::~AutomationObjectRegistry() = default;

/******************************************************************************
* Returns the ID of an object, allocating one the first time the object is seen.
******************************************************************************/
QString AutomationObjectRegistry::idFor(OvitoObject* object, AutomationObjectId::Kind kind)
{
    if(!object)
        return {};
    OVITO_ASSERT_MSG(kind != AutomationObjectId::Kind::Property, "AutomationObjectRegistry::idFor()", "A property is named by its owner.");

    if(auto it = _entries.find(object); it != _entries.end()) {
        if(!it->second.object.expired())
            return it->second.id;
        // The object that used to live at this address is gone, and a new one has taken its place. Both directions of
        // the lookup have to forget it before the new object can be named.
        _idsByText.remove(it->second.id);
        _entries.erase(it);
    }

    // Hand out the next free number of this kind. Numbers are never reused, so an ID a client still holds can never
    // address an object that appeared later.
    AutomationObjectId id;
    switch(kind) {
        case AutomationObjectId::Kind::SceneNode: id = AutomationObjectId::forSceneNode(_nextSceneNode++); break;
        case AutomationObjectId::Kind::Pipeline: id = AutomationObjectId::forPipeline(_nextPipeline++); break;
        case AutomationObjectId::Kind::Modifier: id = AutomationObjectId::forModifier(_nextModifier++); break;
        case AutomationObjectId::Kind::Viewport: id = AutomationObjectId::forViewport(_nextViewport++); break;
        case AutomationObjectId::Kind::Property: OVITO_ASSERT(false); return {};
    }

    Entry entry;
    entry.object = object;
    entry.id = id.toString();
    entry.kind = kind;
    _idsByText.insert(entry.id, object);
    _entries.emplace(object, std::move(entry));
    _assignedIds.insert(id.toString());
    return id.toString();
}

/******************************************************************************
* Returns the ID an object already has, without allocating one.
******************************************************************************/
std::optional<QString> AutomationObjectRegistry::existingId(const OvitoObject* object) const
{
    const auto it = _entries.find(object);
    if(it != _entries.end() && !it->second.object.expired())
        return it->second.id;
    return std::nullopt;
}

/******************************************************************************
* Returns the ID of a property field of an object.
******************************************************************************/
QString AutomationObjectRegistry::propertyIdFor(OvitoObject* owner, AutomationObjectId::Kind ownerKind, const QString& fieldName)
{
    // Name the owner first, so a client never sees a property ID whose object it cannot resolve.
    const QString ownerId = idFor(owner, ownerKind);
    if(ownerId.isEmpty())
        return {};
    const std::optional<AutomationObjectId> parsed = AutomationObjectId::parse(ownerId);
    OVITO_ASSERT(parsed && !parsed->isProperty());
    const QString id = AutomationObjectId::forProperty(parsed->kind(), parsed->number(), fieldName).toString();
    _assignedIds.insert(id);
    return id;
}

/******************************************************************************
* Resolves an ID back to its object.
******************************************************************************/
OORef<OvitoObject> AutomationObjectRegistry::resolve(const QString& id) const
{
    const std::optional<AutomationObjectId> parsed = AutomationObjectId::parse(id);
    if(!parsed || parsed->isProperty())
        return nullptr;
    const auto it = _idsByText.constFind(id);
    if(it == _idsByText.constEnd())
        return nullptr;
    // The object may have died since the ID was handed out; then the ID is invalidated and does not resolve.
    return OOWeakRef<OvitoObject>(it.value()).lock();
}

/******************************************************************************
* Resolves the owner object of a property ID.
******************************************************************************/
OORef<OvitoObject> AutomationObjectRegistry::resolveOwner(const AutomationObjectId& id) const
{
    if(!id.isProperty())
        return nullptr;
    AutomationObjectId ownerId;
    switch(id.ownerKind()) {
        case AutomationObjectId::Kind::SceneNode: ownerId = AutomationObjectId::forSceneNode(id.number()); break;
        case AutomationObjectId::Kind::Pipeline: ownerId = AutomationObjectId::forPipeline(id.number()); break;
        case AutomationObjectId::Kind::Modifier: ownerId = AutomationObjectId::forModifier(id.number()); break;
        case AutomationObjectId::Kind::Viewport: ownerId = AutomationObjectId::forViewport(id.number()); break;
        case AutomationObjectId::Kind::Property: return nullptr;
    }
    return resolve(ownerId.toString());
}

/******************************************************************************
* Reports whether an object was ever named by this registry.
******************************************************************************/
bool AutomationObjectRegistry::wasAssigned(const QString& id) const
{
    return _assignedIds.contains(id);
}

/******************************************************************************
* Forgets one object without reusing its number.
******************************************************************************/
void AutomationObjectRegistry::invalidate(const OvitoObject* object)
{
    const auto it = _entries.find(object);
    if(it == _entries.end())
        return;
    _idsByText.remove(it->second.id);
    _entries.erase(it);
}

/******************************************************************************
* Forgets every object.
******************************************************************************/
void AutomationObjectRegistry::invalidateAll()
{
    // The set of assigned IDs is deliberately kept: after a data set replacement an old ID must still be recognized as
    // a name this session handed out, so the gateway answers `invalidated_object` and not `unknown_object`. The number
    // counters keep running as well, so a name from before the replacement cannot be attached to a new object.
    _entries.clear();
    _idsByText.clear();
}

/******************************************************************************
* Returns the number of objects the registry currently holds an ID for.
******************************************************************************/
qsizetype AutomationObjectRegistry::objectCount() const
{
    return std::count_if(_entries.begin(), _entries.end(), [](const auto& item) { return !item.second.object.expired(); });
}

}   // End of namespace
