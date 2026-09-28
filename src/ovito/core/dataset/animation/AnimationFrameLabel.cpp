// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include "AnimationFrameLabel.h"

namespace Ovito {

/******************************************************************************
* Tries to parse a frame label from its string representation.
******************************************************************************/
AnimationFrameLabel AnimationFrameLabel::parse(const QString& text)
{
    if(text.startsWith("Timestep ")) {
        bool ok;
        qlonglong timestep = text.mid(9).toLongLong(&ok);
        if(ok)
            return { LabelType::Timestep, static_cast<FloatType>(timestep) };
    }
    else if(text.startsWith("Index ")) {
        bool ok;
        qlonglong index = text.mid(6).toLongLong(&ok);
        if(ok)
            return { LabelType::Index, static_cast<FloatType>(index) };
    }
    else if(text.startsWith("Time ")) {
        bool ok;
        double time = text.mid(5).toDouble(&ok);
        if(ok)
            return { LabelType::Time, static_cast<FloatType>(time) };
    }
    else if(auto idx = text.indexOf(" (Frame "); idx > 0 && text.endsWith(QChar(')'))) {
        bool ok;
        qlonglong frame = text.mid(idx + 8).toLongLong(&ok);
        if(ok)
            return { LabelType::FilenameAndFrame, static_cast<FloatType>(frame), text.left(idx) };
    }
    else if(!text.isEmpty()) {
        // Note: We cannot disambiguate between filename and string label types.
        return { LabelType::String, FloatType(0), text };
    }
    return {};
}

}   // End of namespace
