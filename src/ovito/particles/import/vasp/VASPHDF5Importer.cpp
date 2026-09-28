////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/grid/objects/VoxelGrid.h>
#include <ovito/grid/modifier/CreateIsosurfaceModifier.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <3rdparty/netcdf_integration/NetCDFIntegration.h>
#include "VASPHDF5Importer.h"

#include <hdf5.h>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(VASPHDF5Importer);
OVITO_CLASSINFO(VASPHDF5Importer, "DisplayName", "VASP HDF5");

namespace {

// The HDF5 library is not guaranteed to be thread-safe. This global mutex serializes
// all accesses to the library, similar to the NetCDFExclusiveAccess helper.
static QMutex hdf5Mutex;

/// RAII wrapper that automatically releases an HDF5 object identifier.
class H5Id
{
public:
    using Closer = herr_t (*)(hid_t);
    H5Id() = default;
    H5Id(hid_t id, Closer closer) noexcept : _id(id), _closer(closer) {}
    H5Id(H5Id&& other) noexcept : _id(other._id), _closer(other._closer) { other._id = -1; }
    H5Id(const H5Id&) = delete;
    H5Id& operator=(const H5Id&) = delete;
    ~H5Id() { if(_id >= 0 && _closer) _closer(_id); }
    operator hid_t() const { return _id; }
    bool valid() const { return _id >= 0; }
private:
    hid_t _id = -1;
    Closer _closer = nullptr;
};

/// Returns true if the dataset with the given path exists in the file.
bool datasetExists(hid_t file, const char* path)
{
    H5Id dset(H5Dopen2(file, path, H5P_DEFAULT), H5Dclose);
    return dset.valid();
}

/// Returns the dimensions of a dataset, or an empty list if the dataset does not exist.
std::vector<hsize_t> datasetDims(hid_t file, const char* path)
{
    H5Id dset(H5Dopen2(file, path, H5P_DEFAULT), H5Dclose);
    if(!dset.valid()) return {};
    H5Id space(H5Dget_space(dset), H5Sclose);
    int ndims = H5Sget_simple_extent_ndims(space);
    if(ndims < 0) return {};
    std::vector<hsize_t> dims(ndims);
    H5Sget_simple_extent_dims(space, dims.data(), nullptr);
    return dims;
}

/// Reads a complete numeric dataset, converting all values to the requested element type.
template<typename T>
std::vector<T> readNumericArray(hid_t file, const char* path, hid_t memType)
{
    H5Id dset(H5Dopen2(file, path, H5P_DEFAULT), H5Dclose);
    if(!dset.valid()) return {};
    H5Id space(H5Dget_space(dset), H5Sclose);
    hssize_t count = H5Sget_simple_extent_npoints(space);
    if(count < 0) return {};
    std::vector<T> buffer(count);
    if(count > 0 && H5Dread(dset, memType, H5S_ALL, H5S_ALL, H5P_DEFAULT, buffer.data()) < 0)
        return {};
    return buffer;
}

std::vector<double> readDoubleArray(hid_t file, const char* path) { return readNumericArray<double>(file, path, H5T_NATIVE_DOUBLE); }
std::vector<int> readIntArray(hid_t file, const char* path) { return readNumericArray<int>(file, path, H5T_NATIVE_INT); }

/// Reads a scalar integer value, falling back to a default if the dataset is missing.
int readIntScalar(hid_t file, const char* path, int defaultValue)
{
    std::vector<int> values = readIntArray(file, path);
    return values.empty() ? defaultValue : values.front();
}

/// Reads a scalar floating-point value, falling back to a default if the dataset is missing.
double readDoubleScalar(hid_t file, const char* path, double defaultValue)
{
    std::vector<double> values = readDoubleArray(file, path);
    return values.empty() ? defaultValue : values.front();
}

/// Reads a fixed-length string dataset into a list of (trimmed) strings.
QStringList readStringArray(hid_t file, const char* path)
{
    QStringList result;
    H5Id dset(H5Dopen2(file, path, H5P_DEFAULT), H5Dclose);
    if(!dset.valid()) return result;
    H5Id type(H5Dget_type(dset), H5Tclose);
    if(H5Tget_class(type) != H5T_STRING || H5Tis_variable_str(type) > 0)
        return result;
    size_t strSize = H5Tget_size(type);
    if(strSize == 0) return result;
    H5Id space(H5Dget_space(dset), H5Sclose);
    hssize_t count = H5Sget_simple_extent_npoints(space);
    if(count <= 0) return result;
    std::vector<char> buffer((size_t)count * strSize);
    if(H5Dread(dset, type, H5S_ALL, H5S_ALL, H5P_DEFAULT, buffer.data()) < 0)
        return result;
    for(hssize_t i = 0; i < count; i++)
        result.push_back(QString::fromLatin1(buffer.data() + (size_t)i * strSize, strSize).trimmed());
    return result;
}

} // anonymous namespace

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool VASPHDF5Importer::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    QString filename = QDir::toNativeSeparators(file.localFilePath());
    if(filename.isEmpty() || filename.startsWith(QChar(':')))
        return false;

    QMutexLocker locker(&hdf5Mutex);
    initializeHDF5Library();

    // Make sure this is an HDF5 file at all.
    QByteArray nameBytes = filename.toUtf8();
    if(H5Fis_hdf5(nameBytes.constData()) <= 0)
        return false;

    H5Id h5file(H5Fopen(nameBytes.constData(), H5F_ACC_RDONLY, H5P_DEFAULT), H5Fclose);
    if(!h5file.valid())
        return false;

    // A VASP output file is identified by the presence of the /version group together with
    // the atomic structure stored under /results/positions, /input/poscar, or /structure/positions.
    bool isVasp = (H5Lexists(h5file, "version", H5P_DEFAULT) > 0)
        && (datasetExists(h5file, "/results/positions/position_ions")
            || datasetExists(h5file, "/input/poscar/position_ions")
            || datasetExists(h5file, "/structure/positions/position_ions"));
    return isVasp;
}

