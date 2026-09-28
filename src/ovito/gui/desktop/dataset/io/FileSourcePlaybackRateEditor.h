// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * A properties editor for the FileSource object.
 */
class FileSourcePlaybackRateEditor : public PropertiesEditor
{
    OVITO_CLASS(FileSourcePlaybackRateEditor)
    Q_OBJECT

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

private Q_SLOTS:

    /// Updates the displayed information.
    void updateInformation();

    /// Updates the list of trajectory frames displayed in the UI.
    void updateFramesList();

private:

    QComboBox* _framesListBox;
    QStringListModel* _framesListModel;
    QLabel* _numTrajectoryFramesDisplay;
    QLabel* _numAnimationFramesDisplay;
    QRadioButton* _trajectoryModeBtn;
    QRadioButton* _staticFrameModeBtn;
    IntegerParameterUI* _staticFrameNumberUI;
};

}   // End of namespace
