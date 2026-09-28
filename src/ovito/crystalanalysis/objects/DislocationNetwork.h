// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/crystalanalysis/objects/DislocationNode.h>
#include <ovito/crystalanalysis/objects/MicrostructurePhase.h>
#include <ovito/stdobj/simcell/PeriodicDomainObject.h>
#include <ovito/stdobj/simcell/SimulationCell.h>

namespace Ovito {

/**
 * \brief Stores a collection of dislocation lines.
 */
class OVITO_CRYSTALANALYSIS_EXPORT DislocationNetwork : public PeriodicDomainObject
{
    OVITO_CLASS(DislocationNetwork)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Returns the list of dislocation lines.
    const std::vector<DislocationLine*>& lines() const { return _lines; }

    /// Returns the list of dislocation lines.
    std::vector<DislocationLine*>& lines() { return _lines; }

    /// Allocates a new dislocation line terminated by two nodes.
    DislocationLine* createLine(const ClusterVector& burgersVector);

    /// Removes a line from the global list of lines.
    void discardLine(DislocationLine* line);

    /// Smoothens and coarsens the dislocation lines.
    void smoothDislocationLines(int lineSmoothingLevel, FloatType linePointInterval, TaskProgress& progress);

    /// Adds a new crystal structures to the list.
    void addCrystalStructure(DataOORef<const MicrostructurePhase> structure) { _crystalStructures.push_back(this, PROPERTY_FIELD(crystalStructures), std::move(structure)); }

    /// Removes a crystal structure.
    void removeCrystalStructure(int index) { _crystalStructures.remove(this, PROPERTY_FIELD(crystalStructures), index); }

    /// Returns the crystal structure with the given ID, or null if no such structure exists.
    const MicrostructurePhase* structureById(int id) const {
        for(const MicrostructurePhase* stype : crystalStructures())
            if(stype->numericId() == id)
                return stype;
        return nullptr;
    }

    /// Returns the crystal structure with the given name, or null if no such structure exists.
    const MicrostructurePhase* structureByName(const QString& name) const {
        for(const MicrostructurePhase* stype : crystalStructures())
            if(stype->name() == name)
                return stype;
        return nullptr;
    }

    /// Aligns the directions of dislocation lines as much as possible.
    void alignDislocationLineDirections();

    /// Computes statistical information on the identified dislocation lines and outputs it to the pipeline as data tables and global attributes.
    FloatType generateDislocationStatistics(const OOWeakRef<const PipelineNode>& pipelineNode, PipelineFlowState& state, bool replaceDataObjects, const MicrostructurePhase* defaultStructure) const;

protected:

    /// Creates a copy of this object.
    virtual OORef<RefTarget> clone(bool deepCopy, CloneHelper& cloneHelper) const override;

    /// Is called when the value of a reference field of this object changes.
    virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

private:

    /// Smooths the sampling points of a dislocation line.
    static void smoothDislocationLine(int smoothingLevel, std::deque<Point3>& line, bool isLoop);

    /// Removes some of the sampling points from a dislocation line.
    static void coarsenDislocationLine(FloatType linePointInterval, const std::deque<Point3>& input, const std::deque<int>& coreSize, std::deque<Point3>& output, bool isClosedLoop, bool isInfiniteLine);

private:

    /// List of crystal structures.
    DECLARE_MODIFIABLE_VECTOR_REFERENCE_FIELD(DataOORef<const MicrostructurePhase>, crystalStructures, setCrystalStructures);

    /// The associated cluster graph.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(DataOORef<const ClusterGraph>, clusterGraph, setClusterGraph);

    // Used to allocate memory for DislocationNode instances.
    MemoryPool<DislocationNode> _nodePool;

    /// The list of dislocation lines.
    std::vector<DislocationLine*> _lines;

    /// To efficiently allocate memory for DislocationLine structures.
    MemoryPool<DislocationLine> _linePool;
};

}   // End of namespace