/******************************************************************************
* Scans the data file and builds a list of source frames.
******************************************************************************/
void VASPHDF5Importer::discoverFramesInFile(const FileHandle& fileHandle, QVector<FileSourceImporter::Frame>& frames) const
{
    QString filename = QDir::toNativeSeparators(fileHandle.localFilePath());
    if(filename.isEmpty())
        throw Exception(tr("The VASP HDF5 file reader supports reading only from physical files. Cannot read data from an in-memory buffer."));

    QMutexLocker locker(&hdf5Mutex);
    initializeHDF5Library();

    H5Id h5file(H5Fopen(filename.toUtf8().constData(), H5F_ACC_RDONLY, H5P_DEFAULT), H5Fclose);
    if(!h5file.valid())
        throw Exception(tr("Failed to open VASP HDF5 file for reading: %1").arg(filename));

    // The number of trajectory frames is given by the leading dimension of the ion-dynamics positions.
    std::vector<hsize_t> dims = datasetDims(h5file, "/intermediate/ion_dynamics/position_ions");
    size_t numFrames = (dims.size() == 3 && dims[0] > 0) ? (size_t)dims[0] : 1;

    Frame frame(fileHandle);
    for(size_t i = 0; i < numFrames; i++) {
        frame.byteOffset = (qint64)i;
        frame.label.setFrameOfFile(i);
        frames.push_back(frame);
    }
}

