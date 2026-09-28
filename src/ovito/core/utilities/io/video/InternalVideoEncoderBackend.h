// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/utilities/io/video/VideoEncoder.h>

extern "C" {
struct AVFormatContext;
struct AVOutputFormat;
struct AVCodec;
struct AVStream;
struct AVCodecContext;
struct AVFrame;
struct AVFilterInOut;
struct AVFilterGraph;
struct AVFilterContext;
struct SwsContext;
};

namespace Ovito {

/**
 * \brief Wrapper class for the ffmpeg video encoding library.
 */
class OVITO_CORE_EXPORT InternalVideoEncoderBackend : public VideoEncoder::VideoEncoderBackend
{
public:
    /// Constructor.
    InternalVideoEncoderBackend();

    /// Destructor.
    virtual ~InternalVideoEncoderBackend();

    /// Opens a video file for writing.
    virtual void openFile(const QString& filename, int width, int height, float framesPerSecond) override;

    /// Writes a single frame into the video file.
    virtual void writeFrame(const QImage& image) override;

    /// This closes the written video file.
    virtual void closeFile() override;

    /// Returns the list of supported output formats.
    static QList<VideoEncoder::Format> supportedFormats();

    /// Returns the list of supported codecs.
    static QList<const VideoEncoder::CandidateCodec*> supportedCodecs();

private:
    /// Initializes libavcodec, and register all codecs and formats.
    static void initCodecs();

    /// Returns the error string for the given error code.
    static QString errorMessage(int errorCode);

    std::shared_ptr<AVFormatContext> _formatContext;
    std::unique_ptr<quint8[]> _pictureBuf;
    std::vector<quint8> _outputBuf;
    std::shared_ptr<AVFrame> _frame;
    AVStream* _videoStream = nullptr;
    const AVCodec* _codec = nullptr;
    std::shared_ptr<AVCodecContext> _codecContext;
    SwsContext* _imgConvertCtx = nullptr;
    std::shared_ptr<AVFilterGraph> _filterGraph;
    AVFilterContext* _bufferSourceCtx = nullptr;
    AVFilterContext* _bufferSinkCtx = nullptr;
    bool _isOpen = false;
    int _frameDuplication = 1;
    int _numFrames = 0;

    /// The list of supported video formats.
    inline static QList<VideoEncoder::Format> _supportedFormats;

    /// The list of supported video codecs.
    inline static QList<const VideoEncoder::CandidateCodec*> _supportedCodecs;
};

}  // namespace Ovito
