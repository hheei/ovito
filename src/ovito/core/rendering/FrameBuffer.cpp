// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/rendering/FrameBuffer.h>
#include <ovito/core/utilities/io/video/VideoEncoder.h>

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
FrameBuffer::FrameBuffer(int width, int height, QObject* parent) : QObject(parent), _image(width, height, QImage::Format_ARGB32_Premultiplied)
{
    _info.setImageWidth(width);
    _info.setImageHeight(height);
    _viewportRect = QRect(QPoint(0,0), QSize(width, height));
    clear();
}

/******************************************************************************
* Detects the file format based on the filename suffix.
******************************************************************************/
bool ImageInfo::guessFormatFromFilename()
{
    if(filename().endsWith(QStringLiteral(".png"), Qt::CaseInsensitive)) {
        setFormat("png");
        return true;
    }
    else if(filename().endsWith(QStringLiteral(".jpg"), Qt::CaseInsensitive) || filename().endsWith(QStringLiteral(".jpeg"), Qt::CaseInsensitive)) {
        setFormat("jpg");
        return true;
    }
    for(const auto& videoFormat : VideoEncoder::supportedFormats()) {
        for(const QString& extension : videoFormat.candidate->extensions) {
            if(filename().endsWith(QStringLiteral(".") + extension, Qt::CaseInsensitive)) {
                setFormat(videoFormat.candidate->name);
                return true;
            }
        }
    }

    return false;
}

/******************************************************************************
* Returns whether the selected file format is a video format.
******************************************************************************/
bool ImageInfo::isMovie() const
{
    return std::ranges::any_of(VideoEncoder::supportedFormats(),
                               [this](const auto& videoFormat) { return format() == videoFormat.candidate->name; });
}

/******************************************************************************
* Writes an ImageInfo to an output stream.
******************************************************************************/
SaveStream& operator<<(SaveStream& stream, const ImageInfo& i)
{
    stream.beginChunk(0x01);
    stream << i._imageWidth;
    stream << i._imageHeight;
    stream << i._filename;
    stream << i._format;
    stream.endChunk();
    return stream;
}

/******************************************************************************
* Reads an ImageInfo from an input stream.
******************************************************************************/
LoadStream& operator>>(LoadStream& stream, ImageInfo& i)
{
    stream.expectChunk(0x01);
    stream >> i._imageWidth;
    stream >> i._imageHeight;
    stream >> i._filename;
    stream >> i._format;
    stream.closeChunk();
    return stream;
}

/******************************************************************************
* Clears the framebuffer with a uniform color.
******************************************************************************/
void FrameBuffer::clear(const ColorA& color, bool delayed)
{
    commitChanges();
    if(!delayed) {
        QRect bufferRect = _image.rect();
        if(viewportRect().isNull() || viewportRect() == bufferRect) {
            _image.fill(color);
            update(bufferRect);
        }
        else {
            QPainter painter(&_image);
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.fillRect(viewportRect(), color);
            update(viewportRect());
        }
    }
    else {
        _delayedClearRect = viewportRect().isNull() ? _image.rect() : viewportRect();
        _delayedClearColor = color;
    }
}

/******************************************************************************
* Performs a delayed clear buffer operation.
******************************************************************************/
void FrameBuffer::commitChanges()
{
    if(!_delayedClearRect.isNull()) {
        OVITO_ASSERT(!_image.isNull());
        QRect clearRect = std::exchange(_delayedClearRect, QRect()) & _image.rect();
        if(clearRect == _image.rect()) {
            _image.fill(_delayedClearColor);
        }
        else {
            OVITO_ASSERT(_image.format() == QImage::Format_ARGB32 || _image.format() == QImage::Format_ARGB32_Premultiplied);
            QRgb clearColor = _delayedClearColor.qrgb();
            if(_image.format() == QImage::Format_ARGB32_Premultiplied)
                clearColor = qPremultiply(clearColor);
            for(int y = clearRect.top(); y <= clearRect.bottom(); y++) {
                QRgb* dst = reinterpret_cast<QRgb*>(_image.scanLine(y)) + clearRect.left();
                std::fill(dst, dst + clearRect.width(), clearColor);
            }
        }
        _delayedUpdateRect |= clearRect;
    }
}

/******************************************************************************
* Removes unnecessary pixels along the outer edges of the image.
******************************************************************************/
bool FrameBuffer::autoCrop()
{
    const QImage& image = this->image();
    OVITO_ASSERT(image.depth() == 8 * sizeof(QRgb)); // The auto-cropping logic relies on the image being in a 32-bit ARGB-like format that is compatible with the QRgb struct.

    if(image.width() <= 0 || image.height() <= 0)
        return false;

    const QRgb* pixelData = reinterpret_cast<const QRgb*>(image.constBits());

    auto pixelColor = [&](int x, int y) -> QRgb {
        return pixelData[y * image.width() + x];
    };

    auto determineCropRect = [&](QRgb backgroundColor) -> QRect {
        int x1 = 0, y1 = 0;
        int x2 = image.width() - 1, y2 = image.height() - 1;
        bool significant;
        for(;; x1++) {
            significant = false;
            for(int y = y1; y <= y2; y++) {
                if(pixelColor(x1, y) != backgroundColor) {
                    significant = true;
                    break;
                }
            }
            if(significant || x1 > x2)
                break;
        }
        for(; x2 >= x1; x2--) {
            significant = false;
            for(int y = y1; y <= y2; y++) {
                if(pixelColor(x2, y) != backgroundColor) {
                    significant = true;
                    break;
                }
            }
            if(significant || x1 > x2)
                break;
        }
        for(;; y1++) {
            significant = false;
            const QRgb* s = pixelData + y1 * image.width();
            for(int x = x1; x <= x2; x++) {
                if(s[x] != backgroundColor) {
                    significant = true;
                    break;
                }
            }
            if(significant || y1 >= y2)
                break;
        }
        for(; y2 >= y1; y2--) {
            significant = false;
            const QRgb* s = pixelData + y2 * image.width();
            for(int x = x1; x <= x2; x++) {
                if(s[x] != backgroundColor) {
                    significant = true;
                    break;
                }
            }
            if(significant || y1 > y2)
                break;
        }
        return QRect(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
    };

    // Use the pixel colors in the four corners of the images as candidate background colors.
    // Compute the crop rect for each candidate color and use the one that leads
    // to the smallest crop rect.
    QRect cropRect = determineCropRect(pixelColor(0,0));
    QRect r;
    r = determineCropRect(pixelColor(image.width()-1, 0));
    if(r.width()*r.height() < cropRect.width()*cropRect.height())
        cropRect = r;
    r = determineCropRect(pixelColor(image.width()-1, image.height()-1));
    if(r.width()*r.height() < cropRect.width()*cropRect.height())
        cropRect = r;
    r = determineCropRect(pixelColor(0, image.height()-1));
    if(r.width()*r.height() < cropRect.width()*cropRect.height())
        cropRect = r;

    if(cropRect != image.rect() && cropRect.width() > 0 && cropRect.height() > 0) {
        _image = _image.copy(cropRect);
        Q_EMIT bufferResized(_image.size());
        return true;
    }

    return false;
}

}   // End of namespace
