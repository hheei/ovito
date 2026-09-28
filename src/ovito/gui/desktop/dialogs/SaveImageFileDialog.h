// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/rendering/FrameBuffer.h>
#include "HistoryFileDialog.h"

namespace Ovito {

/**
 * \brief This file chooser dialog lets the user select an image file for output.
 */
class OVITO_GUI_EXPORT SaveImageFileDialog : public HistoryFileDialog
{
    Q_OBJECT

public:

    /// \brief Constructs the dialog window.
    SaveImageFileDialog(MainWindowUI& ui, QWidget* parent = nullptr, const QString& caption = QString(), bool includeVideoFormats = false, const ImageInfo& imageInfo = ImageInfo());

    /// \brief Returns the file info after the dialog has been closed with "OK".
    const ImageInfo& imageInfo() const { return _imageInfo; }

private Q_SLOTS:

    /// This is called when the user has pressed the OK button of the dialog box.
    void onFileSelected(const QString& file);

    /// This is called when the user has selected a file format.
    void onFilterSelected(const QString& filter);

private:

    QList<QByteArray> _formatList;
    ImageInfo _imageInfo;
};

}   // End of namespace


