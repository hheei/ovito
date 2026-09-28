// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include "Exception.h"

namespace Ovito {

Exception::Exception()
{
    _messages.push_back("An exception has occurred.");
}

Exception::Exception(const QString& message)
{
    _messages.push_back(message);
}

Exception::Exception(QStringList errorMessages) : _messages(std::move(errorMessages))
{
}

Exception& Exception::appendDetailMessage(const QString& message)
{
    _messages.push_back(message);
    return *this;
}

Exception& Exception::prependGeneralMessage(const QString& message)
{
    _messages.push_front(message);
    return *this;
}

Exception& Exception::prependToMessage(const QString& text)
{
    if(!_messages.empty())
        _messages.front().prepend(text);
    else
        _messages.push_back(text);
    return *this;
}

void Exception::logError() const
{
    if(!traceback().isEmpty())
        qCritical().noquote() << traceback();
    for(const QString& msg : _messages) {
        qCritical().noquote() << msg;
    }
}

}   // namespace Ovito
