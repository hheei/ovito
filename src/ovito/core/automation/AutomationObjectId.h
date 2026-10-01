// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>

namespace Ovito {

/**
 * \brief A stable, client-facing name for one object of a session.
 *
 * An interactive frontend addresses a scene object by pointer or by the row of a model. Neither survives the call:
 * a model row is a presentation index that changes when a modifier is inserted above it, and a pointer means nothing
 * to a client in another process. An automation client therefore addresses objects by an ID of this form:
 *
 * | Object | ID | Meaning |
 * |---|---|---|
 * | a scene node (a visible object of the scene) | `scenenode:s7` | the 7th scene node this session named |
 * | a pipeline (the data source chain of a scene node) | `pipeline:p42` | the 42nd pipeline this session named |
 * | a modification node (a modifier in a pipeline) | `modifier:m108` | the 108th modification node |
 * | a viewport | `viewport:v2` | the 2nd viewport |
 * | a property of an object | `property:m108/distance` | the field `distance` of that modification node |
 *
 * The number comes from AutomationObjectRegistry and is handed out once per object. It keeps its meaning while the
 * object lives, including across an undo/redo cycle that removes the object from the scene and puts it back, and it
 * is never reused after the object is gone: a client that keeps an ID across a data set replacement gets an
 * `invalidated_object` error rather than a different object that happens to have the same number.
 *
 * An earlier draft of the design used a semantic viewport ID such as `viewport:v-perspective`. A viewport's view type
 * is neither unique (two viewports may show the perspective view) nor stable (the user can change it), so it cannot
 * identify a viewport. A semantic alias can be added later next to the numeric ID; it must not replace it.
 */
class OVITO_CORE_EXPORT AutomationObjectId
{
public:

    /// The kinds of object an ID can name.
    enum class Kind {
        SceneNode,
        Pipeline,
        Modifier,
        Viewport,
        /// Not an object of its own: a property field of another object, named by its owner plus a field name.
        Property
    };

    /// An ID of the given object kind. Numbers start at 1; 0 yields an invalid ID.
    static AutomationObjectId forSceneNode(quint64 number);
    static AutomationObjectId forPipeline(quint64 number);
    static AutomationObjectId forModifier(quint64 number);
    static AutomationObjectId forViewport(quint64 number);

    /// The ID of a property field of an object. The owner kind must be an object kind, not Property.
    static AutomationObjectId forProperty(Kind ownerKind, quint64 ownerNumber, QString fieldName);

    /// Parses the textual form. Nothing when the text is not an ID of this grammar.
    static std::optional<AutomationObjectId> parse(const QString& text);

    /// Returns whether this ID is usable; a default-constructed one is not.
    bool isValid() const { return _number > 0; }

    /// The kind of the object this ID names, or Property for a property ID.
    Kind kind() const { return _kind; }

    /// Whether this ID names a property field rather than an object.
    bool isProperty() const { return _kind == Kind::Property; }

    /// The number of the object, or of the owner object for a property ID.
    quint64 number() const { return _number; }

    /// The kind of the object a property belongs to. Only meaningful when isProperty().
    Kind ownerKind() const { return _ownerKind; }

    /// The field name of a property. Only meaningful when isProperty().
    const QString& fieldName() const { return _fieldName; }

    /// The textual form; empty for an invalid ID.
    QString toString() const;

    /// The local part of an object ID, for example `"p42"`; empty for an invalid ID or a property ID.
    static QString localName(Kind kind, quint64 number);

    /// The wire name of an object kind, which is also the prefix of its IDs, for example `"pipeline"`. A client that
    /// wants to know what the ID it is holding refers to reads this, so it is part of the vocabulary and not an
    /// implementation detail of the grammar.
    static QString kindName(Kind kind);

private:

    Kind _kind = Kind::SceneNode;
    quint64 _number = 0;
    Kind _ownerKind = Kind::SceneNode;
    QString _fieldName;
};

}   // End of namespace
