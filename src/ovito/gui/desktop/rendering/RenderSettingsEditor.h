// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/dataset/animation/TimeInterval.h>

namespace Ovito {

/**
 * The editor component for the RenderSettings class.
 */
class RenderSettingsEditor : public PropertiesEditor
{
    OVITO_CLASS(RenderSettingsEditor)

public:

    /// Constructor.
    using PropertiesEditor::PropertiesEditor;

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

    /// This method is called when a referenced object has changed.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// Is called when the value of a reference field of this object changes.
    virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

private Q_SLOTS:

    /// Lets the user choose a filename for the output image.
    void onChooseImageFilename();

    /// Is called when the user selects an output size preset from the drop-down list.
    void onSizePresetActivated(int index);

    /// Lets the user choose a different plug-in rendering engine.
    void onSwitchRenderer();

    /// This is called when another viewport became active.
    void onActiveViewportChanged(Viewport* activeViewport);

    /// Is called when the user toggles the preview mode checkbox.
    void onViewportPreviewModeToggled(bool checked);

    /// Updates the displayed video length based on the current render settings.
    void updateVideoLengthDisplay();

    /// Updates the external FFmpeg label.
    void updateExternalFFmpegLabel();

private:

    /// Reference to the currently active viewport.
    DECLARE_REFERENCE_FIELD(OORef<Viewport>, activeViewport);

    QComboBox* _sizePresetsBox;
    QCheckBox* _viewportPreviewModeBox;
    QLabel* _videoLengthLabel;
    QLabel* _externalFFmpegLabel;
};

}   // End of namespace
