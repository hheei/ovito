// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/rendering/FrameBuffer.h>
#include "HistoryFileDialog.h"

namespace Ovito {

/**
 * \brief This file chooser dialog lets the user select an image file from disk.
 */
class OVITO_GUI_EXPORT LoadImageFileDialog : public HistoryFileDialog
{
    Q_OBJECT

public:

    /// \brief Constructs the dialog window.
    LoadImageFileDialog(MainWindowUI& ui, QWidget* parent = nullptr, const QString& caption = QString(), const ImageInfo& imageInfo = ImageInfo());

    /// \brief Returns the file info after the dialog has been closed with "OK".
    const ImageInfo& imageInfo() const { return _imageInfo; }

private Q_SLOTS:

    /// This is called when the user has pressed the OK button of the dialog box.
    void onFileSelected(const QString& file);

private:

    ImageInfo _imageInfo;
};

}   // End of namespace


