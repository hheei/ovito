// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/import/lammps/LAMMPSDataImporter.h>
#include <ovito/gui/desktop/dataset/io/FileImporterEditor.h>

namespace Ovito {

/**
 * \brief A properties editor for the LAMMPSDataImporter class.
 */
class LAMMPSDataImporterEditor : public FileImporterEditor
{
    OVITO_CLASS(LAMMPSDataImporterEditor)
    Q_OBJECT

public:

    /// This is called by the system when the user has selected a new file to import.
    virtual void inspectNewFile(FileImporter* importer, const QUrl& sourceFile) override;

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;
};

/**
 * This dialog box lets the user choose a LAMMPS atom style.
 */
class LAMMPSAtomStyleDialog : public QDialog
{
    Q_OBJECT

public:

    /// Constructor.
    LAMMPSAtomStyleDialog(MainWindowUI& ui, LAMMPSDataImporter::LAMMPSAtomStyleHints& atomStyleHints, QWidget* parentWindow = nullptr);

private Q_SLOTS:

    /// Updates the displayed list of file data columns.
    void updateColumnList();

    /// Saves the values entered by the user and closes the dialog.
    void onOk();

private:

    LAMMPSDataImporter::LAMMPSAtomStyleHints& _atomStyleHints;
    QComboBox* _atomStyleList;
    QLabel* _subStylesLabel;
    std::array<QComboBox*,3> _subStyleLists;
    QLineEdit* _columnListField;
    QLabel* _columnMismatchLabel;
    QDialogButtonBox* _buttonBox;
};

}   // End of namespace
