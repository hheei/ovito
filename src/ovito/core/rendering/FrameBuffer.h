// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>

namespace Ovito {

/**
 * Stores information about the image in a FrameBuffer.
 */
class OVITO_CORE_EXPORT ImageInfo
{
public:

    /// Default constructor.
    ImageInfo() = default;

    /// Comparison operator.
    bool operator==(const ImageInfo& other) const {
        if(this->_imageWidth != other._imageWidth) return false;
        if(this->_imageHeight != other._imageHeight) return false;
        if(this->_filename != other._filename) return false;
        if(this->_format != other._format) return false;
        return true;
    }

    /// Returns the width of the image in pixels.
    [[nodiscard]] int imageWidth() const { return _imageWidth; }

    /// Sets the width of the image in pixels.
    void setImageWidth(int width) { OVITO_ASSERT(width >= 0); _imageWidth = width; }

    /// Returns the height of the image in pixels.
    [[nodiscard]] int imageHeight() const { return _imageHeight; }

    /// Sets the height of the image to be rendered in pixels.
    void setImageHeight(int height) { OVITO_ASSERT(height >= 0); _imageHeight = height; }

    /// Returns the filename of the image on disk.
    [[nodiscard]] const QString& filename() const { return _filename; }

    /// Sets the filename of the image on disk.
    void setFilename(const QString& filename) {
        _filename = filename;
        guessFormatFromFilename();
    }

    /// Returns the format of the image on disk.
    [[nodiscard]] const QByteArray& format() const { return _format; }

    /// Sets the format of the image on disk.
    void setFormat(const QByteArray& format) { _format = format; }

    /// Detects the file format based on the filename suffix.
    bool guessFormatFromFilename();

    /// Returns whether the selected file format is a video format.
    [[nodiscard]] bool isMovie() const;

private:

    /// The width of the image in pixels.
    int _imageWidth = 0;

    /// The height of the image in pixels.
    int _imageHeight = 0;

    /// The filename of the image on disk.
    QString _filename;

    /// The format of the image on disk.
    QByteArray _format;

    friend OVITO_CORE_EXPORT SaveStream& operator<<(SaveStream& stream, const ImageInfo& i);
    friend OVITO_CORE_EXPORT LoadStream& operator>>(LoadStream& stream, ImageInfo& i);
};

/// Writes an ImageInfo to an output stream.
/// \relates ImageInfo
OVITO_CORE_EXPORT SaveStream& operator<<(SaveStream& stream, const ImageInfo& i);

/// Reads an ImageInfo from an input stream.
/// \relates ImageInfo
OVITO_CORE_EXPORT LoadStream& operator>>(LoadStream& stream, ImageInfo& i);

/**
 * A buffer for storing rendered output images, which is backed by a QImage object.
 * The RenderTarget::renderFrame() method renders the scene into this framebuffer.
 *
 * The contents of the framebuffer can be modified by direct pixel access to the internal QImage
 * returned by the image() method. Alternatively, 2D graphics primitives can be rendered into
 * the framebuffer using the renderPrimitives() method.
 *
 * After modifying the contents of the framebuffer, the update() method must be called
 * to notify any listeners about the changed region of the framebuffer.
 *
 * During rendering, the contents of the framebuffer can be displayed by a FrameBufferWidget
 * or FrameBufferWindow from the GUI module.
 *
 * After rendering, the contents of the framebuffer can be saved to an image file using
 * the Qt QImage::save() method, or passed to the VideoEncoder::writeFrame() method for
 * inclusion in a video file.
 */
class OVITO_CORE_EXPORT FrameBuffer : public QObject
{
    Q_OBJECT

public:

    /// Constructor.
    explicit FrameBuffer(QObject* parent = nullptr) : QObject(parent) {}

    /// Constructor.
    explicit FrameBuffer(int width, int height, QObject* parent = nullptr);

    /// Constructor.
    explicit FrameBuffer(const QSize& size, QObject* parent = nullptr) : FrameBuffer(size.width(), size.height(), parent) {}

    /// Returns the internal QImage that is used to store the pixel data.
    QImage& image() {
        commitChanges();
        return _image;
    }

    /// Returns the internal QImage that is used to store the pixel data.
    const QImage& image() const {
        const_cast<FrameBuffer*>(this)->commitChanges();
        return _image;
    }

    /// Returns a shallow snapshot of the current image suitable for safe display
    /// from a thread other than the one writing the framebuffer. Bumping the QImage
    /// refcount under the mutex guarantees the pixel data stays alive even if the
    /// writer concurrently replaces _image afterwards.
    QImage displayImage() const {
        QMutexLocker locker(&_imageMutex);
        return _image;
    }

    /// Mutex serializing access to the QImage d-pointer between the rendering thread
    /// and the GUI thread. Code that modifies _image while the framebuffer is shared
    /// across threads must hold this mutex for the modification.
    QMutex& imageMutex() const { return _imageMutex; }

    /// Returns the width of the image.
    int width() const { return _image.width(); }

    /// Returns the height of the image.
    int height() const { return _image.height(); }

    /// Returns the size of the image.
    QSize size() const { return _image.size(); }

    /// Sets the size of the frame buffer image.
    void setSize(const QSize& newSize) {
        commitChanges();
        if(newSize == size())
            return;
        _info.setImageWidth(newSize.width());
        _info.setImageHeight(newSize.height());
        _image = _image.copy(0, 0, newSize.width(), newSize.height());
        _viewportRect = QRect(QPoint(0,0), newSize);
        Q_EMIT bufferResized(newSize);
    }

    /// Returns the descriptor of the image.
    const ImageInfo& info() const { return _info; }

	/// Returns the target area currently being rendered into.
	const QRect& viewportRect() const { return _viewportRect; }

    /// Sets the target area currently being rendered into.
    void setViewportRect(const QRect& rect) { _viewportRect = rect; }

    /// Clears the framebuffer with a uniform color.
    void clear(const ColorA& color = ColorA(0,0,0,0), bool delayed = false);

    /// This method must be called each time the contents of the frame buffer have been modified.
    /// Fires the contentChanged() signal.
    void update(const QRect& changedRegion) {
        commitChanges();
        Q_EMIT contentChanged(changedRegion | _delayedUpdateRect);
        _delayedUpdateRect = {};
    }

    /// Removes unnecessary pixels along the outer edges of the image.
    bool autoCrop();

    /// Applies a delayed clear buffer operation.
    void commitChanges();

    /// Discards a delayed clear buffer operation.
    void discardChanges() {
        _delayedClearRect = {};
    }

Q_SIGNALS:

    /// This signal is emitted by the framebuffer when a part of its content has changed.
    void contentChanged(const QRect& changedRegion);

    /// This signal is emitted by the framebuffer when the backing store has changed its size.
    void bufferResized(QSize size);

private:

    /// The internal pixel data storage.
    QImage _image;

    /// The descriptor of the image.
    ImageInfo _info;

    /// The target area currently being rendered into.
	QRect _viewportRect;

    /// Saved rect to be cleared at some later time.
    QRect _delayedClearRect;

    /// Uniform color for delayed buffer clearing.
    ColorA _delayedClearColor;

    /// Saved rect to be updated at some later time.
    QRect _delayedUpdateRect;

    /// Protects access to _image when shared across threads (see imageMutex()).
    mutable QMutex _imageMutex;
};

}   // End of namespace
