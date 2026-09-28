// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>

namespace Ovito {

class DisjointSet
{
public:
    DisjointSet(size_t n)
    {
        parents.resize(n);
        sizes.resize(n);
        clear();
    }

    void clear()
    {
        std::iota(parents.begin(), parents.end(), (size_t)0);
        std::fill(sizes.begin(), sizes.end(), 1);
    }

    // "Find" part of Union-Find.
    size_t find(size_t index)
    {
        // Find root and make root as parent of i (path compression)
        size_t x = parents[index];
        while(x != parents[x]) {
            parents[x] = parents[parents[x]];
            x = parents[x];
        }

        parents[index] = x;
        return x;
    }

    // "Union" part of Union-Find.
    size_t merge(size_t index1, size_t index2)
    {
        size_t parentA = find(index1);
        size_t parentB = find(index2);
        if(parentA == parentB) return parentA;

        // Attach smaller tree under root of larger tree
        if(sizes[parentA] < sizes[parentB]) {
            parents[parentA] = parentB;
            sizes[parentB] += sizes[parentA];
            return parentB;
        }
        else {
            parents[parentB] = parentA;
            sizes[parentA] += sizes[parentB];
            return parentA;
        }
    }

    size_t nodesize(size_t index) const { return sizes[index]; }

private:
    std::vector<size_t> parents;
    std::vector<size_t> sizes;
};

}  // namespace Ovito
