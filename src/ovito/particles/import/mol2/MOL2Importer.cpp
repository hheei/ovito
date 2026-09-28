// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/objects/Bonds.h>
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/particles/objects/BondType.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/utilities/io/CompressedTextReader.h>
#include <ovito/core/utilities/io/FileManager.h>
#include "MOL2Importer.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(MOL2Importer);
OVITO_CLASSINFO(MOL2Importer, "DisplayName", "Tripos MOL2");

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool MOL2Importer::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    CompressedTextReader stream(file);

    // Skip comment lines (starting with '#') and look for the @<TRIPOS>MOLECULE marker.
    for(int i = 0; i < 20 && !stream.eof(); i++) {
        const char* line = stream.readLineTrimLeft(256);
        if(line[0] == '#') continue;
        if(line[0] == '\0') continue;
        return strncmp(line, "@<TRIPOS>MOLECULE", 17) == 0;
    }
    return false;
}

/******************************************************************************
* Scans the data file and builds a list of source frames (one per MOLECULE).
******************************************************************************/
void MOL2Importer::discoverFramesInFile(const FileHandle& fileHandle, QVector<FileSourceImporter::Frame>& frames) const
{
    CompressedTextReader stream(fileHandle);

    TaskProgress progress(this_task::ui());
    progress.setText(tr("Scanning file %1").arg(fileHandle.toString()));
    progress.setMaximum(stream.underlyingSize());

    int frameNumber = 0;
    Frame frame(fileHandle);

    while(!stream.eof() && !this_task::isCanceled()) {
        // Save byte offset of the line we are about to read so we can record
        // the exact start position of each @<TRIPOS>MOLECULE section.
        qint64 lineOffset = stream.byteOffset();
        int lineNumber = stream.lineNumber();

        const char* line = stream.readLineTrimLeft();

        if(strncmp(line, "@<TRIPOS>MOLECULE", 17) == 0) {
            // For the first frame always use offset 0 to avoid an unnecessary reload
            // triggered by the FileSource.
            if(frameNumber == 0) {
                frame.byteOffset = 0;
                frame.lineNumber = 0;
            }
            else {
                frame.byteOffset = lineOffset;
                frame.lineNumber = lineNumber;
            }

            // Read the molecule name line and use it as the frame label if non-empty.
            if(!stream.eof()) {
                const char* nameLine = stream.readLine();
                const QString molName = QString::fromLatin1(nameLine).trimmed();
                if(!molName.isEmpty())
                    frame.label.setToString(molName);
                else
                    frame.label.setFrameOfFile(frameNumber);
            }
            else {
                frame.label.setFrameOfFile(frameNumber);
            }

            frameNumber++;
            frames.push_back(frame);
        }

        progress.setValueIntermittent(stream.byteOffset());
    }
}

namespace {

/// Maps a MOL2 bond type string to a type ID, display name, and numeric bond order.
struct Mol2BondType {
    int id;
    const char* name;
    GraphicsFloatType order;
};

Mol2BondType parseMol2BondType(const char* typeStr)
{
    OVITO_ASSERT(typeStr && typeStr[0] != '\0');
    if(typeStr[1] == '\0') {
        switch(typeStr[0]) {
            case '1': return {1, "Single",   1.0f};
            case '2': return {2, "Double",   2.0f};
            case '3': return {3, "Triple",   3.0f};
            case '4': return {4, "Aromatic", 1.5f};
        }
    }
    if(strncmp(typeStr, "ar", 2) == 0) return {4, "Aromatic",      1.5f};
    if(strncmp(typeStr, "am", 2) == 0) return {5, "Amide",         1.5f};
    if(strncmp(typeStr, "du", 2) == 0) return {6, "Dummy",         1.0f};
    if(strncmp(typeStr, "un", 2) == 0) return {7, "Unknown",       1.0f};
    if(strncmp(typeStr, "nc", 2) == 0) return {8, "Not Connected", 1.0f};
    return {1, "Single", 1.0f};
}

}  // namespace

