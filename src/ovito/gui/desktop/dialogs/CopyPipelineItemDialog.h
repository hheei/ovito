// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * This dialog box lets the user make a copy of a pipeline scene node.
 */
class CopyPipelineItemDialog : public QDialog, public UserInterfaceComponent<MainWindowUI>
{
    Q_OBJECT

public:

    /// Constructor.
    CopyPipelineItemDialog(MainWindowUI& ui, QWidget* parentWindow, Pipeline* sourcePipeline, std::vector<OORef<PipelineNode>> pipelineNodes);

private Q_SLOTS:

    /// Is called when the user presses the 'Ok' button.
    void onAccept();

private:

    /// The source pipeline.
    OORef<Pipeline> _sourcePipeline;

    /// The pipeline nodes to be copied.
    std::vector<OORef<PipelineNode>> _pipelineNodes;

    /// Target pipeline selector.
    QComboBox* _destinationPipelineList;

    /// Selects the insertion position.
    QRadioButton* _insertAtEndBtn;
    QRadioButton* _insertAtStartBtn;

    /// Controls the cloning mode.
    QCheckBox* _shareBetweenPipelinesBox;
};

}   // End of namespace
