// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include "LoadImageFileDialog.h"

namespace Ovito {

/******************************************************************************
* Constructs the dialog window.
******************************************************************************/
LoadImageFileDialog::LoadImageFileDialog(MainWindowUI& ui, QWidget* parent, const QString& caption, const ImageInfo& imageInfo) :
    HistoryFileDialog(ui, QStringLiteral("load_image"), parent, caption), _imageInfo(imageInfo)
{
    connect(this, &QFileDialog::fileSelected, this, &LoadImageFileDialog::onFileSelected);
    setAcceptMode(QFileDialog::AcceptOpen);
    setNameFilter(tr("Image files (*.png *.jpg *.jpeg)"));
    if(_imageInfo.filename().isEmpty() == false)
        selectFile(_imageInfo.filename());
}

/******************************************************************************
* This is called when the user has pressed the OK button of the dialog.
******************************************************************************/
void LoadImageFileDialog::onFileSelected(const QString& file)
{
    _imageInfo.setFilename(file);
}

}   // End of namespace