/******************************************************************************
* Parses the given input file.
******************************************************************************/
void VASPHDF5Importer::FrameLoader::loadFile()
{
    TaskProgress progress(this_task::ui());
    progress.setText(tr("Reading VASP HDF5 file %1").arg(fileHandle().toString()));

    QString filename = QDir::toNativeSeparators(fileHandle().localFilePath());
    if(filename.isEmpty())
        throw Exception(tr("The VASP HDF5 file reader supports reading only from physical files. Cannot read data from an in-memory buffer."));

    const size_t frameIndex = (size_t)std::max<qint64>(0, frame().byteOffset);

    QMutexLocker locker(&hdf5Mutex);
    initializeHDF5Library();

    H5Id h5file(H5Fopen(filename.toUtf8().constData(), H5F_ACC_RDONLY, H5P_DEFAULT), H5Fclose);
    if(!h5file.valid())
        throw Exception(tr("Failed to open VASP HDF5 file for reading: %1").arg(filename));

    // Determine whether the file contains an ion-dynamics trajectory.
    std::vector<hsize_t> trajDims = datasetDims(h5file, "/intermediate/ion_dynamics/position_ions");
    const bool isTrajectory = (trajDims.size() == 3 && trajDims[0] > 0);

    // Read the element type names and counts. These are constant across all frames.
    QStringList ionTypes = readStringArray(h5file, "/results/positions/ion_types");
    if(ionTypes.isEmpty())
        ionTypes = readStringArray(h5file, "/input/poscar/ion_types");
    if(ionTypes.isEmpty())
        ionTypes = readStringArray(h5file, "/structure/positions/ion_types");
    std::vector<int> ionCounts = readIntArray(h5file, "/results/positions/number_ion_types");
    if(ionCounts.empty())
        ionCounts = readIntArray(h5file, "/input/poscar/number_ion_types");
    if(ionCounts.empty())
        ionCounts = readIntArray(h5file, "/structure/positions/number_ion_types");
    if(ionCounts.empty())
        throw Exception(tr("Invalid or missing 'number_ion_types' dataset in VASP HDF5 file: %1").arg(filename));

    size_t totalAtomCount = 0;
    for(int c : ionCounts) {
        if(c < 0) throw Exception(tr("Invalid negative ion count in VASP HDF5 file: %1").arg(filename));
        totalAtomCount += (size_t)c;
    }
    if(totalAtomCount == 0)
        throw Exception(tr("VASP HDF5 file contains no atoms: %1").arg(filename));
    setParticleCount(totalAtomCount);

    // Read the simulation cell (lattice vectors) and the scale factor for the current frame.
    // The scale factor must be read from the same group as the lattice vectors: VASP stores the
    // ion-dynamics lattice in absolute (already-scaled) units with a scale of 1, whereas the
    // input/results lattice is given in scaled units that still need to be multiplied by the scale.
    std::vector<double> lattice;
    double scale = 1.0;
    if(isTrajectory && datasetExists(h5file, "/intermediate/ion_dynamics/lattice_vectors")) {
        std::vector<double> all = readDoubleArray(h5file, "/intermediate/ion_dynamics/lattice_vectors");
        if(all.size() >= (frameIndex + 1) * 9) {
            lattice.assign(all.begin() + frameIndex * 9, all.begin() + (frameIndex + 1) * 9);
            scale = readDoubleScalar(h5file, "/intermediate/ion_dynamics/scale", 1.0);
        }
    }
    if(lattice.size() != 9) {
        lattice = readDoubleArray(h5file, "/results/positions/lattice_vectors");
        scale = readDoubleScalar(h5file, "/results/positions/scale", 1.0);
    }
    if(lattice.size() != 9) {
        lattice = readDoubleArray(h5file, "/input/poscar/lattice_vectors");
        scale = readDoubleScalar(h5file, "/input/poscar/scale", 1.0);
    }
    if(lattice.size() != 9) {
        lattice = readDoubleArray(h5file, "/structure/positions/lattice_vectors");
        scale = readDoubleScalar(h5file, "/structure/positions/scale", 1.0);
    }
    if(lattice.size() != 9)
        throw Exception(tr("Invalid or missing 'lattice_vectors' dataset in VASP HDF5 file: %1").arg(filename));
    if(scale <= 0.0) scale = 1.0;

    // VASP stores the lattice as a row-major 3x3 matrix where row i is the i-th lattice
    // vector. OVITO's cell matrix uses the columns as the cell vectors, hence the transpose.
    AffineTransformation cell = AffineTransformation::Identity();
    for(size_t i = 0; i < 3; i++)
        for(size_t j = 0; j < 3; j++)
            cell(j, i) = lattice[i * 3 + j] * scale;
    simulationCell()->setCellMatrix(cell);
    simulationCell()->setPbcFlags(true, true, true);

    // Read the atomic positions for the current frame.
    std::vector<double> positions;
    if(isTrajectory) {
        std::vector<double> all = readDoubleArray(h5file, "/intermediate/ion_dynamics/position_ions");
        if(all.size() >= (frameIndex + 1) * totalAtomCount * 3)
            positions.assign(all.begin() + frameIndex * totalAtomCount * 3, all.begin() + (frameIndex + 1) * totalAtomCount * 3);
    }
    if(positions.size() != totalAtomCount * 3) {
        positions = readDoubleArray(h5file, "/results/positions/position_ions");
        if(positions.size() != totalAtomCount * 3)
            positions = readDoubleArray(h5file, "/input/poscar/position_ions");
        if(positions.size() != totalAtomCount * 3)
            positions = readDoubleArray(h5file, "/structure/positions/position_ions");
    }
    if(positions.size() != totalAtomCount * 3)
        throw Exception(tr("Invalid or missing 'position_ions' dataset in VASP HDF5 file: %1").arg(filename));

    const bool directCoords = readIntScalar(h5file, "/results/positions/direct_coordinates",
        readIntScalar(h5file, "/input/poscar/direct_coordinates",
            readIntScalar(h5file, "/structure/positions/direct_coordinates", 1))) != 0;

    // Create the particle properties for positions and types.
    Property* posProperty = particles()->createProperty(Particles::PositionProperty);
    Property* typeProperty = particles()->createProperty(Particles::TypeProperty);
    BufferWriteAccess<Point3, access_mode::discard_write> posAccess(posProperty);
    BufferWriteAccess<int32_t, access_mode::discard_write> typeAccess(typeProperty);

    auto* p = posAccess.begin();
    auto* a = typeAccess.begin();
    size_t atomIndex = 0;
    for(size_t atype = 1; atype <= ionCounts.size(); atype++) {
        int typeId = (int)atype;
        if(atype <= (size_t)ionTypes.size() && !ionTypes[atype - 1].isEmpty())
            typeId = addNamedType(Particles::OOClass(), typeProperty, ionTypes[atype - 1])->numericId();
        else
            addNumericType(Particles::OOClass(), typeProperty, (int)atype, QString{});
        for(int i = 0; i < ionCounts[atype - 1]; i++, ++p, ++a, ++atomIndex) {
            *a = typeId;
            Point3 pos(positions[atomIndex * 3 + 0], positions[atomIndex * 3 + 1], positions[atomIndex * 3 + 2]);
            *p = directCoords ? (cell * pos) : (pos * scale);
        }
    }
    posAccess.reset();
    typeAccess.reset();

    // Read per-atom velocities, if present.
    std::vector<double> velocities;
    if(isTrajectory && datasetExists(h5file, "/intermediate/ion_dynamics/ion_velocities")) {
        std::vector<double> all = readDoubleArray(h5file, "/intermediate/ion_dynamics/ion_velocities");
        if(all.size() >= (frameIndex + 1) * totalAtomCount * 3)
            velocities.assign(all.begin() + frameIndex * totalAtomCount * 3, all.begin() + (frameIndex + 1) * totalAtomCount * 3);
    }
    if(velocities.size() != totalAtomCount * 3)
        velocities = readDoubleArray(h5file, "/results/positions/ion_velocities");
    if(velocities.size() == totalAtomCount * 3) {
        BufferWriteAccess<Vector3, access_mode::discard_write> velAccess = particles()->createProperty(Particles::VelocityProperty);
        for(size_t i = 0; i < totalAtomCount; i++)
            velAccess[i] = Vector3(velocities[i * 3 + 0], velocities[i * 3 + 1], velocities[i * 3 + 2]);
    }

    // Read per-atom forces, if present (trajectory only).
    if(isTrajectory && datasetExists(h5file, "/intermediate/ion_dynamics/forces")) {
        std::vector<double> all = readDoubleArray(h5file, "/intermediate/ion_dynamics/forces");
        if(all.size() >= (frameIndex + 1) * totalAtomCount * 3) {
            BufferWriteAccess<Vector3, access_mode::discard_write> forceAccess = particles()->createProperty(Particles::ForceProperty);
            const double* f = all.data() + frameIndex * totalAtomCount * 3;
            for(size_t i = 0; i < totalAtomCount; i++)
                forceAccess[i] = Vector3(f[i * 3 + 0], f[i * 3 + 1], f[i * 3 + 2]);
        }
    }

    // Expose the per-frame energies as global attributes.
    if(isTrajectory && datasetExists(h5file, "/intermediate/ion_dynamics/energies")) {
        std::vector<hsize_t> edims = datasetDims(h5file, "/intermediate/ion_dynamics/energies");
        if(edims.size() == 2 && edims[1] > 0) {
            size_t ncol = (size_t)edims[1];
            std::vector<double> energies = readDoubleArray(h5file, "/intermediate/ion_dynamics/energies");
            QStringList tags = readStringArray(h5file, "/intermediate/ion_dynamics/energies_tags");
            if(energies.size() >= (frameIndex + 1) * ncol) {
                const double* e = energies.data() + frameIndex * ncol;
                for(size_t c = 0; c < ncol; c++) {
                    // Build a clean attribute name from the (possibly verbose) energy tag.
                    QString key = (c < (size_t)tags.size()) ? tags[c].simplified() : QStringLiteral("energy%1").arg(c);
                    key.replace(QChar(' '), QChar('_'));
                    if(key.isEmpty()) key = QStringLiteral("energy%1").arg(c);
                    state().setAttribute(QStringLiteral("VASP.%1").arg(key), QVariant::fromValue(e[c]), pipelineNode());
                }
            }
        }
    }

    QString statusString = tr("%1 atoms").arg(totalAtomCount);
    if(isTrajectory)
        statusString += tr(" (frame %1 of %2)").arg(frameIndex + 1).arg(trajDims[0]);

    // Read charge density grid if present (only for the first frame; field is constant across frames).
    if(frameIndex == 0) {
        this_task::throwIfCanceled();
        std::vector<hsize_t> cdims = datasetDims(h5file, "/charge/charge");
        if(cdims.size() == 4 && cdims[0] > 0 && cdims[1] > 0 && cdims[2] > 0 && cdims[3] > 0) {
            std::vector<double> chargeData = readDoubleArray(h5file, "/charge/charge");
            this_task::throwIfCanceled();
            if(chargeData.size() == cdims[0] * cdims[1] * cdims[2] * cdims[3])
                statusString += readChargeDensity((size_t)cdims[0], (size_t)cdims[3], (size_t)cdims[2], (size_t)cdims[1], chargeData, progress);
        }
    }

    state().setStatus(statusString);

    // Release the HDF5 lock before the potentially expensive bond generation.
    locker.unlock();

    // Generate ad-hoc bonds between atoms based on their van der Waals radii.
    if(_generateBonds)
        generateBonds(progress);
    else
        setBondCount(0);

    // Call base implementation to finalize the loaded particle data.
    ParticleImporter::FrameLoader::loadFile();
}

