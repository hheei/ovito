// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/utilities/Exception.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/oo/OvitoClass.h>
#include "SaveStream.h"

namespace Ovito {

using namespace std;

/******************************************************************************
* Opens the stream for writing.
******************************************************************************/
SaveStream::SaveStream(QDataStream& destination) : _os(destination)
{
    OVITO_ASSERT(this_task::get());

    OVITO_ASSERT_MSG(!_os.device()->isSequential(), "SaveStream constructor", "SaveStream class requires a seekable output stream.");
    if(_os.device()->isSequential())
        throw Exception("SaveStream class requires a seekable output stream.");

    _isOpen = true;

    // Write file header.

    // This is used to recognize the application file format.
    *this << (quint32)0x0FACC5AB;   // The first magic file code.
    *this << (quint32)0x0AFCCA5A;   // The second magic file code.

    // This is the version of the stream file format.
    *this << (quint32)OVITO_FILE_FORMAT_VERSION;
    // Note: This QDataStream version is deliberately pinned. It must not be changed without also
    // bumping OVITO_FILE_FORMAT_VERSION, because LoadStream picks the matching version based on it.
    _os.setVersion(QDataStream::Qt_6_10);
    _os.setFloatingPointPrecision(sizeof(FloatType) == 4 ? QDataStream::SinglePrecision : QDataStream::DoublePrecision);

    // Store the floating-point precision used throughout the file.
    *this << (quint32)sizeof(FloatType);

    // Write application name.
    *this << Application::applicationName();

    // Write application version.
    *this << (quint32)Application::applicationVersionMajor();
    *this << (quint32)Application::applicationVersionMinor();
    *this << (quint32)Application::applicationVersionRevision();
    *this << Application::applicationVersionString();
}

/******************************************************************************
* Closes the stream.
******************************************************************************/
void SaveStream::close()
{
    _isOpen = false;
}

/******************************************************************************
* Writes an array of bytes to the ouput stream.
******************************************************************************/
void SaveStream::write(const void* buffer, size_t numBytes)
{
    if(_os.device()->write(reinterpret_cast<const char*>(buffer), numBytes) != numBytes)
        throw Exception(tr("Failed to write output file. %1").arg(_os.device()->errorString()));
}

/******************************************************************************
* Start a new chunk with the given id and put it on the stack.
******************************************************************************/
void SaveStream::beginChunk(quint32 chunkId)
{
    *this << chunkId;
    *this << (quint32)0;    // This will be backpatched by endChunk().

    _chunks.push(filePosition());
}

/******************************************************************************
* Closes the chunk on the top of the chunk stack.
******************************************************************************/
void SaveStream::endChunk()
{
    OVITO_ASSERT(!_chunks.empty());
    qint64 chunkStart = _chunks.top();
    _chunks.pop();

    qint64 chunkSize = filePosition() - chunkStart;
    OVITO_ASSERT(chunkSize >= 0 && chunkSize <= 0xFFFFFFFF);

    // Seek to chunk size field.
    if(!_os.device()->seek(chunkStart - sizeof(quint32)) )
        throw Exception(tr("Failed to close chunk in output file."));

    // Patch chunk size field.
    *this << (quint32)chunkSize;

    // Jump back to end of file.
    if(!_os.device()->seek(_os.device()->size()))
        throw Exception(tr("Failed to close chunk in output file."));

    OVITO_ASSERT(filePosition() == chunkStart + chunkSize);
}

/******************************************************************************
* Writes a pointer to the stream.
* This method generates a unique ID for the given pointer that
* is written to the stream instead of the pointer itself.
******************************************************************************/
void SaveStream::writePointer(void* pointer)
{
    if(pointer == nullptr) *this << (quint64)0;
    else {
        quint64& id = _pointerMap[pointer];
        if(id == 0) id = (quint64)_pointerMap.size();
        *this << id;
    }
}

/******************************************************************************
* Returns the ID for a pointer that was used to write the pointer to the stream.
* Return 0 if the given pointer hasn't been written to the stream yet.
******************************************************************************/
quint64 SaveStream::pointerID(void* pointer) const
{
    if(auto item = _pointerMap.find(pointer); item != _pointerMap.end())
        return item->second;
    else
        return 0;
}

/******************************************************************************
* Checks the status of the underlying output stream and throws an exception
* if an error has occurred.
******************************************************************************/
void SaveStream::checkErrorCondition()
{
    if(dataStream().status() != QDataStream::Ok) {
        throw Exception(tr("I/O error: Could not write to file."));
    }
}

/******************************************************************************
* Writes a font to a SaveStream.
******************************************************************************/
SaveStream& operator<<(SaveStream& stream, const QFont& font)
{
    // A QFont that was default-constructed while no QGuiApplication existed has an empty family list.
    // This happens in non-graphical program sessions, e.g. when a viewport layer is created by a Python
    // script that never renders an image. Qt's own serialization function unconditionally dereferences
    // the first element of the family list and crashes on such a font, so complete the list here.
    // A single empty family name is written, which is how Qt itself represents an unspecified font family.
    if(font.families().empty()) {
        QFont completedFont = font;
        completedFont.setFamily(QString());
        stream.writeValue(completedFont, std::false_type());
        return stream;
    }

    stream.writeValue(font, std::false_type());
    return stream;
}

/******************************************************************************
* Writes a URL to a SaveStream.
******************************************************************************/
SaveStream& operator<<(SaveStream& stream, QUrl url)
{
    // Let the optional callback function process the URL.
    if(stream.urlCallback())
        std::invoke(stream.urlCallback(), url);

    // Write original URL to stream.
    stream.writeValue(url, std::false_type());

    // Additionally write the path relative to current output file.
    // This only works if the file referenced by the URL is in the same directory as the stream destination file.
    QString relativePath;
    if(url.isLocalFile() && !url.isRelative()) {
        // Extract relative portion of path (only if both the scene file path and the external file path are absolute).
        if(QFileDevice* fileDevice = qobject_cast<QFileDevice*>(stream.dataStream().device())) {
            QFileInfo streamFile(fileDevice->fileName());
            if(streamFile.isAbsolute()) {
                relativePath = streamFile.dir().relativeFilePath(url.toLocalFile());
            }
        }
    }
    stream << relativePath;

    return stream;
}

}   // End of namespace
