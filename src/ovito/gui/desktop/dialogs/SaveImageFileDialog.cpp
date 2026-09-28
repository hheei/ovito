// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/utilities/io/video/VideoEncoder.h>
#include "SaveImageFileDialog.h"

namespace Ovito {

/******************************************************************************
* Constructs the dialog window.
******************************************************************************/
SaveImageFileDialog::SaveImageFileDialog(MainWindowUI& ui, QWidget* parent, const QString& caption, bool includeVideoFormats, const ImageInfo& imageInfo) :
    HistoryFileDialog(ui, QStringLiteral("save_image"), parent, caption), _imageInfo(imageInfo)
{
    connect(this, &QFileDialog::fileSelected, this, &SaveImageFileDialog::onFileSelected);
    connect(this, &QFileDialog::filterSelected, this, &SaveImageFileDialog::onFilterSelected);

    // Build filter string.
    QStringList filterStrings;
    QList<QByteArray> supportedFormats = QImageWriter::supportedImageFormats();

    // Add image formats.
    if(supportedFormats.contains("png")) { filterStrings << tr("PNG image file (*.png)"); _formatList << "png"; }
    if(supportedFormats.contains("jpg")) { filterStrings << tr("JPEG image file (*.jpg *.jpeg)"); _formatList << "jpg"; }
    if(supportedFormats.contains("eps")) { filterStrings << tr("EPS Encapsulated PostScript (*.eps)"); _formatList << "eps"; }
    if(supportedFormats.contains("tiff")) { filterStrings << tr("TIFF Tagged image file (*.tif *.tiff)"); _formatList << "tiff"; }
    if(supportedFormats.contains("tga")) { filterStrings << tr("TGA Targa image file (*.tga)"); _formatList << "tga"; }

    if(includeVideoFormats) {
        // Add video formats.
        for(const auto& videoFormat : VideoEncoder::supportedFormats()) {
            QString filterString = videoFormat.candidate->longName + " (";
            QStringList extensions;
            for(const QString& ext : videoFormat.candidate->extensions)
                extensions << QStringLiteral("*.%1").arg(ext);
            filterString += extensions.join(" ") + ")";
            filterStrings << filterString;
            _formatList << QByteArray::fromRawData(videoFormat.candidate->name.data(), videoFormat.candidate->name.size());
        }
    }

    if(filterStrings.isEmpty())
        throw Exception(tr("There are no image format plugins available."));

    setNameFilters(filterStrings);
    setAcceptMode(QFileDialog::AcceptSave);
    setLabelText(QFileDialog::FileType, tr("Save as type"));
    if(_imageInfo.filename().isEmpty() == false)
        selectFile(_imageInfo.filename());

    int index = _formatList.indexOf(_imageInfo.format().toLower());
    if(index >= 0) selectNameFilter(filterStrings[index]);

    // Select the default suffix.
    onFilterSelected(selectedNameFilter());
}

/******************************************************************************
* This is called when the user has selected a file format.
******************************************************************************/
void SaveImageFileDialog::onFilterSelected(const QString& filter)
{
    int index = nameFilters().indexOf(filter);
    if(index >= 0 && index < _formatList.size())
        setDefaultSuffix(_formatList[index]);
}

/******************************************************************************
* This is called when the user has pressed the OK button of the dialog.
******************************************************************************/
void SaveImageFileDialog::onFileSelected(const QString& file)
{
    _imageInfo.setFilename(file);
    int index = nameFilters().indexOf(selectedNameFilter());
    if(index >= 0 && index < _formatList.size())
        _imageInfo.setFormat(_formatList[index]);
}

}   // End of namespace