/******************************************************************************
* Stores the pre-read charge density (and optionally magnetization density)
* as a VoxelGrid in the pipeline state.
* chargeData: flat array in HDF5 C-order [nspins, nz, ny, nx] (x varies fastest).
******************************************************************************/
QString VASPHDF5Importer::FrameLoader::readChargeDensity(size_t nspins, size_t nx, size_t ny, size_t nz,
    const std::vector<double>& chargeData, TaskProgress& progress)
{
    const size_t gridSize = nx * ny * nz;

    // VASP stores charge density as ρ * Ω; divide by cell volume to obtain electrons/Å³.
    FloatType cellVolume = std::abs(simulationCell()->cellMatrix().determinant());
    if(cellVolume == 0.0) cellVolume = 1.0;

    // Create the voxel grid data object.
    VoxelGrid* voxelGrid = state().getMutableObject<VoxelGrid>();
    if(!voxelGrid) {
        voxelGrid = state().createObject<VoxelGrid>(pipelineNode(), tr("Charge density"));
        voxelGrid->visElement()->setEnabled(false);
        voxelGrid->visElement()->setTitle(voxelGrid->title());
        voxelGrid->visElement()->freezeInitialParameterValues({SNAPSHOT_PROPERTY_FIELD(ActiveObject::isEnabled), SNAPSHOT_PROPERTY_FIELD(ActiveObject::title)});
    }
    voxelGrid->setDomain(simulationCell());
    voxelGrid->setGridType(VoxelGrid::PointData);
    voxelGrid->setIdentifier(QStringLiteral("charge-density"));
    const VoxelGrid::GridDimensions gridDims = {nx, ny, nz};
    voxelGrid->setShape(gridDims);
    voxelGrid->setContent(gridSize, {});

    // Create "Charge Density" property.
    {
        BufferWriteAccess<FloatType, access_mode::discard_write> chargeAccess = voxelGrid->createProperty(QStringLiteral("Charge Density"), DataBuffer::FloatDefault);
        if(nspins == 1) {
            for(size_t i = 0; i < gridSize; i++)
                chargeAccess[i] = (FloatType)(chargeData[i] / cellVolume);
        } else {
            // Spin-polarized: spin[0] = spin-up, spin[1] = spin-down.
            // Total charge density = up + down.
            for(size_t i = 0; i < gridSize; i++)
                chargeAccess[i] = (FloatType)((chargeData[i] + chargeData[gridSize + i]) / cellVolume);
        }
    }

    // For spin-polarized calculations, also create a "Magnetization Density" property (up - down).
    if(nspins == 2) {
        BufferWriteAccess<FloatType, access_mode::discard_write> magAccess = voxelGrid->createProperty(QStringLiteral("Magnetization Density"), DataBuffer::FloatDefault);
        for(size_t i = 0; i < gridSize; i++)
            magAccess[i] = (FloatType)((chargeData[i] - chargeData[gridSize + i]) / cellVolume);
    }

    voxelGrid->verifyIntegrity();
    return tr("\nCharge density grid: %1 x %2 x %3").arg(nx).arg(ny).arg(nz);
}

