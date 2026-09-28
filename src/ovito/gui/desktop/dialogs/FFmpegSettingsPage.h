// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/dialogs/ApplicationSettingsDialog.h>

namespace Ovito {

/**
 * Page of the application settings dialog, which hosts rendering options.
 */
class OVITO_GUI_EXPORT FFmpegSettingsPage : public ApplicationSettingsDialogPage
{
    Q_OBJECT
    OVITO_CLASS(FFmpegSettingsPage)

public:
    /// Default constructor.
    explicit FFmpegSettingsPage() = default;

    /// \brief Creates the widget.
    virtual void insertSettingsDialogPage(QTabWidget* tabWidget) override;

    /// \brief Lets the settings page to save all values entered by the user.
    virtual void saveValues(QTabWidget* tabWidget) override;

    /// \brief Returns an integer value that is used to sort the dialog pages in ascending order.
    virtual int pageSortingKey() const override { return 20; }

    /// \brief Help topic to open when the user presses the help button.
    virtual QString helpTopicId() const override {
        return QStringLiteral("manual:application_settings.video_encoding");
    }

Q_SIGNALS:
    /// Emitted when the path to the ffmpeg executable has been validated.
    void ffmpegPathValidated(bool isValid);

private:
    /// Validates the user provided FFmpeg executable.
    /// Falls back to the built-in FFmpeg library if the path or executable is invalid.
    void validateFfmpegPath();

    /// Refresh the list of available codecs.
    void refreshCodecCombobox();

private:
    StatusWidget* _statusLabel;
    QLineEdit* _ffmpegPath;
    QComboBox* _ffmpegCodec;
    QComboBox* _ffmpegQualityBox;
    QByteArray _ffmpegCodecName;
    bool _externalFFmpeg;
};

}  // namespace Ovito
