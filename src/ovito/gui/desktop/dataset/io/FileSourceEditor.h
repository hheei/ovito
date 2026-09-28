// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/gui/desktop/properties/PropertiesPanel.h>
#include <ovito/gui/desktop/widgets/general/ElidedTextLabel.h>
#include <ovito/gui/desktop/widgets/display/StatusWidget.h>

namespace Ovito {

/**
 * A properties editor for the FileSource object.
 */
class FileSourceEditor : public PropertiesEditor
{
    OVITO_CLASS(FileSourceEditor)
    Q_OBJECT

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// Loads a new file into the FileSource.
    void importNewFile(FileSource* fileSource, const QUrl& url, OvitoClassPtr importerType, const QString& importerFormat);

protected Q_SLOTS:

    /// Is called when the user presses the "Pick local input file" button.
    void onPickLocalInputFile();

    /// Is called when the user presses the "Pick remote input file" button.
    void onPickRemoteInputFile();

    /// Is called when the user presses the Reload frame button.
    void onReloadFrame();

    /// Is called when the user presses the Reload animation button.
    void onReloadAnimation();

    /// Updates the displayed status information.
    void updateDisplayedInformation();

    /// Updates the list of trajectory frames displayed in the UI.
    void updateFramesList();

    /// This is called when the user has changed the source URL.
    void onWildcardPatternEntered();

    /// Is called when the user has selected a certain frame in the frame list box.
    void onFrameSelected(int index);

private:

    QLineEdit* _filenameLabel;
    QLineEdit* _sourcePathLabel;
    QLineEdit* _wildcardPatternTextbox;
    QLabel* _fileSeriesLabel;
    QLabel* _timeSeriesLabel = nullptr;
    StatusWidget* _statusLabel;
    QComboBox* _framesListBox = nullptr;
    QStringListModel* _framesListModel = nullptr;
    QLabel* _playbackRatioDisplay = nullptr;
    QPushButton* _editPlaybackBtn = nullptr;
    bool _deferredDisplayUpdatePending = false;
};

}   // End of namespace