/******************************************************************************
* Parses the given input file.
* Format specification: http://chemyang.ccnu.edu.cn/ccb/server/AIMMS/mol2.pdf
******************************************************************************/
void MOL2Importer::FrameLoader::loadFile()
{
    TaskProgress progress(this_task::ui());
    progress.setText(tr("Reading MOL2 file %1").arg(fileHandle().toString()));

    // Open file for reading, seeking to the start of this frame.
    CompressedTextReader stream(fileHandle(), frame().byteOffset, frame().lineNumber);

    // Scan forward to the @<TRIPOS>MOLECULE section for this frame.
    const char* line = stream.readLineTrimLeft(1024);
    while(!stream.eof() && strncmp(line, "@<TRIPOS>MOLECULE", 17) != 0) {
        if(this_task::isCanceled()) return;
        line = stream.readLineTrimLeft(1024);
    }
    if(stream.eof())
        throw Exception(tr("Invalid MOL2 file: missing @<TRIPOS>MOLECULE section in %1").arg(fileHandle().toString()));

    //--------------------------------------------------------------------------
    // Parse @<TRIPOS>MOLECULE header.
    //--------------------------------------------------------------------------

    // Line 1: molecule name (may be blank).
    line = stream.readLine(1024);
    state().setAttribute(QStringLiteral("MOL2.Molecule"), QVariant::fromValue(QString::fromLatin1(line).trimmed()), pipelineNode());

    // Line 2: num_atoms [num_bonds [num_subst [num_feat [num_sets]]]]
    line = stream.readLineTrimLeft(1024);
    qlonglong numAtoms = 0, numBonds = 0;
    if(sscanf(line, "%lld %lld", &numAtoms, &numBonds) < 1)
        throw Exception(tr("Invalid MOL2 file: cannot parse atom/bond counts in line %1: %2")
                            .arg(stream.lineNumber()).arg(stream.lineString().trimmed()));

    // Line 3: molecule type (optional, stop if next section marker encountered).
    QByteArray chargeType;
    line = stream.readLineTrimLeft(1024);
    if(line[0] != '\0' && line[0] != '@') {
        state().setAttribute(QStringLiteral("MOL2.MoleculeType"), QVariant::fromValue(QString::fromLatin1(line).trimmed()), pipelineNode());

        // Line 4: charge type (optional).
        line = stream.readLineTrimLeft(1024);
        if(line[0] != '\0' && line[0] != '@') {
            chargeType = QByteArray(line).trimmed();
            state().setAttribute(QStringLiteral("MOL2.ChargeType"), QVariant::fromValue(QString::fromLatin1(chargeType)), pipelineNode());
            line = stream.readLineTrimLeft(1024);
        }
    }

    if(this_task::isCanceled()) return;

    progress.setMaximum(numAtoms + numBonds);
    qlonglong rowsProcessed = 0;

    //--------------------------------------------------------------------------
    // Allocate particle property arrays.
    //--------------------------------------------------------------------------
    setParticleCount(numAtoms);

    // Always-present properties.
    BufferWriteAccess<Point3, access_mode::discard_write> posAccess = particles()->createProperty(Particles::PositionProperty);
    BufferWriteAccess<int64_t, access_mode::discard_write> idAccess = particles()->createProperty(Particles::IdentifierProperty);
    Property* typeProperty = particles()->createProperty(Particles::TypeProperty);
    BufferWriteAccess<int32_t, access_mode::discard_write> typeAccess(typeProperty);
    Property* hybridProperty = particles()->createProperty(QStringLiteral("Hybridization State"), DataBuffer::Int32);
    hybridProperty->setTitle(tr("Hybridization states"));
    BufferWriteAccess<int32_t, access_mode::discard_write> hybridAccess(hybridProperty);
    Property* atomNameProperty = particles()->createProperty(QStringLiteral("Atom Name"), DataBuffer::String);
    BufferWriteAccess<QString, access_mode::discard_write> atomNameAccess(atomNameProperty);

    // Optional properties — removed at the end if not found in file.
    Property* molIdProperty = particles()->createProperty(Particles::MoleculeProperty);
    BufferWriteAccess<int64_t, access_mode::discard_write> molIdAccess(molIdProperty);
    Property* molTypeProperty = particles()->createProperty(Particles::MoleculeTypeProperty);
    BufferWriteAccess<int32_t, access_mode::discard_write> molTypeAccess(molTypeProperty);
    Property* chargeProperty = particles()->createProperty(Particles::ChargeProperty);
    BufferWriteAccess<FloatType, access_mode::discard_write> chargeAccess(chargeProperty);

    Point3* posIter = posAccess.begin();
    int64_t* idIter = idAccess.begin();
    int32_t* typeIter = typeAccess.begin();
    int32_t* hybridIter = hybridAccess.begin();
    QString* atomNameIter = atomNameAccess.begin();
    int64_t* molIdIter = molIdAccess.begin();
    int32_t* molTypeIter = molTypeAccess.begin();
    FloatType* chargeIter = chargeAccess.begin();

    bool hasSubst = false, hasCharge = false;
    bool atomSectionRead = false;
    bool bondSectionRead = false;
    bool hasCrysin = false;

    // Helper: skip remaining lines of the current section until the next @<TRIPOS> header.
    auto skipToNextSection = [&]() {
        while(!stream.eof() && !this_task::isCanceled()) {
            line = stream.readLineTrimLeft(1024);
            if(line[0] == '@') break;
        }
    };

    //--------------------------------------------------------------------------
    // Section dispatch loop.
    //--------------------------------------------------------------------------
    while(!stream.eof() && !this_task::isCanceled()) {

        if(strncmp(line, "@<TRIPOS>MOLECULE", 17) == 0) {
            break;  // Next molecule found — stop parsing this frame.
        }
        else if(strncmp(line, "@<TRIPOS>ATOM", 13) == 0) {

            //------------------------------------------------------------------
            // @<TRIPOS>ATOM section.
            //------------------------------------------------------------------
            char atomName[64], atomType[64], substName[64];
            qlonglong atomId, substId;
            FloatType x, y, z, charge;

            for(qlonglong i = 0; i < numAtoms; i++) {
                progress.setValueIntermittent(++rowsProcessed);

                line = stream.readLine(1024);
                if(stream.eof() && i < numAtoms - 1)
                    throw Exception(tr("MOL2 file ended unexpectedly while reading atoms (line %1)").arg(stream.lineNumber()));

                substId = 1;
                substName[0] = '\0';
                charge = 0;

                int n = sscanf(line,
                    "%lld %63s " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " %63s %lld %63s " FLOATTYPE_SCANF_STRING,
                    &atomId, atomName, &x, &y, &z, atomType, &substId, substName, &charge);

                if(n < 6)
                    throw Exception(tr("MOL2 parse error at line %1 of %2: expected at least 6 columns, got: %3")
                                        .arg(stream.lineNumber()).arg(fileHandle().toString()).arg(stream.lineString().trimmed()));

                *posIter++ = Point3(x, y, z);
                *idIter++ = atomId;

                // Use only the element symbol (before '.') for Particle Type so that
                // modifiers expecting chemical symbols (e.g. BondOrderModifier) work correctly.
                // Store the full SYBYL type in the "Hybridization State" property.
                const char* dotPos = strchr(atomType, '.');
                int elemLen = dotPos ? (int)(dotPos - atomType) : (int)strlen(atomType);
                const QLatin1String elemSymbol(atomType, elemLen);
                const int elemId = (int)ParticleType::getChemicalElementFromSymbol(elemSymbol);
                addNumericType(Particles::OOClass(), typeProperty, elemId, elemSymbol);
                *typeIter++ = elemId;
                *hybridIter++ = addNamedType(Particles::OOClass(), hybridProperty, QLatin1String(atomType, (int)strlen(atomType)))->numericId();
                *atomNameIter++ = QString::fromLatin1(atomName, (int)strlen(atomName));

                if(n >= 7) {
                    hasSubst = true;
                    *molIdIter++ = substId;
                    *molTypeIter++ = (substName[0] != '\0')
                        ? addNamedType(Particles::OOClass(), molTypeProperty, QLatin1String(substName, (int)strlen(substName)))->numericId()
                        : 0;
                }
                else {
                    *molIdIter++ = 0;
                    *molTypeIter++ = 0;
                }

                if(n >= 9 && chargeType != "NO_CHARGES") {
                    hasCharge = true;
                    *chargeIter++ = charge;
                }
                else {
                    *chargeIter++ = 0;
                }
            }

            atomSectionRead = true;

            // Release write locks so subsequent property accesses work.
            posAccess.reset();
            idAccess.reset();
            typeAccess.reset();
            hybridAccess.reset();
            atomNameAccess.reset();
            molIdAccess.reset();
            molTypeAccess.reset();
            chargeAccess.reset();

            hybridProperty->sortElementTypesByName();

            if(!stream.eof()) line = stream.readLineTrimLeft(1024);
        }
        else if(strncmp(line, "@<TRIPOS>BOND", 13) == 0) {

            //------------------------------------------------------------------
            // @<TRIPOS>BOND section.
            //------------------------------------------------------------------
            setBondCount(numBonds);
            if(numBonds == 0) {
                skipToNextSection();
                continue;
            }

            Property* topologyProperty = bonds()->createProperty(Bonds::TopologyProperty);
            BufferWriteAccess<ParticleIndexPair, access_mode::discard_write> topoAccess(topologyProperty);
            Property* bondTypeProperty = bonds()->createProperty(Bonds::TypeProperty);
            BufferWriteAccess<int32_t, access_mode::discard_write> bondTypeAccess(bondTypeProperty);
            Property* bondOrderProperty = bonds()->createProperty(Bonds::OrderProperty);
            BufferWriteAccess<GraphicsFloatType, access_mode::discard_write> bondOrderAccess(bondOrderProperty);

            ParticleIndexPair* topoIter = topoAccess.begin();
            int32_t* bondTypeIter = bondTypeAccess.begin();
            GraphicsFloatType* bondOrderIter = bondOrderAccess.begin();

            for(qlonglong i = 0; i < numBonds; i++) {
                progress.setValueIntermittent(++rowsProcessed);

                line = stream.readLine(1024);
                if(stream.eof() && i < numBonds - 1)
                    throw Exception(tr("MOL2 file ended unexpectedly while reading bonds (line %1)").arg(stream.lineNumber()));

                qlonglong bondId, atom1, atom2;
                char bondTypeStr[16];
                bondTypeStr[0] = '1'; bondTypeStr[1] = '\0';
                if(sscanf(line, "%lld %lld %lld %15s", &bondId, &atom1, &atom2, bondTypeStr) < 3)
                    throw Exception(tr("MOL2 parse error at bond line %1 of %2: %3")
                                        .arg(stream.lineNumber()).arg(fileHandle().toString()).arg(stream.lineString().trimmed()));

                (*topoIter)[0] = atom1;   // 1-indexed; remapped below
                (*topoIter)[1] = atom2;
                ++topoIter;

                auto [typeId, typeName, order] = parseMol2BondType(bondTypeStr);
                addNumericType(Bonds::OOClass(), bondTypeProperty, typeId, QLatin1String(typeName));
                *bondTypeIter++ = typeId;
                *bondOrderIter++ = order;
            }

            topoAccess.reset();
            bondTypeAccess.reset();
            bondOrderAccess.reset();

            // Remap bond topology from 1-indexed to 0-indexed.
            for(ParticleIndexPair& ab :
                BufferWriteAccess<ParticleIndexPair, access_mode::read_write>(bonds()->getMutableProperty(Bonds::TopologyProperty))) {
                ab[0] -= 1;
                ab[1] -= 1;
            }

            bondSectionRead = true;

            if(!stream.eof()) line = stream.readLineTrimLeft(1024);
        }
        else if(strncmp(line, "@<TRIPOS>CRYSIN", 15) == 0) {

            //------------------------------------------------------------------
            // @<TRIPOS>CRYSIN section (crystallographic unit cell).
            //------------------------------------------------------------------
            line = stream.readLineTrimLeft(1024);
            FloatType a, b, c, alpha, beta, gamma;
            if(sscanf(line,
                      FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING
                      " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING,
                      &a, &b, &c, &alpha, &beta, &gamma) == 6) {

                // Convert crystallographic (a, b, c, α, β, γ) parameters to a Cartesian
                // cell matrix using the standard right-handed triclinic convention.
                // (Same computation as in PDBImporter.)
                AffineTransformation cell = AffineTransformation::Identity();
                if(alpha == 90 && beta == 90 && gamma == 90) {
                    cell(0,0) = a;
                    cell(1,1) = b;
                    cell(2,2) = c;
                }
                else if(alpha == 90 && beta == 90) {
                    FloatType g = qDegreesToRadians(gamma);
                    cell(0,0) = a;
                    cell(0,1) = b * std::cos(g);
                    cell(1,1) = b * std::sin(g);
                    cell(2,2) = c;
                }
                else {
                    FloatType al = qDegreesToRadians(alpha);
                    FloatType be = qDegreesToRadians(beta);
                    FloatType ga = qDegreesToRadians(gamma);
                    FloatType v = a * b * c * std::sqrt(FloatType(1)
                        - std::cos(al)*std::cos(al)
                        - std::cos(be)*std::cos(be)
                        - std::cos(ga)*std::cos(ga)
                        + FloatType(2) * std::cos(al) * std::cos(be) * std::cos(ga));
                    cell(0,0) = a;
                    cell(0,1) = b * std::cos(ga);
                    cell(1,1) = b * std::sin(ga);
                    cell(0,2) = c * std::cos(be);
                    cell(1,2) = c * (std::cos(al) - std::cos(be)*std::cos(ga)) / std::sin(ga);
                    cell(2,2) = v / (a * b * std::sin(ga));
                }
                simulationCell()->setCellMatrix(cell);
                simulationCell()->setPbcFlags(true, true, true);
                hasCrysin = true;
            }

            skipToNextSection();
        }
        else if(strncmp(line, "@<TRIPOS>", 9) == 0) {
            // All other @<TRIPOS> sections are skipped.
            skipToNextSection();
        }
        else {
            if(!stream.eof()) line = stream.readLineTrimLeft(1024);
        }
    }

    if(!atomSectionRead)
        throw Exception(tr("Invalid MOL2 file: no @<TRIPOS>ATOM section found in %1").arg(fileHandle().toString()));

    // Remove optional properties that were absent from this file.
    if(!hasSubst) {
        particles()->removeProperty(molIdProperty);
        particles()->removeProperty(molTypeProperty);
    }
    if(!hasCharge)
        particles()->removeProperty(chargeProperty);

    if(!hasCrysin) {
        // Set simulation cell from bounding box if no @<TRIPOS>CRYSIN section exists.
        generateBoundingBox();
    }
    else if(hasSimulationCell()) {
        // Wrap atomic coordinates into the unit cell if a cell was defined.
        particles()->wrapCoordinates(*simulationCell());
    }

    // Compute PBC shift vectors for bonds based on the minimum image convention if a cell is present.
    generateBondPeriodicImageProperty();

    state().setStatus(tr("%1 particles, %2 bonds").arg(numAtoms).arg(bondSectionRead ? numBonds : 0));

    // Sort particles by ID if requested.
    if(_sortParticles)
        particles()->sortById();

    // Finalize particle data.
    ParticleImporter::FrameLoader::loadFile();

    // Hint that more frames may follow in the file.
    if(!stream.eof())
        signalAdditionalFrames();
}

}  // namespace Ovito
