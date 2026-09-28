// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/dataset/io/FileExporter.h>

namespace Ovito {

/**
 * \brief This dialog box lets the user adjust the settings of a FileExporter.
 */
class OVITO_GUI_EXPORT FileExporterSettingsDialog : public QDialog, public UserInterfaceComponent<MainWindowUI>
{
    Q_OBJECT

public:

    /// Constructor.
    FileExporterSettingsDialog(MainWindowUI& ui, Scene& scene, FileExporter* exporter, QWidget* parent);

    virtual int exec() override {
        // If there is no animation sequence (just a single frame), and if the exporter does not expose any other settings,
        // then we can skip showing the dialog box altogether.
        if(_skipDialog)
            return QDialog::Accepted;
        return QDialog::exec();
    }

protected Q_SLOTS:

    /// This is called when the user has pressed the OK button.
    virtual void onOk();

    /// Updates the displayed list of data object available for export.
    void updateDataObjectList();

protected:

    QVBoxLayout* _mainLayout;
    OORef<FileExporter> _exporter;
    SpinnerWidget* _startTimeSpinner;
    SpinnerWidget* _endTimeSpinner;
    SpinnerWidget* _nthFrameSpinner;
    QLineEdit* _wildcardTextbox;
    QButtonGroup* _fileGroupButtonGroup = nullptr;
    QButtonGroup* _rangeButtonGroup;
    QComboBox* _sceneNodeBox;
    QComboBox* _dataObjectBox;
    bool _skipDialog = true;
};

}   // End of namespace
