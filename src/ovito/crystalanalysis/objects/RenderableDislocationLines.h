// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/crystalanalysis/objects/ClusterVector.h>

namespace Ovito {

/**
 * \brief A non-periodic version of the dislocation lines that is generated from a periodic DislocationNetworkObject.
 */
class OVITO_CRYSTALANALYSIS_EXPORT RenderableDislocationLines
{
public:

    /// A linear segment of a dislocation line.
    struct Segment
    {
        /// The two vertices of the segment.
        std::array<Point3, 2> verts;

        /// The Burgers vector of the segment.
        Cluster::VecType burgersVector;

        /// The crystallite the dislocation segment is embedded in.
        int region;

        /// Identifies the original dislocation line this segment is part of.
        int dislocationIndex;

        /// Equal comparison operator.
        bool operator==(const Segment& other) const { return verts == other.verts && dislocationIndex == other.dislocationIndex && burgersVector == other.burgersVector && region == other.region; }
    };

    /// Constructor.
    RenderableDislocationLines(std::vector<Segment> lineSegments, DataOORef<const ClusterGraph> clusterGraph) :
        _lineSegments(std::move(lineSegments)), _clusterGraph(std::move(clusterGraph)) {}

    /// Returns the list of clipped and wrapped line segments.
    const std::vector<Segment>& lineSegments() const { return _lineSegments; }

    /// Returns the cluster graph.
    const DataOORef<const ClusterGraph>& clusterGraph() const { return _clusterGraph; }

private:

    /// The list of clipped and wrapped line segments.
    std::vector<Segment> _lineSegments;

    /// The associated cluster graph.
    DataOORef<const ClusterGraph> _clusterGraph;
};

}   // End of namespace
