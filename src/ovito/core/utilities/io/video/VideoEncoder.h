// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>

#ifdef OVITO_VIDEO_OUTPUT_SUPPORT
extern "C" {
    struct AVOutputFormat;
};
#endif

namespace Ovito {

/**
 * \brief Wrapper class for the ffmpeg video encoding library.
 */
class OVITO_CORE_EXPORT VideoEncoder : public QObject
{
    Q_OBJECT

public:
    /// List of supported video formats (containers, i.e. ffmpeg -demuxers)
    struct CandidateFormat {
        const QByteArray name;
        const QString longName;
        const QStringList extensions;
    };

    /// List of supported video codecs (encoders)
    struct CandidateCodec {
        const QByteArray name;
        const QByteArray longName;
        const QByteArray libName;
    };

    /**
     * Describes an output format supported by the video encoding engine.
     */
    struct Format {
        const CandidateFormat* candidate;
#ifdef OVITO_VIDEO_OUTPUT_SUPPORT
        const AVOutputFormat* avformat = nullptr;
#endif
    };

    // Available video encoding quality presets.
    enum class Quality : uint8_t
    {
        Low,
        Medium,
        High
    };
    Q_ENUM(Quality)

    /**
     * \brief Abstract base class for the ffmpeg video encoding backend.
     */
    class VideoEncoderBackend
    {
    public:
        virtual ~VideoEncoderBackend() = default;

        /// Opens a video file for writing.
        virtual void openFile(const QString& filename, int width, int height, float framesPerSecond) = 0;

        /// Writes a single frame into the video file.
        virtual void writeFrame(const QImage& image) = 0;

        /// This closes the written video file.
        virtual void closeFile() = 0;

    protected:
        /// Compares a format name to the list of supported formats and returns the corresponding CandidateFormat.
        static const CandidateFormat* getCandidateFormat(QByteArrayView name);
        /// Compares a codec name to the list of supported codecs and returns the corresponding CandidateCodec.
        static const CandidateCodec* getCandidateCodec(QByteArrayView name);
    };

    enum class Backend : uint8_t
    {
        EXTERNAL,
        INTERNAL
    };
    /// Constructor.
    VideoEncoder(QObject* parent = nullptr);

    /// Opens a video file for writing.
    void openFile(const QString& filename, int width, int height, float framesPerSecond);

    /// Writes a single frame into the video file.
    void writeFrame(const QImage& image);

    /// This closes the written video file.
    void closeFile();

    /// Returns the list of supported output formats from the backend.
    static QList<Format> supportedFormats(std::optional<Backend> backend = std::nullopt, std::optional<QString> path = std::nullopt);

    /// Returns the list of supported output codecs from the backend.
    static QList<const VideoEncoder::CandidateCodec*> supportedCodecs(std::optional<Backend> backend = std::nullopt,
                                                                      std::optional<QString> path = std::nullopt);

    /// Clears the list of supported codecs.
    static void clearCodecs();

    /// Keys used in QSettings to configure the FFmpeg backend.
    constexpr static const char* FFMPEG_PATH_SETTING = "renderer/ffmpeg_path";
    constexpr static const char* FFMPEG_CODEC_SETTING = "renderer/ffmpeg_codec";
    constexpr static const char* FFMPEG_USE_EXT_SETTING = "renderer/ffmpeg_use_external";
    constexpr static const char* FFMPEG_QUALITY_SETTING = "renderer/ffmpeg_quality";

    /// Environment variable used to configure the FFmpeg backend.
    constexpr static const char* OVITO_FFMPEG_CODEC = "OVITO_FFMPEG_CODEC";
    constexpr static const char* OVITO_FFMPEG_EXE = "OVITO_FFMPEG_EXE";
    constexpr static const char* OVITO_FFMPEG_QUALITY = "OVITO_FFMPEG_QUALITY";
    constexpr static const char* OVITO_FFMPEG_USE_EXT = "OVITO_FFMPEG_USE_EXT";

private:
    static Backend getBackend();

    std::unique_ptr<VideoEncoderBackend> _backend;
};

}   // End of namespace