/******************************************************************************
* Called when the pipeline is first created. Adds a CreateIsosurfaceModifier
* to the pipeline in GUI mode, using the mean charge density as the iso-level.
******************************************************************************/
Future<void> VASPHDF5Importer::setupPipeline(OORef<Pipeline> pipeline, OORef<FileSource> importObj)
{
    // Only in interactive (GUI) mode.
    if(!this_task::isInteractive())
        co_return;

    // Skip if the pipeline already contains a CreateIsosurfaceModifier.
    ModificationNode* modNode = dynamic_object_cast<ModificationNode>(pipeline->head());
    while(modNode) {
        if(dynamic_object_cast<CreateIsosurfaceModifier>(modNode->modifier()))
            co_return;
        modNode = dynamic_object_cast<ModificationNode>(modNode->input());
    }

    // Evaluate the pipeline to obtain the charge density grid.
    AnimationTime time = this_task::ui()->datasetContainer().currentAnimationTime();
    PipelineFlowState data;
    try {
        PipelineEvaluationResult future = pipeline->evaluatePipeline(PipelineEvaluationRequest(time));
        data = co_await FutureAwaiter(ObjectExecutor(this), std::move(future).asFuture());
    }
    catch(const Exception&) {
        co_return;
    }

    // Look up the charge-density voxel grid.
    const ConstDataObjectPath path = data.getObject<VoxelGrid>(QStringLiteral("charge-density"));
    const VoxelGrid* grid = path.lastAs<VoxelGrid>();
    if(!grid) co_return;

    BufferReadAccess<FloatType> chargeAccess(grid->getProperty(QStringLiteral("Charge Density")));
    if(!chargeAccess || chargeAccess.size() == 0) co_return;

    FloatType mean = std::accumulate(chargeAccess.begin(), chargeAccess.end(), (FloatType)0) / (FloatType)chargeAccess.size();

    AnimationSuspender animSuspender(*this_task::ui());

    OORef<CreateIsosurfaceModifier> mod = OORef<CreateIsosurfaceModifier>::create();
    mod->setIsolevel(mean);
    mod->setInputGrid(path);
    mod->setSourceProperty(QStringLiteral("Charge Density"));
    mod->surfaceMeshVis()->setSurfaceTransparency(0.333);
    pipeline->applyModifier(time, false, mod);
}

}   // End of namespace
