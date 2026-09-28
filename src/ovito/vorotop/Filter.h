// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/vorotop/VoroTopPlugin.h>

namespace Ovito::VoroTop {

/**
 * A filter is a specification of topological types, recorded with Weinberg codes.
 */
class OVITO_VOROTOP_EXPORT Filter
{
public:

    /// Data type holding a single Weinberg vector.
    using WeinbergVector = std::vector<int>;

    /// Maximum edges and vertices of type in Filter
    int maximumEdges;
    int maximumVertices;

public:

    /// Loads the filter definition from the given input stream.
    void load(CompressedTextReader& stream, bool readHeaderOnly, TaskProgress& progress);

    /// Returns the comment text loaded from the filter definition file.
    const QString& filterDescription() const { return _filterDescription; }

    /// Returns the number of Weinberg vectors of this filter.
    size_t size() const { return _entries.size(); }

    /// Looks up the structure type associated with the given Weinberg vector.
    /// Return 0 if Weinberg vector is not in filter.
    int findType(const WeinbergVector& wvector) const {
        if(auto item = _entries.find(wvector); item != _entries.end())
            return item->second;
        else
            return 0;
    }

    /// Returns the number of structure types defined in this filter (including the "other" type).
    int structureTypeCount() const { return _structureTypeLabels.size(); }

    /// Returns the name of the structure type with the given index.
    const QString& structureTypeLabel(int index) const { return _structureTypeLabels[index]; }

    /// Returns the description string of the structure type with the given index.
    const QString& structureTypeDescription(int index) const { return _structureTypeDescriptions[index]; }

private:

    /// Names of the structures types this filter maps to, e.g. "FCC", "FCC-HCP", "BCC", etc.
    QStringList _structureTypeLabels;

    /// Descriptions strings of the structures types.
    QStringList _structureTypeDescriptions;

    /// Mapping from Weinberg vectors to structure types.
    std::map<WeinbergVector, int> _entries;

    /// Comment text loaded from the filter definition file.
    QString _filterDescription;
};

}   // End of namespace
