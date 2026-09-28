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
#include <ovito/stdobj/simcell/SimulationCell.h>
#include "MedeA_SCI_Importer.h"
#include "MedeA_SLI_Importer.h"

#include <sqlite3.h>
#include <QtEndian>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(MedeA_SLI_Importer);
OVITO_CLASSINFO(MedeA_SLI_Importer, "DisplayName", "MedeA SLI");

namespace {

/// SQLite3 magic header bytes (first 16 bytes of every SQLite database file).
constexpr char kSQLiteMagic[16] = {
    'S','Q','L','i','t','e',' ','f','o','r','m','a','t',' ','3','\0'
};

/// Tables that must all be present for a file to be recognised as a valid SLI database.
/// "info" is intentionally omitted — some SLI files lack it.
constexpr const char* kRequiredTables[] = { "structure", "frame", "property", "frame_property" };

/******************************************************************************
* RAII wrapper for a sqlite3* connection that closes on destruction.
******************************************************************************/
struct SqliteGuard {
    sqlite3* db = nullptr;
    explicit SqliteGuard(sqlite3* p) : db(p) {}
    ~SqliteGuard() { if(db) sqlite3_close(db); }
    SqliteGuard(const SqliteGuard&) = delete;
    SqliteGuard& operator=(const SqliteGuard&) = delete;
};

/******************************************************************************
* RAII wrapper that finalises a prepared sqlite3 statement on destruction.
******************************************************************************/
struct StmtGuard {
    sqlite3_stmt* stmt = nullptr;
    explicit StmtGuard(sqlite3_stmt* s) : stmt(s) {}
    ~StmtGuard() { sqlite3_finalize(stmt); }
    StmtGuard(const StmtGuard&) = delete;
    StmtGuard& operator=(const StmtGuard&) = delete;
};

/******************************************************************************
* Opens the SQLite database at path in read-only mode.
* Returns nullptr on failure.
******************************************************************************/
sqlite3* openReadOnly(const QString& path)
{
    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(path.toUtf8().constData(), &db, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, nullptr);
    if(rc != SQLITE_OK) {
        if(db) sqlite3_close(db);
        return nullptr;
    }
    return db;
}

/******************************************************************************
* Returns true if the named table exists in the database.
******************************************************************************/
bool tableExists(sqlite3* db, const char* tableName)
{
    sqlite3_stmt* stmt = nullptr;
    if(sqlite3_prepare_v2(db,
            "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1 LIMIT 1",
            -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(stmt, 1, tableName, -1, SQLITE_STATIC);
    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return found;
}

/******************************************************************************
* Returns true if a column with the given name exists in a table.
******************************************************************************/
bool columnExists(sqlite3* db, const char* tableName, const char* columnName)
{
    QByteArray sql = QByteArray("PRAGMA table_info(") + tableName + ")";
    sqlite3_stmt* stmt = nullptr;
    if(sqlite3_prepare_v2(db, sql.constData(), -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    bool found = false;
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        const char* col = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if(col && strcmp(col, columnName) == 0) {
            found = true;
            break;
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

/******************************************************************************
* Decompresses a zlib-compressed BLOB stored in an SLI frame_property row.
* The values in SLI databases are stored as zlib-compressed UTF-8 text.
* Returns the decompressed text, or an empty string on failure.
******************************************************************************/
QByteArray decompressSLIBlob(const void* blob, int blobSize)
{
    if(!blob || blobSize < 2)
        return {};
    // Qt's qUncompress() requires a 4-byte big-endian uncompressed-size header
    // before the raw zlib stream. Property values are short strings, so 4 KiB
    // is always sufficient as the initial buffer estimate.
    constexpr quint32 kEstimate = 4096;
    char header[4];
    qToBigEndian(kEstimate, reinterpret_cast<unsigned char*>(header));
    QByteArray wrapper(static_cast<const char*>(blob), blobSize);
    wrapper.prepend(QByteArrayView(header, sizeof(header)));
    return qUncompress(wrapper);
}

/******************************************************************************
* Parses a Tcl braced vector list "{{x y z} {x y z} ...}" into a vector of
* Point3 values.  The outer braces are optional.
******************************************************************************/
QVector<Point3> parseTclVec3List(const QByteArray& text, qsizetype reserveCount = 0)
{
    QVector<Point3> result;
    if(reserveCount > 0) result.reserve(reserveCount);

    const char* p   = text.constData();
    const char* end = p + text.size();

    // Skip leading whitespace and optional outer opening brace.
    while(p < end && std::isspace((unsigned char)*p)) ++p;
    if(p < end && *p == '{') ++p;

    while(p < end) {
        while(p < end && std::isspace((unsigned char)*p)) ++p;
        if(p >= end || *p == '}') break;
        if(*p != '{') break;
        ++p; // skip opening '{'

        double vals[3] = {};
        for(double& v : vals) {
            while(p < end && std::isspace((unsigned char)*p)) ++p;
            char* endPtr;
            v = std::strtod(p, &endPtr);
            p = endPtr;
        }

        // Advance past the closing '}'.
        while(p < end && *p != '}') ++p;
        if(p < end) ++p;

        result.push_back(Point3((FloatType)vals[0], (FloatType)vals[1], (FloatType)vals[2]));
    }
    return result;
}

}  // anonymous namespace

/******************************************************************************
* Checks if the given file has a format that can be read by this importer.
******************************************************************************/
bool MedeA_SLI_Importer::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    // SQLite requires a local file path — reject in-memory and remote files.
    QString path = QDir::toNativeSeparators(file.localFilePath());
    if(path.isEmpty() || path.startsWith(QChar(':')))
        return false;

    // Verify the 16-byte SQLite magic header before opening the database engine.
    {
        auto dev = file.createIODevice();
        if(!dev || !dev->open(QIODevice::ReadOnly))
            return false;
        char header[16];
        if(dev->read(header, 16) != 16)
            return false;
        if(memcmp(header, kSQLiteMagic, 16) != 0)
            return false;
    }

    // Open the database and verify all required SLI tables are present.
    SqliteGuard guard(openReadOnly(file.localFilePath()));
    if(!guard.db)
        return false;
    for(const char* tbl : kRequiredTables) {
        if(!tableExists(guard.db, tbl))
            return false;
    }

    return true;
}

/******************************************************************************
* Scans the SLI database to discover all animation frames.
*
* Two cases are handled:
*   (a) The 'frame' table is empty: each row in 'structure' becomes one OVITO
*       frame.  frame.byteOffset = structure.id, frame.lineNumber = 0.
*   (b) The 'frame' table is non-empty for the first structure: each row
*       becomes one trajectory frame.  frame.byteOffset = frame.id,
*       frame.lineNumber = sid of the first structure.
*
* If both structures and frames are present, the trajectory frames of the
* first structure take priority and any additional structures are ignored.
******************************************************************************/
void MedeA_SLI_Importer::discoverFramesInFile(const FileHandle& fileHandle, QVector<FileSourceImporter::Frame>& frames) const
{
    TaskProgress progress(this_task::ui());
    progress.setText(tr("Scanning file %1").arg(fileHandle.toString()));

    QString path = fileHandle.localFilePath();
    if(path.isEmpty())
        throw Exception(tr("The MedeA SLI file reader supports reading only from physical files."));

    SqliteGuard guard(openReadOnly(path));
    if(!guard.db)
        throw Exception(tr("Cannot open MedeA SLI database: '%1'. Invalid or wrong file format.").arg(path));

    // Helper: prepare a statement and throw on failure.
    auto prepareOrThrow = [&](const char* sql) -> sqlite3_stmt* {
        sqlite3_stmt* s = nullptr;
        if(sqlite3_prepare_v2(guard.db, sql, -1, &s, nullptr) != SQLITE_OK)
            throw Exception(tr("Failed to query MedeA SLI database '%1': %2")
                .arg(path, QString::fromUtf8(sqlite3_errmsg(guard.db))));
        return s;
    };

    // Step 1: read all structures ordered by id
    struct StructureInfo {
        sqlite3_int64 id;
        QString name;
        QString formula;
    };
    QVector<StructureInfo> structures;
    {
        StmtGuard sg(prepareOrThrow("SELECT id, name, formula FROM structure ORDER BY id"));
        while(sqlite3_step(sg.stmt) == SQLITE_ROW) {
            StructureInfo& s = structures.emplace_back();
            s.id = sqlite3_column_int64(sg.stmt, 0);
            if(const char* n = reinterpret_cast<const char*>(sqlite3_column_text(sg.stmt, 1)))
                s.name = QString::fromUtf8(n);
            if(const char* f = reinterpret_cast<const char*>(sqlite3_column_text(sg.stmt, 2)))
                s.formula = QString::fromUtf8(f);
        }
    }
    if(structures.isEmpty())
        throw Exception(tr("No structures found in MedeA SLI database '%1'. Invalid or empty file.").arg(path));

    // When loading a trajectory, only the first structure is considered.
    const sqlite3_int64 firstSid = structures.front().id;

    // Step 2: count trajectory frames for the first structure
    sqlite3_int64 frameCount = 0;
    {
        StmtGuard sg(prepareOrThrow("SELECT COUNT(*) FROM frame WHERE sid=?1"));
        sqlite3_bind_int64(sg.stmt, 1, firstSid);
        if(sqlite3_step(sg.stmt) == SQLITE_ROW)
            frameCount = sqlite3_column_int64(sg.stmt, 0);
    }

    if(frameCount > 1) {
        // --- Case (b): trajectory frames ---

        // Detect optional name/formula columns in the frame table.
        const bool hasFrameName    = columnExists(guard.db, "frame", "name");
        const bool hasFrameFormula = columnExists(guard.db, "frame", "formula");

        // Find the 'Time' property id for the first structure (if present).
        sqlite3_int64 timePropId = -1;
        {
            StmtGuard sg(prepareOrThrow("SELECT id FROM property WHERE sid=?1 AND name='Time' LIMIT 1"));
            sqlite3_bind_int64(sg.stmt, 1, firstSid);
            if(sqlite3_step(sg.stmt) == SQLITE_ROW)
                timePropId = sqlite3_column_int64(sg.stmt, 0);
        }

        // Build the SELECT list dynamically based on available columns.
        // Column order: f.id [, f.name] [, f.formula] , fp.value
        QByteArray sql = "SELECT f.id";
        if(hasFrameName)    sql += ", f.name";
        if(hasFrameFormula) sql += ", f.formula";
        sql += ", fp.value FROM frame f"
               " LEFT JOIN frame_property fp ON fp.fid=f.id AND fp.prid=?1"
               " WHERE f.sid=?2 ORDER BY f.id";

        StmtGuard sg(prepareOrThrow(sql.constData()));
        // When timePropId == -1 no frame_property row will match (prid ≥ 1), so
        // fp.value will always be NULL, which is the correct "no time" signal.
        sqlite3_bind_int64(sg.stmt, 1, timePropId);
        sqlite3_bind_int64(sg.stmt, 2, firstSid);

        while(sqlite3_step(sg.stmt) == SQLITE_ROW) {
            const sqlite3_int64 fid = sqlite3_column_int64(sg.stmt, 0);
            int colIdx = 1;

            // Read optional name and formula columns.
            QString frameName;
            if(hasFrameName) {
                if(const char* n = reinterpret_cast<const char*>(sqlite3_column_text(sg.stmt, colIdx++)))
                    frameName = QString::fromUtf8(n);
            }
            if(hasFrameFormula) {
                if(frameName.isEmpty()) {
                    if(const char* f = reinterpret_cast<const char*>(sqlite3_column_text(sg.stmt, colIdx)))
                        frameName = QString::fromUtf8(f);
                }
                colIdx++;
            }

            // Decompress the Time blob (last column).
            bool hasTime = false;
            FloatType timeValue = 0;
            if(timePropId >= 0 && sqlite3_column_type(sg.stmt, colIdx) != SQLITE_NULL) {
                const void* blob     = sqlite3_column_blob(sg.stmt, colIdx);
                const int   blobSize = sqlite3_column_bytes(sg.stmt, colIdx);
                const QByteArray text = decompressSLIBlob(blob, blobSize);
                if(!text.isEmpty()) {
                    bool ok;
                    const double t = text.toDouble(&ok);
                    if(ok) {
                        timeValue = FloatType(t);
                        hasTime = true;
                    }
                }
            }

            Frame fd(fileHandle);
            fd.byteOffset = fid;
            fd.lineNumber  = static_cast<int>(firstSid);
            if(!frameName.isEmpty())
                fd.label.setToString(frameName);
            else if(hasTime)
                fd.label.setToTime(timeValue);
            frames.push_back(std::move(fd));
        }
    }
    else {
        // --- Case (a): multiple static structures, no trajectory frames ---
        Frame fd(fileHandle);
        for(const StructureInfo& s : structures) {
            fd.byteOffset = s.id;
            fd.lineNumber  = 0; // signals structure-only load (no trajectory frame)
            if(!s.name.isEmpty())
                fd.label.setToString(s.name);
            else if(!s.formula.isEmpty())
                fd.label.setToString(s.formula);
            else
                fd.label.setToString(QStringLiteral("Structure %1").arg(s.id));
            frames.push_back(fd);
        }
    }
}

/******************************************************************************
* Reads the frame data from the SLI database.
*
* For both structure frames (lineNumber == 0) and trajectory frames
* (lineNumber > 0) the base topology is loaded from structure.tsstring via
* MedeA_SCI_Importer::FrameLoader.  For trajectory frames, the particle
* positions and simulation cell are subsequently overridden with data from the
* frame_property table (Coordinates and CellVectors properties).
******************************************************************************/
void MedeA_SLI_Importer::FrameLoader::loadFile()
{
    const QString path = fileHandle().localFilePath();
    if(path.isEmpty())
        throw Exception(tr("The MedeA SLI file reader supports reading only from physical files."));

    TaskProgress progress(this_task::ui());
    progress.setText(tr("Reading MedeA SLI file %1").arg(fileHandle().toString()));

    // Phase 1: open the database and fetch the compressed SCI blob for this frame's initial structure.
    // Phase 2: load the trajectory frame's atomic coordinates
    if(frame().lineNumber != 0)
        progress.beginSubSteps({3, 1});

    SqliteGuard guard(openReadOnly(path));
    if(!guard.db)
        throw Exception(tr("Cannot open MedeA SLI database: '%1'.").arg(path));

    // For trajectory frames (lineNumber > 0): sid = lineNumber, fid = byteOffset.
    // For structure frames (lineNumber == 0): sid = byteOffset.
    const sqlite3_int64 sid = (frame().lineNumber != 0) ? frame().lineNumber : frame().byteOffset;

    // Fetch the compressed SCI blob for the structure with the given sid.
    sqlite3_stmt* rawStmt = nullptr;
    if(sqlite3_prepare_v2(guard.db,
            "SELECT tsstring FROM structure WHERE id=?1",
            -1, &rawStmt, nullptr) != SQLITE_OK)
        throw Exception(tr("Failed to query structure table in MedeA SLI database: '%1'.").arg(path));
    StmtGuard sg(rawStmt);
    sqlite3_bind_int64(sg.stmt, 1, sid);

    QByteArray sciData;
    if(sqlite3_step(sg.stmt) == SQLITE_ROW) {
        const void* blob     = sqlite3_column_blob(sg.stmt, 0);
        const int   blobSize = sqlite3_column_bytes(sg.stmt, 0);
        sciData = decompressSLIBlob(blob, blobSize);
    }

    if(sciData.isEmpty())
        throw Exception(tr("Empty or missing structure data for structure id %1 in MedeA SLI database '%2'.")
            .arg(sid).arg(path));

    // Delegate parsing to the SCI frame loader via loadFileBody(), which accepts the
    // raw SCI body buffer directly and bypasses the file-format header check,
    // avoiding an O(N) header-prepend copy.
    LoadOperationRequest sciRequest = loadRequest();
    MedeA_SCI_Importer::FrameLoader sciLoader(sciRequest, _recenterCell, _generateBoundingBox, false);
    sciLoader.loadFileBody(sciData.constData(), sciData.constData() + sciData.size(), progress);

    // For trajectory frames, override the static positions and cell matrix with
    // the current frame's data from the frame_property table.
    if(frame().lineNumber != 0) {
        progress.nextSubStep();
        const sqlite3_int64 fid = frame().byteOffset;

        // Helper: fetch and decompress a named frame property for this frame.
        auto fetchFrameProperty = [&](const char* propName) -> QByteArray {
            sqlite3_stmt* s = nullptr;
            if(sqlite3_prepare_v2(guard.db,
                    "SELECT fp.value FROM frame_property fp"
                    " JOIN property p ON p.id = fp.prid"
                    " WHERE fp.fid=?1 AND p.sid=?2 AND p.name=?3 LIMIT 1",
                    -1, &s, nullptr) != SQLITE_OK)
                return {};
            StmtGuard sg2(s);
            sqlite3_bind_int64(s, 1, fid);
            sqlite3_bind_int64(s, 2, sid);
            sqlite3_bind_text(s, 3, propName, -1, SQLITE_STATIC);
            if(sqlite3_step(s) != SQLITE_ROW) return {};
            return decompressSLIBlob(sqlite3_column_blob(s, 0), sqlite3_column_bytes(s, 0));
        };

        // Override simulation cell vectors.
        const QByteArray cellData = fetchFrameProperty("CellVectors");
        if(!cellData.isEmpty() && sciLoader.hasSimulationCell()) {
            const QVector<Point3> vecs = parseTclVec3List(cellData, 3);
            if(vecs.size() == 3) {
                AffineTransformation cellMatrix = AffineTransformation::Zero();
                for(int col = 0; col < 3; ++col) {
                    cellMatrix(0, col) = vecs[col].x();
                    cellMatrix(1, col) = vecs[col].y();
                    cellMatrix(2, col) = vecs[col].z();
                }
                sciLoader.simulationCell()->setCellMatrix(cellMatrix);
            }
        }

        // Override particle positions.
        const QByteArray coordData = fetchFrameProperty("Coordinates");
        if(!coordData.isEmpty()) {
            const size_t nParticles = sciLoader.particles()->elementCount();
            const QVector<Point3> newCoords = parseTclVec3List(coordData, (qsizetype)nParticles);
            if((size_t)newCoords.size() == nParticles) {
                if(BufferWriteAccess<Point3, access_mode::read_write> posAccess = sciLoader.particles()->getMutableProperty(Particles::PositionProperty)) {
                    for(size_t i = 0; i < nParticles; ++i)
                        posAccess[i] = newCoords[i];
                }

                // Since the atoms have moved, we need to update the periodic image vectors of the bonds to account
                // for possible wrap-around changes.
                sciLoader.generateBondPeriodicImageProperty();

                if(_generateBoundingBox && (!sciLoader.hasSimulationCell() || !sciLoader.simulationCell()->hasPbcCorrected())) {
                    // Recompute the bounding box from the new coordinates to update the ad-hoc simulation cell.
                    sciLoader.generateBoundingBox();
                }
            }
        }

        // Import recognised scalar frame properties as OVITO global attributes.
        // Each property is looked up by its short SLI name first, then by the long alias.
        static constexpr struct { const char* shortName; const char* longName; const char* attrName; } kScalarProps[] = {
            {"E",    "Energy",          "SLI.Energy"},
            {"T",    "Temperature",     "SLI.Temperature"},
            {"V",    "Volume",          "SLI.Volume"},
            {"P",    "Pressure",        "SLI.Pressure"},
            {"Epot", "PotentialEnergy", "SLI.PotentialEnergy"},
            {"Ekin", "KineticEnergy",   "SLI.KineticEnergy"},
        };
        for(const auto& prop : kScalarProps) {
            QByteArray data = fetchFrameProperty(prop.shortName);
            if(data.isEmpty())
                data = fetchFrameProperty(prop.longName);
            if(!data.isEmpty()) {
                bool ok;
                const double value = data.toDouble(&ok);
                if(ok)
                    sciLoader.state().setAttribute(QString::fromUtf8(prop.attrName), QVariant::fromValue(value), pipelineNode());
            }
        }
        progress.endSubSteps();
    }

    // Adopt the fully processed output state from the SCI loader.
    state() = std::move(sciLoader.state());
}

}  // namespace Ovito
