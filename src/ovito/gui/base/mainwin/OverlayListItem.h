// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/oo/RefMaker.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/viewport/overlays/ViewportOverlay.h>

namespace Ovito {

/**
 * An item of the OverlayListModel representing a ViewportOverlay attached to a Viewport.
 */
class OVITO_GUIBASE_EXPORT OverlayListItem : public QObject, public RefMaker
{
    OVITO_CLASS(OverlayListItem)
    Q_OBJECT

public:

    enum OverlayItemType {
        Layer,
        ViewportHeader,
        SceneLayer,
    };

public:

    /// Constructor.
    void initializeObject(ViewportOverlay* overlay, OverlayItemType itemType);

    /// Returns the status of the object represented by the list item.
    const PipelineStatus& status() const;

    /// Returns the title text for this list item.
    QString title(Viewport* selectedViewport) const;

    /// Returns the type of this list item.
    OverlayItemType itemType() const { return _itemType; }

    /// Returns a short piece of information (typically a string) to be displayed next to the object's title in the UI.
    QVariant shortInfo(Viewport* selectedViewport) const;

Q_SIGNALS:

    /// This signal is emitted when this item has changed.
    void itemChanged(OverlayListItem* item);

protected:

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

private:

    /// The overlay represented by this item in the list box.
    DECLARE_REFERENCE_FIELD(OORef<ViewportOverlay>, overlay);

    /// The type of this list item.
    OverlayItemType _itemType;
};

}   // End of namespace
