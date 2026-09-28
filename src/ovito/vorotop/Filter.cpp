// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/vorotop/VoroTopPlugin.h>
#include <ovito/core/utilities/io/CompressedTextReader.h>
#include <ovito/core/utilities/io/NumberParsing.h>
#include "Filter.h"

namespace Ovito::VoroTop {

/******************************************************************************
* Loads the filter definition from the given input stream.
******************************************************************************/
void Filter::load(CompressedTextReader& stream, bool readHeaderOnly, TaskProgress& progress)
{
    // Parse comment lines starting with '#':
    _filterDescription.clear();
    const char* line;
    while(!stream.eof()) {
        line = stream.readLineTrimLeft();
        if(line[0] != '#') break;
        _filterDescription += QString::fromUtf8(line + 1).trimmed() + QChar('\n');
        this_task::throwIfCanceled();
    }

    // Create the default "Other" structure type.
    _structureTypeLabels.clear();
    _structureTypeLabels.push_back("Other");
    _structureTypeDescriptions.clear();
    _structureTypeDescriptions.push_back(QString());

    // Parse list of structure types (lines starting with '*').
    while(!stream.eof()) {
        if(line[0] != '*') break;
        int typeId;
        int stringLength;
        if(sscanf(line, "* %i%n", &typeId, &stringLength) != 1)
            throw Exception(QString("Invalid structure type definition in line %1 of VoroTop filter definition file").arg(stream.lineNumber()));
        if(typeId != _structureTypeLabels.size())
            throw Exception(QString("Invalid structure type definition in line %1 of VoroTop filter definition file: Type IDs must start at 1 and form a consecutive sequence.").arg(stream.lineNumber()));
        QStringList columns = QString::fromUtf8(line + stringLength).trimmed().split(QChar('\t'), Qt::SkipEmptyParts);
        if(columns.empty())
            throw Exception(QString("Invalid structure type definition in line %1 of VoroTop filter definition file: Type label is missing.").arg(stream.lineNumber()));
        _structureTypeLabels.push_back(columns[0]);
        _structureTypeDescriptions.push_back(columns.size() >= 2 ? columns[1] : QString());

        line = stream.readLineTrimLeft();
        this_task::throwIfCanceled();
    }
    if(_structureTypeLabels.size() <= 1)
        throw Exception(QString("Invalid filter definition file"));

    if(readHeaderOnly)
        return;

    progress.setMaximum(stream.underlyingSize());

    // Parse Weinberg vector list.
    for(;;) {

        // Parse structure type the current Weinberg code will be mapped to.
        int typeId;
        int stringLength;
        if(sscanf(line, "%i (%n", &typeId, &stringLength) != 1 || typeId <= 0 || typeId >= _structureTypeLabels.size())
            throw Exception(QString("Invalid Weinberg vector in line %1 of VoroTop filter definition file").arg(stream.lineNumber()));
        line += stringLength;

        // Parse Weinberg code sequence.
        WeinbergVector wvector;
        for(;;) {
            const char* s = line;
            while(*s != '\0' && *s != ')' && *s != ',')
                ++s;
            int32_t label;
            if(*s == '\0' || !parseInt32(line, s, label))
                throw Exception(QString("Invalid Weinberg vector in line %1 of VoroTop filter definition file").arg(stream.lineNumber()));
            wvector.push_back(label);
            if(label > maximumVertices) maximumVertices = label;

            if(*s == ')') break;
            line = s + 1;
        }
        int edges = (wvector.size()-1)/2;
        if(edges > maximumEdges) maximumEdges = edges;

        _entries.emplace(std::move(wvector), typeId);
        if(stream.eof()) break;

        line = stream.readNonEmptyLine();

        // Update progress indicator.
        progress.setValueIntermittent(stream.underlyingByteOffset());
    }
}

}   // End of namespace
