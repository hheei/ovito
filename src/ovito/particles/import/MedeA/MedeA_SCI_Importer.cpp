// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/objects/Bonds.h>
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/particles/objects/BondType.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/utilities/io/CompressedTextReader.h>
#include <ovito/core/utilities/io/NumberParsing.h>
#include "MedeA_SCI_Importer.h"

#include <gemmi/symmetry.hpp>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(MedeA_SCI_Importer);
OVITO_CLASSINFO(MedeA_SCI_Importer, "DisplayName", "MedeA SCI");

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool MedeA_SCI_Importer::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    CompressedTextReader stream(file);
    const char* line = stream.readLineTrimLeft(64);
    return strncmp(line, "#MD System", 10) == 0 || strncmp(line, "#SciCo System", 13) == 0;
}

namespace {

/******************************************************************************
* Reads the next Tcl list token from *p, advancing p past it and trailing
* whitespace. Returns a string_view of the token value (outer braces stripped),
* pointing directly into the source buffer — no copy is made. Returns an
* empty view if no more tokens are found on the line.
******************************************************************************/
std::string_view nextTclToken(const char*& p)
{
    // Skip leading whitespace (but not newlines).
    while(*p == ' ' || *p == '\t') ++p;

    if(*p == '\0' || *p == '\n' || *p == '\r')
        return {};

    if(*p == '{') {
        // Braced token — track nesting depth.
        const char* start = p + 1;  // content starts after opening brace
        int depth = 1;
        ++p;
        while(*p != '\0' && depth > 0) {
            if(*p == '{') ++depth;
            else if(*p == '}') --depth;
            ++p;
        }
        // p now points past the closing '}'; content is [start, p-1).
        std::string_view token(start, static_cast<size_t>(p - start - 1));
        // Skip trailing whitespace.
        while(*p == ' ' || *p == '\t') ++p;
        return token;
    }
    else {
        // Plain word — read until whitespace or end.
        const char* start = p;
        while(*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') ++p;
        std::string_view token(start, static_cast<size_t>(p - start));
        while(*p == ' ' || *p == '\t') ++p;
        return token;
    }
}

/******************************************************************************
* Trims leading and trailing ASCII whitespace from a [begin, end) range.
******************************************************************************/
std::string trimmed(const char* s, const char* e)
{
    while(s < e && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')) ++s;
    while(e > s && (*(e-1) == ' ' || *(e-1) == '\t' || *(e-1) == '\n' || *(e-1) == '\r')) --e;
    return std::string(s, e);
}

/******************************************************************************
* Parses a braced 3-vector token "x y z" into three doubles.
******************************************************************************/
bool parseVector3(std::string_view inner, double& x, double& y, double& z)
{
    namespace qi = boost::spirit::qi;
    const char* p   = inner.data();
    const char* end = p + inner.size();
    const char* p0  = p;
    if(qi::phrase_parse(p, end, qi::double_ >> qi::double_ >> qi::double_, qi::space, x, y, z))
        return true;
    // Fallback for numbers Boost.Spirit cannot parse (e.g. very large/small exponents).
    std::istringstream iss(std::string(p0, end));
    iss.imbue(std::locale::classic());
    return bool(iss >> x >> y >> z);
}

/******************************************************************************
* Parses a braced 3-int token "i j k" into three ints.
******************************************************************************/
bool parseVector3i(std::string_view inner, Vector3I& vec)
{
    namespace qi = boost::spirit::qi;
    const char* p   = inner.data();
    const char* end = p + inner.size();
    return qi::phrase_parse(p, end, qi::int_ >> qi::int_ >> qi::int_, qi::space, vec.x(), vec.y(), vec.z());
}

/******************************************************************************
* Maps an SCI bond order integer to OVITO type ID, display name, and float order.
* SCI encoding: 0=single, 1=aromatic, 2=double, 3=triple
******************************************************************************/
struct SciBondInfo
{
    int typeId;
    QLatin1StringView name;
    float order;
};

SciBondInfo sciBondInfo(int sciOrder)
{
    constexpr std::array<SciBondInfo, 4> bondInfos = {{
        {1, QLatin1StringView("Single"),   1.0f},
        {2, QLatin1StringView("Aromatic"), 1.5f},
        {3, QLatin1StringView("Double"),   2.0f},
        {4, QLatin1StringView("Triple"),   3.0f}
    }};
    return bondInfos[(sciOrder >= 0 && sciOrder < 4) ? sciOrder : 0];
}

/******************************************************************************
* Represents a single table section in the SCI file (schema + raw row data).
******************************************************************************/
struct ColumnDef {
    std::string name;
    std::string type;         // "int", "double", "float", "string", "reference"
    std::string refTable;     // only for "reference" type
    std::string defaultValue; // default value string from schema (empty for "reference")
};

struct RawTable {
    std::string name;
    std::vector<ColumnDef> columns;
    std::unordered_map<std::string, int> colIndex;   // column name → 0-based position in row

    // Flat, row-major storage: cells[row * ncols + col] — string_views into the file buffer.
    size_t rowCount = 0;
    std::vector<std::string_view> cells;

    /// Returns -1 if column is absent.
    int col(const char* colName) const {
        auto it = colIndex.find(colName);
        return (it != colIndex.end()) ? it->second : -1;
    }

    /// Returns true if column is present.
    bool hasCol(const char* colName) const { return colIndex.count(colName) > 0; }

    /// Returns a string_view for the given cell, or an empty view if indices are out of range.
    std::string_view cell(size_t row, int col) const {
        if(col < 0 || static_cast<size_t>(col) >= columns.size() || row >= rowCount)
            return {};
        return cells[row * columns.size() + static_cast<size_t>(col)];
    }
};

/******************************************************************************
* Reconstructs the order in which MedeA enumerates symmetry-equivalent atoms
* during space-group expansion.
*
* MedeA derives this order from its internal (BYU/Miller-Love) space-group
* tables, which list the symmetry operations in a different sequence than
* gemmi. The Bond table references atoms by this MedeA-internal index, so to
* remap bonds onto OVITO's gemmi-based atom ordering we must reproduce the
* same enumeration order.
*
* MedeA's order, when expressed in the space group's *reference* setting,
* follows a fixed canonical scheme: lattice-centering translations form the
* outer loop (origin first), and within each the point operations are sorted
* with the proper rotations first, each ranked by its reference-frame rotation
* matrix. Converting back to the file's actual setting (via the change-of-basis
* operator) reproduces MedeA's sequence for all primitive space-group settings.
*
* Returns the operations in MedeA's enumeration order. The caller applies them
* to each asymmetric atom, deduplicates, and matches the generated positions
* against OVITO's expanded atoms to obtain the index bijection.
******************************************************************************/
std::vector<gemmi::Op> reconstructMedeaOperationOrder(const gemmi::SpaceGroup* sg)
{
    gemmi::GroupOps go = sg->operations();
    const gemmi::Op basisop = sg->basisop();
    const gemmi::Op basisInv = basisop.inverse();

    // Determinant of an integer rotation matrix (entries scaled by Op::DEN).
    auto detRot = [](const gemmi::Op::Rot& r) -> long {
        return (long)r[0][0] * ((long)r[1][1]*r[2][2] - (long)r[1][2]*r[2][1])
             - (long)r[0][1] * ((long)r[1][0]*r[2][2] - (long)r[1][2]*r[2][0])
             + (long)r[0][2] * ((long)r[1][0]*r[2][1] - (long)r[1][1]*r[2][0]);
    };
    constexpr long DEN3 = (long)gemmi::Op::DEN * gemmi::Op::DEN * gemmi::Op::DEN;

    // Compute a canonical sort key for each symmetry operation from its
    // representation in the reference setting.
    struct KeyedOp {
        std::array<int, 10> key;  // [0]: 0=proper, 1=improper; [1..9]: rotation matrix
        gemmi::Op op;
    };
    std::vector<KeyedOp> syms;
    syms.reserve(go.sym_ops.size());
    for(const gemmi::Op& op : go.sym_ops) {
        const gemmi::Op ref = basisInv * op * basisop;
        const long det = detRot(ref.rot) / DEN3;  // +1 (proper) or -1 (improper)
        KeyedOp k;
        k.key[0] = (det > 0) ? 0 : 1;             // proper rotations sort first
        const int s = (det > 0) ? 1 : -1;         // map improper ops to their proper-rotation equivalent
        int idx = 1;
        for(int i = 0; i < 3; i++)
            for(int j = 0; j < 3; j++)
                k.key[idx++] = -s * ref.rot[i][j]; // negate so identity-like matrices sort first
        k.op = op;
        syms.push_back(std::move(k));
    }
    std::sort(syms.begin(), syms.end(), [](const KeyedOp& a, const KeyedOp& b) { return a.key < b.key; });

    // Centering translations form the outer loop, ordered with the origin first.
    std::vector<gemmi::Op::Tran> cens = go.cen_ops;
    std::sort(cens.begin(), cens.end());

    std::vector<gemmi::Op> ordered;
    ordered.reserve(go.sym_ops.size() * cens.size());
    for(const gemmi::Op::Tran& cen : cens) {
        for(const KeyedOp& k : syms) {
            gemmi::Op op = k.op;
            op.tran[0] += cen[0];
            op.tran[1] += cen[1];
            op.tran[2] += cen[2];
            ordered.push_back(op);
        }
    }
    return ordered;
}

/******************************************************************************
* Bead type descriptor for mesoscale systems.
******************************************************************************/
struct BeadTypeDef {
    std::string name;
    double mass   = 0.0;
    double radius = 0.0;
    double charge = 0.0;
    Color  color  = Color(0.5f, 0.5f, 0.5f);
};

}  // anonymous namespace

/******************************************************************************
* Parses the given input file.
******************************************************************************/
void MedeA_SCI_Importer::FrameLoader::loadFile()
{
    TaskProgress progress(this_task::ui());
    progress.setText(tr("Reading MedeA SCI file %1").arg(fileHandle().toString()));

    CompressedTextReader stream(fileHandle(), frame().byteOffset, frame().lineNumber);

    // Validate header line.
    const char* line = stream.readLineTrimLeft(256);
    if(strncmp(line, "#MD System", 10) != 0 && strncmp(line, "#SciCo System", 13) != 0)
        throw Exception(tr("Not a valid MedeA SCI file: unexpected header in %1").arg(fileHandle().toString()));

    // Map the rest of the file into memory for zero-copy parsing.
    // For uncompressed files, mmap() avoids any data copy.
    // For compressed files, readAll() decompresses into a single buffer.
    const char* bufStart;
    const char* bufEnd;
    QByteArray fileContents;
    std::tie(bufStart, bufEnd) = stream.mmap();
    if(!bufStart) {
        fileContents = stream.readAll();
        bufStart = fileContents.constData();
        bufEnd   = bufStart + fileContents.size();
    }

    loadFileBody(bufStart, bufEnd, progress);

    // Unmap the memory-mapped region (no-op if readAll() was used instead).
    if(fileContents.isEmpty()) stream.munmap();
}

/******************************************************************************
* Parses SCI body data from a raw memory buffer, bypassing
* the file-format header. Called by loadFile() after header validation, and
* also directly by MedeA_SLI_Importer.
******************************************************************************/
void MedeA_SCI_Importer::FrameLoader::loadFileBody(const char* bufStart, const char* bufEnd, TaskProgress& progress)
{
    // Phase 1 (weight=1): parse tables
    // Phase 2 (weight=2): process data
    progress.beginSubSteps({1, 2});

    // Set the progress maximum to the file size, and update progress value as we advance through the buffer.
    progress.setMaximum(static_cast<qlonglong>(bufEnd - bufStart));

    // Scan all table sections into RawTable objects.
    // All string_view cells point directly into the mapped/buffered region.
    std::string titleStr;
    std::unordered_map<std::string, RawTable> tables;  // keyed by table name

    // Helper: advance p to the start of the next line, returning the trimmed
    // line content as [lineStart, lineContentEnd). Returns {nullptr,nullptr} at EOF.
    auto nextLine = [&](const char*& p) -> std::pair<const char*, const char*> {
        while(p < bufEnd) {
            const char* ls = p;
            while(p < bufEnd && *p != '\n') ++p;
            const char* le = p;                    // points at '\n' or bufEnd
            if(p < bufEnd) ++p;                    // skip '\n'
            // Trim leading whitespace.
            while(ls < le && (*ls == ' ' || *ls == '\t')) ++ls;
            // Trim trailing '\r'.
            while(le > ls && *(le-1) == '\r') --le;
            if(ls < le) return {ls, le};           // non-empty line
        }
        return {nullptr, nullptr};
    };

    const char* p = bufStart;
    while(p < bufEnd && !this_task::isCanceled()) {
        auto [ls, le] = nextLine(p);
        if(!ls) break;

        if(le - ls >= 6 && strncmp(ls, "@Title", 6) == 0) {
            titleStr = trimmed(ls + 6, le);
        }
        else if(le - ls >= 8 && strncmp(ls, "@Columns", 8) == 0) {
            // Parse schema block.
            RawTable tbl;
            tbl.name = trimmed(ls + 8, le);

            while(p < bufEnd) {
                auto [sl, se] = nextLine(p);
                if(!sl) break;
                if(se - sl >= 4 && strncmp(sl, "@end", 4) == 0) break;

                // Each schema line: <ColumnName> <Type> [<refTable>] [<default>]
                const char* sp = sl;
                std::string colName(nextTclToken(sp));
                std::string colType(nextTclToken(sp));
                std::string extra(nextTclToken(sp));   // refTable or default

                if(colName.empty() || colType.empty()) continue;

                ColumnDef cd;
                cd.name = std::move(colName);
                cd.type = std::move(colType);
                for(char& c : cd.type) c = (char)std::tolower((unsigned char)c);
                if(cd.type == "reference")
                    cd.refTable = std::move(extra);
                else
                    cd.defaultValue = std::move(extra);

                tbl.colIndex[cd.name] = static_cast<int>(tbl.columns.size());
                tbl.columns.push_back(std::move(cd));
            }

            // Parse the immediately following @data block.
            {
                auto [dl, de] = nextLine(p);
                if(dl && de - dl >= 5 && strncmp(dl, "@data", 5) == 0) {
                    const size_t ncols = tbl.columns.size();
                    while(p < bufEnd) {
                        auto [dal, dae] = nextLine(p);
                        if(!dal) break;
                        if(dae - dal >= 4 && strncmp(dal, "@end", 4) == 0) break;

                        // Tokenize the row — string_views point into the buffer, no copy.
                        const char* tok = dal;
                        for(size_t ci = 0; ci < ncols; ++ci)
                            tbl.cells.push_back(nextTclToken(tok));
                        ++tbl.rowCount;
                        progress.setValueIntermittent(static_cast<qlonglong>(p - bufStart));
                    }
                }
            }

            tables.emplace(tbl.name, std::move(tbl));
        }
    }
    progress.nextSubStep();

    if(!titleStr.empty())
        state().setAttribute(QStringLiteral("SCI.Title"), QVariant::fromValue(QString::fromStdString(titleStr)), pipelineNode());

    // Determine system type.
    const bool hasAtom          = tables.count("Atom") > 0;
    const bool hasAsymAtom      = tables.count("AsymmetricAtom") > 0;
    const bool hasCell          = tables.count("Cell") > 0;
    const bool hasBead          = tables.count("Bead") > 0;
    const bool hasBond          = tables.count("Bond") > 0;
    const bool hasAsymBond      = tables.count("AsymmetricBond") > 0;
    const bool hasProperties    = tables.count("Properties") > 0;

    // Detect mesoscale: the Atom table (non-periodic) or the AsymmetricAtom table (periodic)
    // has a "Bead" reference column.
    const bool isMesoscale = hasBead && (
        (hasAtom && tables.at("Atom").hasCol("Bead")) ||
        (hasAsymAtom && tables.at("AsymmetricAtom").hasCol("Bead")));

    // Detect periodicity.
    const bool isPeriodic = hasCell;

    // In periodic mode, if there is no separate "Atom" (Cartesian) table but only
    // "AsymmetricAtom" (fractional), we need to perform gemmi symmetry expansion.
    const bool needsGemmiExpansion = isPeriodic && hasAsymAtom && !hasAtom;

    if(!hasAtom && !hasAsymAtom)
        throw Exception(tr("Invalid MedeA SCI file: no Atom or AsymmetricAtom table found in %1").arg(fileHandle().toString()));

    // Determine progress step count.
    qlonglong totalRows = 0, rowsProcessed = 0;
    if(hasAtom) totalRows += tables.at("Atom").rowCount;
    if(hasAsymAtom) totalRows += tables.at("AsymmetricAtom").rowCount * 5;
    if(hasBond) totalRows += tables.at("Bond").rowCount;
    if(hasAsymBond) totalRows += tables.at("AsymmetricBond").rowCount;
    if(totalRows == 0) totalRows = 1; // avoid zero max for progress bar
    progress.setMaximum(totalRows);

    // Process Cell table (if periodic).
    AffineTransformation cellMatrix = AffineTransformation::Identity();
    gemmi::GroupOps symOps;  // default: empty (only identity will be used for expansion)
    const gemmi::SpaceGroup* spaceGroup = nullptr;  // resolved space group (for MedeA expansion-order reconstruction)
    int spaceGroupNumber = 1;

    if(hasCell) {
        const RawTable& cellTbl = tables.at("Cell");
        if(cellTbl.rowCount == 0)
            throw Exception(tr("MedeA SCI file has an empty Cell table in %1").arg(fileHandle().toString()));

        // Parse lattice parameters {a b c α β γ}.
        int paramsCol = cellTbl.col("Parameters");
        double a = 10, b = 10, c = 10, alpha = 90, beta = 90, gamma = 90;
        if(paramsCol >= 0) {
            std::string_view sv = cellTbl.cell(0, paramsCol);
            namespace qi = boost::spirit::qi;
            const char* cp = sv.data(); const char* ce = cp + sv.size();
            qi::phrase_parse(cp, ce,
                qi::double_ >> qi::double_ >> qi::double_ >>
                qi::double_ >> qi::double_ >> qi::double_,
                qi::space, a, b, c, alpha, beta, gamma);
        }

        // Convert (a, b, c, α, β, γ) → Cartesian cell matrix (same formula as MOL2Importer).
        if(alpha == 90 && beta == 90 && gamma == 90) {
            cellMatrix(0,0) = a;
            cellMatrix(1,1) = b;
            cellMatrix(2,2) = c;
        }
        else if(alpha == 90 && beta == 90) {
            FloatType g = qDegreesToRadians((FloatType)gamma);
            cellMatrix(0,0) = (FloatType)a;
            cellMatrix(0,1) = (FloatType)(b * std::cos(g));
            cellMatrix(1,1) = (FloatType)(b * std::sin(g));
            cellMatrix(2,2) = (FloatType)c;
        }
        else {
            FloatType al = qDegreesToRadians((FloatType)alpha);
            FloatType be = qDegreesToRadians((FloatType)beta);
            FloatType ga = qDegreesToRadians((FloatType)gamma);
            FloatType v = (FloatType)(a * b * c) * std::sqrt(FloatType(1)
                - std::cos(al)*std::cos(al)
                - std::cos(be)*std::cos(be)
                - std::cos(ga)*std::cos(ga)
                + FloatType(2) * std::cos(al) * std::cos(be) * std::cos(ga));
            cellMatrix(0,0) = (FloatType)a;
            cellMatrix(0,1) = (FloatType)(b * std::cos(ga));
            cellMatrix(1,1) = (FloatType)(b * std::sin(ga));
            cellMatrix(0,2) = (FloatType)(c * std::cos(be));
            cellMatrix(1,2) = (FloatType)(c * (std::cos(al) - std::cos(be)*std::cos(ga)) / std::sin(ga));
            cellMatrix(2,2) = v / ((FloatType)(a * b) * std::sin(ga));
        }

        simulationCell()->setCellMatrix(cellMatrix);
        simulationCell()->setPbcFlags(true, true, true);

        // Space group.
        int sgNumCol = cellTbl.col("SpaceGroupNumber");
        int sgStrCol = cellTbl.col("SpaceGroup");
        std::string sgName;
        if(sgNumCol >= 0) {
            if(!parseInt32(cellTbl.cell(0, sgNumCol), spaceGroupNumber))
                spaceGroupNumber = 1;
        }
        if(sgStrCol >= 0)
            sgName = std::string(cellTbl.cell(0, sgStrCol));

        state().setAttribute(QStringLiteral("SCI.SpaceGroupNumber"), QVariant::fromValue(spaceGroupNumber), pipelineNode());
        if(!sgName.empty())
            state().setAttribute(QStringLiteral("SCI.SpaceGroup"), QVariant::fromValue(QString::fromStdString(sgName)), pipelineNode());

        // Look up gemmi space group (for symmetry expansion).
        // Name is checked first so that non-standard settings (e.g. "Pbnm" instead of
        // the standard "Pnma") are resolved correctly; number lookup always returns the
        // standard setting and would apply the wrong symmetry operations in that case.
        if(needsGemmiExpansion) {
            if(!sgName.empty())
                spaceGroup = gemmi::find_spacegroup_by_name(sgName);
            if(!spaceGroup)
                spaceGroup = gemmi::find_spacegroup_by_number(spaceGroupNumber);
            if(spaceGroup)
                symOps = spaceGroup->operations();
        }
    }

    // Process Bead table (mesoscale mode).
    std::vector<BeadTypeDef> beadTypes;
    if(isMesoscale) {
        const RawTable& beadTbl = tables.at("Bead");
        int nameCol   = beadTbl.col("Name");
        int massCol   = beadTbl.col("Mass");
        int radCol    = beadTbl.col("Radius");
        int colorCol  = beadTbl.col("Color");
        int chargeCol = beadTbl.col("Charge");

        for(size_t ri = 0; ri < beadTbl.rowCount; ri++) {
            BeadTypeDef bt;
            double tmpD = 0;
            if(nameCol >= 0)
                bt.name = std::string(beadTbl.cell(ri, nameCol));
            if(massCol >= 0) {
                parseFloatType(beadTbl.cell(ri, massCol), bt.mass);
            }
            if(radCol >= 0) {
                parseFloatType(beadTbl.cell(ri, radCol), bt.radius);
            }
            if(chargeCol >= 0) {
                parseFloatType(beadTbl.cell(ri, chargeCol), bt.charge);
            }
            if(colorCol >= 0) {
                double r = 0.5, g = 0.5, bl = 0.5;
                parseVector3(beadTbl.cell(ri, colorCol), r, g, bl);
                bt.color = Color((FloatType)r, (FloatType)g, (FloatType)bl);
            }
            beadTypes.push_back(std::move(bt));
        }
    }

    // Helper: registers the bead type with the given index in the type property (creating the
    // OVITO particle type with name, radius, color and mass on first encounter) and returns the
    // numeric type ID to assign to the particle. Out-of-range indices fall back to bead 0.
    auto assignBeadType = [&](Property* typeProperty, int beadIdx) -> int32_t {
        if(beadIdx < 0 || beadIdx >= static_cast<int>(beadTypes.size()))
            beadIdx = 0;
        const BeadTypeDef& bt = beadIdx < static_cast<int>(beadTypes.size())
            ? beadTypes[beadIdx] : BeadTypeDef{};
        const ElementType* et = addNumericType(Particles::OOClass(), typeProperty, beadIdx,
            QLatin1String(bt.name.data(), (qsizetype)bt.name.size()));
        if(et) {
            ParticleType* mutableType = static_object_cast<ParticleType>(typeProperty->makeMutable(et));
            // Log in type radius, color, and mass assigned by the file reader as default value for the particle type.
            // This is needed for the Python code generator to detect changes subsequently made by the user.
            if(bt.radius > 0) {
                mutableType->setRadius((FloatType)bt.radius);
                mutableType->freezeInitialParameterValues({SNAPSHOT_PROPERTY_FIELD(ParticleType::radius)});
            }
            if(bt.color != Color(0.5f, 0.5f, 0.5f)) {
                mutableType->setColor(bt.color);
                mutableType->freezeInitialParameterValues({SNAPSHOT_PROPERTY_FIELD(ElementType::color)});
            }
            if(bt.mass > 0) {
                mutableType->setMass((FloatType)bt.mass);
                mutableType->freezeInitialParameterValues({SNAPSHOT_PROPERTY_FIELD(ParticleType::mass)});
            }
        }
        return beadIdx;
    };

    // Process AsymmetricBond table (buffer bond orders for periodic bonds).
    std::vector<int> asymBondOrders;
    if(hasAsymBond) {
        const RawTable& abt = tables.at("AsymmetricBond");
        int orderCol = abt.col("Order");
        for(size_t ri = 0; ri < abt.rowCount; ri++) {
            int ord = 0;
            if(orderCol >= 0) {
                parseInt32(abt.cell(ri, orderCol), ord);
            }
            asymBondOrders.push_back(ord);
            progress.setValueIntermittent(++rowsProcessed);
        }
    }

    // Process atom table.

    // Helper: returns the cell value for the given row and column index as a string_view.
    auto colVal = [](const RawTable& tbl, size_t ri, int ci) -> std::string_view {
        return tbl.cell(ri, ci);
    };

    if(needsGemmiExpansion) {
        // --- Periodic system, AsymmetricAtom only: use gemmi to expand ---
        const RawTable& asymAtomTbl = tables.at("AsymmetricAtom");
        const qlonglong numAsym = static_cast<qlonglong>(asymAtomTbl.rowCount);

        int anCol      = asymAtomTbl.col("AtomicNumber");
        int beadCol    = asymAtomTbl.col("Bead");    // mesoscale only
        int fracCol    = asymAtomTbl.col("Fractional");
        int nameCol    = asymAtomTbl.col("Name");
        int ffTypeCol  = asymAtomTbl.col("FFAtomType");
        int ffChargeCol= asymAtomTbl.col("FFCharge");
        int massCol    = asymAtomTbl.col("Mass");
        int spinCol    = asymAtomTbl.col("Spin");

        // Collect fractional coordinates.
        std::vector<std::array<double, 3>> asymFrac(numAsym, {0.0, 0.0, 0.0});
        for(qlonglong i = 0; i < numAsym; i++) {
            if(fracCol >= 0) {
                double fx, fy, fz;
                if(parseVector3(colVal(asymAtomTbl, static_cast<size_t>(i), fracCol), fx, fy, fz))
                    asymFrac[i] = {fx, fy, fz};
            }
            progress.setValueIntermittent(++rowsProcessed);
        }

        // Expand using gemmi symmetry operations.
        // expandedAtoms[k] = {wrappedFrac, asymAtomIndex}.
        struct ExpandedAtom {
            std::array<double,3> frac;
            qlonglong asymIdx;
        };
        std::vector<ExpandedAtom> expandedAtoms;

        // Collect all operations (combining sym_ops × cen_ops).
        std::vector<gemmi::Op> allOps;
        if(symOps.sym_ops.empty()) {
            // Default: identity only.
            allOps.push_back(gemmi::Op::identity());
        }
        else {
            for(const gemmi::Op& sop : symOps.sym_ops) {
                for(const auto& cen : symOps.cen_ops) {
                    gemmi::Op combined = sop;
                    combined.tran[0] += cen[0];
                    combined.tran[1] += cen[1];
                    combined.tran[2] += cen[2];
                    allOps.push_back(combined);
                }
            }
        }

        constexpr double kEps = 1e-4;
        if(allOps.size() == 1) {
            // Single operation (P1 / identity): every asymmetric atom maps to exactly one
            // site with no possible duplicates. Skip the O(N²) deduplication scan.
            expandedAtoms.reserve(numAsym);
            for(qlonglong i = 0; i < numAsym; i++) {
                auto [nx, ny, nz] = allOps[0].apply_to_xyz(asymFrac[i]);
                nx -= std::floor(nx);
                ny -= std::floor(ny);
                nz -= std::floor(nz);
                expandedAtoms.push_back({{nx, ny, nz}, i});
            }
        }
        else {
            for(qlonglong i = 0; i < numAsym; i++) {
                for(const gemmi::Op& op : allOps) {
                    auto [nx, ny, nz] = op.apply_to_xyz(asymFrac[i]);
                    // Wrap to [0,1).
                    nx -= std::floor(nx);
                    ny -= std::floor(ny);
                    nz -= std::floor(nz);
                    // Deduplicate.
                    bool dup = false;
                    for(const ExpandedAtom& ea : expandedAtoms) {
                        if(std::abs(nx - ea.frac[0]) < kEps &&
                           std::abs(ny - ea.frac[1]) < kEps &&
                           std::abs(nz - ea.frac[2]) < kEps) { dup = true; break; }
                    }
                    if(!dup)
                        expandedAtoms.push_back({{nx, ny, nz}, i});
                }
            }
        }

        const qlonglong numExpanded = static_cast<qlonglong>(expandedAtoms.size());
        setParticleCount(numExpanded);

        // Create properties.
        BufferWriteAccess<Point3, access_mode::discard_write> posAccess = particles()->createProperty(Particles::PositionProperty);
        Property* typeProperty = particles()->createProperty(Particles::TypeProperty);
        BufferWriteAccess<int32_t, access_mode::discard_write> typeAccess(typeProperty);
        BufferWriteAccess<int64_t, access_mode::discard_write> idAccess = particles()->createProperty(Particles::IdentifierProperty);

        Property* atomNameProp  = particles()->createProperty(QStringLiteral("Atom Name"), DataBuffer::String);
        BufferWriteAccess<QString, access_mode::discard_write> atomNameAccess(atomNameProp);
        Property* ffTypeProp    = particles()->createProperty(QStringLiteral("Force-Field Type"), DataBuffer::Int32);
        ffTypeProp->setTitle(tr("Force-field atom types"));
        BufferWriteAccess<int32_t, access_mode::discard_write> ffTypeAccess(ffTypeProp);
        addNumericType(Particles::OOClass(), ffTypeProp, 0, QStringLiteral("None"));  // reserve 0 for "None" type
        Property* chargeProp    = particles()->createProperty(Particles::ChargeProperty);
        BufferWriteAccess<FloatType, access_mode::discard_write> chargeAccess(chargeProp);
        Property* massProp      = particles()->createProperty(Particles::MassProperty);
        BufferWriteAccess<FloatType, access_mode::discard_write> massAccess(massProp);
        Property* spinProp      = particles()->createProperty(Particles::SpinProperty);
        BufferWriteAccess<FloatType, access_mode::discard_write> spinAccess(spinProp);

        bool hasFFType = false, hasCharge = false, hasMass = false, hasSpin = false;

        for(qlonglong k = 0; k < numExpanded; k++) {
            if(k % (numExpanded / numAsym) == 0)
                progress.setValueIntermittent(rowsProcessed += 4);

            const ExpandedAtom& ea = expandedAtoms[k];
            const size_t asymRow = static_cast<size_t>(ea.asymIdx);

            // Position: fractional → Cartesian.
            Point3 cartPos = cellMatrix * Point3(
                (FloatType)ea.frac[0],
                (FloatType)ea.frac[1],
                (FloatType)ea.frac[2]);
            posAccess[k] = cartPos;

            idAccess[k] = k + 1;

            // Particle type.
            if(isMesoscale && beadCol >= 0) {
                // Mesoscale: type determined by bead index.
                int beadIdx = 0;
                parseInt32(colVal(asymAtomTbl, asymRow, beadCol), beadIdx);
                typeAccess[k] = assignBeadType(typeProperty, beadIdx);
            }
            else {
                // Atomistic: type determined by atomic number.
                int atomicNumber = 0;
                if(anCol >= 0) {
                    parseInt32(colVal(asymAtomTbl, asymRow, anCol), atomicNumber);
                }
                if(atomicNumber >= 0 && atomicNumber < (int)ParticleType::ChemicalElement::NUMBER_OF_PREDEFINED_CHEMICAL_TYPES)
                    addNumericType(Particles::OOClass(), typeProperty, atomicNumber, ParticleType::getChemicalElementSymbol(static_cast<ParticleType::ChemicalElement>(atomicNumber)));
                typeAccess[k] = atomicNumber;
            }

            // Atom name.
            if(nameCol >= 0) {
                std::string_view sv = colVal(asymAtomTbl, asymRow, nameCol);
                atomNameAccess[k] = QString::fromUtf8(QByteArrayView(sv));
            }
            else
                atomNameAccess[k].clear();

            // Force-field atom type.
            int32_t ffTypeId = 0;
            if(ffTypeCol >= 0) {
                std::string_view fft = colVal(asymAtomTbl, asymRow, ffTypeCol);
                if(!fft.empty()) {
                    hasFFType = true;
                    ffTypeId = addNamedType(Particles::OOClass(), ffTypeProp,
                                            QLatin1String(fft.data(), (qsizetype)fft.size()))->numericId();
                }
            }
            ffTypeAccess[k] = ffTypeId;

            // Charge (FFCharge).
            FloatType charge = 0;
            if(ffChargeCol >= 0) {
                double q = 0;
                if(parseFloatType(colVal(asymAtomTbl, asymRow, ffChargeCol), q) && q != 0.0) {
                    hasCharge = true;
                    charge = (FloatType)q;
                }
            }
            chargeAccess[k] = charge;

            // Mass.
            FloatType mass = 0;
            if(massCol >= 0) {
                double m = 0;
                if(parseFloatType(colVal(asymAtomTbl, asymRow, massCol), m) && m != 0.0) {
                    hasMass = true;
                    mass = (FloatType)m;
                }
            }
            massAccess[k] = mass;

            // Spin.
            FloatType spin = 0;
            if(spinCol >= 0) {
                double s = 0;
                if(parseFloatType(colVal(asymAtomTbl, asymRow, spinCol), s) && s != 0.0) {
                    hasSpin = true;
                    spin = (FloatType)s;
                }
            }
            spinAccess[k] = spin;
        }

        posAccess.reset();
        typeAccess.reset();
        idAccess.reset();
        atomNameAccess.reset();
        ffTypeAccess.reset();
        chargeAccess.reset();
        massAccess.reset();
        spinAccess.reset();

        if(!isMesoscale) typeProperty->sortElementTypesById();
        ffTypeProp->sortElementTypesByName();

        if(!hasFFType)  { particles()->removeProperty(ffTypeProp);  ffTypeProp  = nullptr; }
        if(!hasCharge)  { particles()->removeProperty(chargeProp);  chargeProp  = nullptr; }
        if(!hasMass)    { particles()->removeProperty(massProp);    massProp    = nullptr; }
        if(!hasSpin)    { particles()->removeProperty(spinProp);    spinProp    = nullptr; }

        // Build MedeA-index → our-index mapping for Bond table atom index remapping.
        // MedeA expands asymmetric atoms using symmetry operations in a different order than
        // gemmi, so the atom indices referenced by the Bond table do not match OVITO's atom
        // ordering. We reconstruct MedeA's enumeration order (see
        // reconstructMedeaOperationOrder()), regenerate the expansion in that order, and look
        // up each generated position in a reverse map of our gemmi expansion to obtain the
        // bijection medeaIdx → ourIdx. For P1 (single identity operation) no remapping is
        // needed and this block is skipped — the orderings are trivially identical.
        std::vector<qlonglong> medeaToOurIdx;
        if(hasBond && spaceGroup && allOps.size() > 1) {
            // Quantise fractional coords (wrapped to [0,1)) to integer keys for O(1) lookup.
            auto qcoord = [](double c) -> int64_t {
                return ((int64_t)std::lround((c - std::floor(c)) * 10000.0)) % 10000;
            };
            auto posKey64 = [&](double x, double y, double z) -> int64_t {
                return (qcoord(x) << 28) | (qcoord(y) << 14) | qcoord(z);
            };
            std::unordered_map<int64_t, qlonglong> posToOurIdx;
            posToOurIdx.reserve(numExpanded);
            for(qlonglong k = 0; k < numExpanded; k++)
                posToOurIdx.emplace(posKey64(expandedAtoms[k].frac[0], expandedAtoms[k].frac[1], expandedAtoms[k].frac[2]), k);

            // Regenerate the expansion in MedeA's enumeration order, deduplicating per
            // asymmetric atom, and map each MedeA atom slot to our gemmi-based index.
            const std::vector<gemmi::Op> medeaOps = reconstructMedeaOperationOrder(spaceGroup);
            medeaToOurIdx.reserve(numExpanded);
            for(qlonglong i = 0; i < numAsym; i++) {
                std::vector<std::array<double,3>> seen;
                for(const gemmi::Op& op : medeaOps) {
                    auto [nx, ny, nz] = op.apply_to_xyz(asymFrac[i]);
                    nx -= std::floor(nx); ny -= std::floor(ny); nz -= std::floor(nz);
                    bool dup = false;
                    for(const auto& s : seen)
                        if(std::abs(nx-s[0])<kEps && std::abs(ny-s[1])<kEps && std::abs(nz-s[2])<kEps) { dup=true; break; }
                    if(!dup) {
                        seen.push_back({nx, ny, nz});
                        auto it = posToOurIdx.find(posKey64(nx, ny, nz));
                        medeaToOurIdx.push_back(it != posToOurIdx.end() ? it->second : (qlonglong)medeaToOurIdx.size());
                    }
                }
            }
        }

        // Bonds for periodic gemmi-expansion mode.
        // Use expanded Bond table if available (Atom1/Atom2 reference expanded atom indices).
        // Otherwise use AsymmetricBond and generate bonds via gemmi.
        if(hasBond) {
            const RawTable& bondTbl = tables.at("Bond");
            int a1Col       = bondTbl.col("Atom1");
            int a2Col       = bondTbl.col("Atom2");
            int abCol       = bondTbl.col("AsymmetricBond");
            int co2Col      = bondTbl.col("CellOffset2");

            const qlonglong numBonds = static_cast<qlonglong>(bondTbl.rowCount);
            setBondCount(numBonds);

            Property* topoProp    = bonds()->createProperty(Bonds::TopologyProperty);
            Property* bTypeProp   = bonds()->createProperty(Bonds::TypeProperty);
            Property* bOrderProp  = bonds()->createProperty(Bonds::OrderProperty);
            Property* bImgProp    = bonds()->createProperty(Bonds::PeriodicImageProperty);

            BufferWriteAccess<ParticleIndexPair, access_mode::discard_write> topoAcc(topoProp);
            BufferWriteAccess<int32_t, access_mode::discard_write> bTypeAcc(bTypeProp);
            BufferWriteAccess<GraphicsFloatType, access_mode::discard_write> bOrderAcc(bOrderProp);
            BufferWriteAccess<Vector3I, access_mode::discard_write> bImgAcc(bImgProp);

            for(qlonglong bi = 0; bi < numBonds; bi++) {
                progress.setValueIntermittent(++rowsProcessed);
                const size_t bri = static_cast<size_t>(bi);

                qlonglong a1 = 0, a2 = 0;
                if(a1Col >= 0) { int64_t tmp = 0; parseInt64(colVal(bondTbl, bri, a1Col), tmp); a1 = tmp; }
                if(a2Col >= 0) { int64_t tmp = 0; parseInt64(colVal(bondTbl, bri, a2Col), tmp); a2 = tmp; }
                // Remap MedeA-ordered atom indices to our gemmi expansion ordering.
                if(!medeaToOurIdx.empty()) {
                    if(a1 >= 0 && a1 < (qlonglong)medeaToOurIdx.size()) a1 = medeaToOurIdx[a1];
                    if(a2 >= 0 && a2 < (qlonglong)medeaToOurIdx.size()) a2 = medeaToOurIdx[a2];
                }
                topoAcc[bi][0] = a1;
                topoAcc[bi][1] = a2;

                // Bond order from AsymmetricBond reference.
                int sciOrd = 0;
                if(abCol >= 0 && !asymBondOrders.empty()) {
                    int abIdx = 0;
                    parseInt32(colVal(bondTbl, bri, abCol), abIdx);
                    if(abIdx >= 0 && abIdx < static_cast<int>(asymBondOrders.size()))
                        sciOrd = asymBondOrders[abIdx];
                }
                auto bi_info = sciBondInfo(sciOrd);
                addNumericType(Bonds::OOClass(), bTypeProp, bi_info.typeId, bi_info.name);
                bTypeAcc[bi] = bi_info.typeId;
                bOrderAcc[bi] = bi_info.order;

                // Periodic image offset.
                Vector3I img = Vector3I::Zero();
                if(co2Col >= 0)
                    parseVector3i(colVal(bondTbl, bri, co2Col), img);
                bImgAcc[bi] = img;
            }

            topoAcc.reset();
            bTypeAcc.reset();
            bOrderAcc.reset();
            bImgAcc.reset();
        }
        else if(hasAsymBond) {
            // Generate expanded bonds from AsymmetricBond using gemmi.
            const RawTable& abt = tables.at("AsymmetricBond");
            int a1Col  = abt.col("Atom1");
            int a2Col  = abt.col("Atom2");
            int ordCol = abt.col("Order");

            // Build lookup: fractional position → expanded atom index.
            struct ExpandedBond {
                qlonglong atom1, atom2;
                Vector3I img;
                int sciOrd;
            };
            std::vector<ExpandedBond> expandedBonds;

            for(size_t ri = 0; ri < abt.rowCount; ri++) {
                progress.setValueIntermittent(++rowsProcessed);
                qlonglong i = 0, j = 0;
                int ord = 0;
                if(a1Col >= 0) { int64_t tmp = 0; parseInt64(colVal(abt, ri, a1Col), tmp); i = tmp; }
                if(a2Col >= 0) { int64_t tmp = 0; parseInt64(colVal(abt, ri, a2Col), tmp); j = tmp; }
                if(ordCol >= 0) { parseInt32(colVal(abt, ri, ordCol), ord); }

                if(i < 0 || i >= numAsym || j < 0 || j >= numAsym) continue;

                for(size_t opIdx = 0; opIdx < allOps.size(); opIdx++) {
                    const gemmi::Op& op = allOps[opIdx];

                    // Find expanded index for atom i under this operation.
                    auto [nxi, nyi, nzi] = op.apply_to_xyz(asymFrac[i]);
                    nxi -= std::floor(nxi); nyi -= std::floor(nyi); nzi -= std::floor(nzi);
                    qlonglong ei = -1;
                    for(qlonglong k = 0; k < numExpanded; k++) {
                        if(std::abs(nxi - expandedAtoms[k].frac[0]) < kEps &&
                           std::abs(nyi - expandedAtoms[k].frac[1]) < kEps &&
                           std::abs(nzi - expandedAtoms[k].frac[2]) < kEps) { ei = k; break; }
                    }
                    if(ei < 0) continue;

                    // Apply same op to frac_j, capture floor before wrapping.
                    auto [nxj_raw, nyj_raw, nzj_raw] = op.apply_to_xyz(asymFrac[j]);
                    int di = static_cast<int>(std::floor(nxj_raw));
                    int dj = static_cast<int>(std::floor(nyj_raw));
                    int dk = static_cast<int>(std::floor(nzj_raw));
                    double nxj = nxj_raw - di, nyj = nyj_raw - dj, nzj = nzj_raw - dk;

                    qlonglong ej = -1;
                    for(qlonglong k = 0; k < numExpanded; k++) {
                        if(std::abs(nxj - expandedAtoms[k].frac[0]) < kEps &&
                           std::abs(nyj - expandedAtoms[k].frac[1]) < kEps &&
                           std::abs(nzj - expandedAtoms[k].frac[2]) < kEps) { ej = k; break; }
                    }
                    if(ej < 0) continue;

                    // Deduplicate bonds (avoid self-bonds and duplicates).
                    if(ei == ej) continue;
                    bool dup = false;
                    for(const ExpandedBond& eb : expandedBonds) {
                        if(eb.atom1 == ei && eb.atom2 == ej && eb.img == Vector3I(di, dj, dk)) { dup = true; break; }
                    }
                    if(!dup)
                        expandedBonds.push_back({ei, ej, Vector3I(di, dj, dk), ord});
                }
            }
            const qlonglong numBonds = static_cast<qlonglong>(expandedBonds.size());
            setBondCount(numBonds);

            Property* topoProp   = bonds()->createProperty(Bonds::TopologyProperty);
            Property* bTypeProp  = bonds()->createProperty(Bonds::TypeProperty);
            Property* bOrderProp = bonds()->createProperty(Bonds::OrderProperty);
            Property* bImgProp   = bonds()->createProperty(Bonds::PeriodicImageProperty);

            BufferWriteAccess<ParticleIndexPair, access_mode::discard_write> topoAcc(topoProp);
            BufferWriteAccess<int32_t, access_mode::discard_write> bTypeAcc(bTypeProp);
            BufferWriteAccess<GraphicsFloatType, access_mode::discard_write> bOrderAcc(bOrderProp);
            BufferWriteAccess<Vector3I, access_mode::discard_write> bImgAcc(bImgProp);

            for(qlonglong bi = 0; bi < numBonds; bi++) {
                const ExpandedBond& eb = expandedBonds[bi];
                topoAcc[bi][0] = eb.atom1;
                topoAcc[bi][1] = eb.atom2;
                auto bi_info = sciBondInfo(eb.sciOrd);
                addNumericType(Bonds::OOClass(), bTypeProp, bi_info.typeId, bi_info.name);
                bTypeAcc[bi] = bi_info.typeId;
                bOrderAcc[bi] = bi_info.order;
                bImgAcc[bi] = eb.img;
            }
            topoAcc.reset();
            bTypeAcc.reset();
            bOrderAcc.reset();
            bImgAcc.reset();
        }
        else {
            setBondCount(0);
        }
    }
    else {
        // --- Non-periodic or periodic with explicit Atom (Cartesian) table ---
        const RawTable& atomTbl = tables.at("Atom");
        const qlonglong numAtoms = static_cast<qlonglong>(atomTbl.rowCount);

        int anCol       = atomTbl.col("AtomicNumber");
        int coordCol    = atomTbl.col("Coordinates");
        int nameCol     = atomTbl.col("Name");
        int pointCol    = atomTbl.col("Point");
        int beadCol     = atomTbl.col("Bead");    // mesoscale only
        int ffTypeCol   = atomTbl.col("FFAtomType");
        int ffChargeCol = atomTbl.col("FFCharge");
        int massCol     = atomTbl.col("Mass");

        setParticleCount(numAtoms);

        BufferWriteAccess<Point3, access_mode::discard_write> posAccess = particles()->createProperty(Particles::PositionProperty);
        Property* typeProperty = particles()->createProperty(Particles::TypeProperty);
        BufferWriteAccess<int32_t, access_mode::discard_write> typeAccess(typeProperty);
        BufferWriteAccess<int64_t, access_mode::discard_write> idAccess = particles()->createProperty(Particles::IdentifierProperty);

        Property* atomNameProp = particles()->createProperty(QStringLiteral("Atom Name"), DataBuffer::String);
        BufferWriteAccess<QString, access_mode::discard_write> atomNameAccess(atomNameProp);
        Property* ffTypeProp   = particles()->createProperty(QStringLiteral("Force-Field Type"), DataBuffer::Int32);
        ffTypeProp->setTitle(tr("Force-field atom types"));
        BufferWriteAccess<int32_t, access_mode::discard_write> ffTypeAccess(ffTypeProp);
        addNumericType(Particles::OOClass(), ffTypeProp, 0, QStringLiteral("None"));  // reserve 0 for "None" type
        Property* chargeProp   = particles()->createProperty(Particles::ChargeProperty);
        BufferWriteAccess<FloatType, access_mode::discard_write> chargeAccess(chargeProp);
        Property* massProp     = particles()->createProperty(Particles::MassProperty);
        BufferWriteAccess<FloatType, access_mode::discard_write> massAccess(massProp);

        bool hasFFType = false, hasCharge = false, hasMass = false;

        for(qlonglong i = 0; i < numAtoms; i++) {
            progress.setValueIntermittent(++rowsProcessed);
            const size_t ri = static_cast<size_t>(i);

            // Position.
            Point3 pos(0, 0, 0);
            if(coordCol >= 0) {
                double x, y, z;
                if(parseVector3(colVal(atomTbl, ri, coordCol), x, y, z))
                    pos = Point3((FloatType)x, (FloatType)y, (FloatType)z);
            }
            posAccess[i] = pos;

            // Identifier: use Point column only when its default is 0 (unique-ID mode).
            // A default of 1 means the column is a boolean inclusion flag, not an atom ID.
            int64_t id = i + 1;
            if(pointCol >= 0 && atomTbl.columns[pointCol].defaultValue != "1") {
                int pt = 0;
                if(parseInt32(colVal(atomTbl, ri, pointCol), pt)) id = pt;
            }
            idAccess[i] = id;

            // Particle type.
            if(isMesoscale && beadCol >= 0) {
                // Mesoscale: type determined by bead index.
                int beadIdx = 0;
                parseInt32(colVal(atomTbl, ri, beadCol), beadIdx);
                typeAccess[i] = assignBeadType(typeProperty, beadIdx);
            }
            else {
                // Atomistic: type determined by atomic number.
                int atomicNumber = 0;
                if(anCol >= 0) {
                    parseInt32(colVal(atomTbl, ri, anCol), atomicNumber);
                }
                if(atomicNumber >= 0 && atomicNumber < (int)ParticleType::ChemicalElement::NUMBER_OF_PREDEFINED_CHEMICAL_TYPES)
                    addNumericType(Particles::OOClass(), typeProperty, atomicNumber, ParticleType::getChemicalElementSymbol(static_cast<ParticleType::ChemicalElement>(atomicNumber)));
                typeAccess[i] = atomicNumber;
            }

            // Atom name.
            if(nameCol >= 0) {
                std::string_view sv = colVal(atomTbl, ri, nameCol);
                atomNameAccess[i] = QString::fromUtf8(QByteArrayView(sv.data(), (qsizetype)sv.size()));
            }
            else
                atomNameAccess[i].clear();

            // Force-field atom type.
            int32_t ffTypeId = 0;
            if(ffTypeCol >= 0) {
                std::string_view fft = colVal(atomTbl, ri, ffTypeCol);
                if(!fft.empty()) {
                    hasFFType = true;
                    ffTypeId = addNamedType(Particles::OOClass(), ffTypeProp,
                                            QLatin1StringView(fft.data(), (qsizetype)fft.size()))->numericId();
                }
            }
            ffTypeAccess[i] = ffTypeId;

            // Charge (FFCharge).
            FloatType charge = 0;
            if(ffChargeCol >= 0) {
                double q = 0;
                if(parseFloatType(colVal(atomTbl, ri, ffChargeCol), q) && q != 0.0) {
                    hasCharge = true;
                    charge = (FloatType)q;
                }
            }
            chargeAccess[i] = charge;

            // Mass.
            FloatType mass = 0;
            if(massCol >= 0) {
                double m = 0;
                if(parseFloatType(colVal(atomTbl, ri, massCol), m) && m != 0.0) {
                    hasMass = true;
                    mass = (FloatType)m;
                }
            }
            massAccess[i] = mass;
        }

        posAccess.reset();
        typeAccess.reset();
        idAccess.reset();
        atomNameAccess.reset();
        ffTypeAccess.reset();
        chargeAccess.reset();
        massAccess.reset();

        if(!isMesoscale) typeProperty->sortElementTypesById();
        ffTypeProp->sortElementTypesByName();

        if(!hasFFType) { particles()->removeProperty(ffTypeProp); ffTypeProp = nullptr; }
        if(!hasCharge) { particles()->removeProperty(chargeProp); chargeProp = nullptr; }
        if(!hasMass)   { particles()->removeProperty(massProp);   massProp   = nullptr; }

        // Bonds for non-periodic or periodic-explicit mode.
        if(hasBond) {
            const RawTable& bondTbl = tables.at("Bond");
            int a1Col  = bondTbl.col("Atom1");
            int a2Col  = bondTbl.col("Atom2");
            int ordCol = bondTbl.col("Order");
            int abCol  = bondTbl.col("AsymmetricBond");
            int co2Col = bondTbl.col("CellOffset2");

            const qlonglong numBonds = static_cast<qlonglong>(bondTbl.rowCount);
            setBondCount(numBonds);

            Property* topoProp   = bonds()->createProperty(Bonds::TopologyProperty);
            Property* bTypeProp  = bonds()->createProperty(Bonds::TypeProperty);
            Property* bOrderProp = bonds()->createProperty(Bonds::OrderProperty);

            BufferWriteAccess<ParticleIndexPair, access_mode::discard_write> topoAcc(topoProp);
            BufferWriteAccess<int32_t, access_mode::discard_write> bTypeAcc(bTypeProp);
            BufferWriteAccess<GraphicsFloatType, access_mode::discard_write> bOrderAcc(bOrderProp);

            // Periodic-image property (only for periodic-explicit mode).
            Property* bImgProp = nullptr;
            BufferWriteAccess<Vector3I, access_mode::discard_write> bImgAcc;
            if(isPeriodic && co2Col >= 0) {
                bImgProp = bonds()->createProperty(Bonds::PeriodicImageProperty);
                bImgAcc = BufferWriteAccess<Vector3I, access_mode::discard_write>(bImgProp);
            }

            for(qlonglong bi = 0; bi < numBonds; bi++) {
                progress.setValueIntermittent(++rowsProcessed);
                const size_t bri = static_cast<size_t>(bi);

                qlonglong a1 = 0, a2 = 0;
                if(a1Col >= 0) { int64_t tmp = 0; parseInt64(colVal(bondTbl, bri, a1Col), tmp); a1 = tmp; }
                if(a2Col >= 0) { int64_t tmp = 0; parseInt64(colVal(bondTbl, bri, a2Col), tmp); a2 = tmp; }
                topoAcc[bi][0] = a1;
                topoAcc[bi][1] = a2;

                // Bond order: from Order column (non-periodic) or AsymmetricBond lookup (periodic).
                int sciOrd = 0;
                if(ordCol >= 0) {
                    parseInt32(colVal(bondTbl, bri, ordCol), sciOrd);
                }
                else if(abCol >= 0 && !asymBondOrders.empty()) {
                    int abIdx = 0;
                    parseInt32(colVal(bondTbl, bri, abCol), abIdx);
                    if(abIdx >= 0 && abIdx < static_cast<int>(asymBondOrders.size()))
                        sciOrd = asymBondOrders[abIdx];
                }

                auto bi_info = sciBondInfo(sciOrd);
                addNumericType(Bonds::OOClass(), bTypeProp, bi_info.typeId, bi_info.name);
                bTypeAcc[bi] = bi_info.typeId;
                bOrderAcc[bi] = bi_info.order;

                if(bImgProp) {
                    Vector3I img = Vector3I::Zero();
                    parseVector3i(colVal(bondTbl, bri, co2Col), img);
                    bImgAcc[bi] = img;
                }
            }

            topoAcc.reset();
            bTypeAcc.reset();
            bOrderAcc.reset();
            if(bImgProp) bImgAcc.reset();
        }
        else setBondCount(0);
    }

    // Process Properties table (non-periodic, molecular charge / spin).
    if(hasProperties) {
        const RawTable& propTbl = tables.at("Properties");
        if(propTbl.rowCount > 0) {
            int chargeCol = propTbl.col("Charge");
            int spinCol   = propTbl.col("SpinMultiplicity");

            if(chargeCol >= 0) {
                int q = 0;
                parseInt32(propTbl.cell(0, chargeCol), q);
                state().setAttribute(QStringLiteral("SCI.MolecularCharge"), QVariant::fromValue(q), pipelineNode());
            }
            if(spinCol >= 0) {
                int s = 0;
                parseInt32(propTbl.cell(0, spinCol), s);
                state().setAttribute(QStringLiteral("SCI.SpinMultiplicity"), QVariant::fromValue(s), pipelineNode());
            }
        }
    }

    // Release the raw data tables before post-processing.
    tables.clear();

    progress.endSubSteps();

    // Generate bounding box if no Cell table was present.
    if(!hasCell)
        generateBoundingBox();

    // Finalize.
    qlonglong numAtomsFinal = particles()->elementCount();
    qlonglong numBondsFinal = hasBonds() ? bonds()->elementCount() : 0;
    state().setStatus(tr("%1 particles, %2 bonds").arg(numAtomsFinal).arg(numBondsFinal));

    // Sort particles by ID if requested.
    if(_sortParticles)
        particles()->sortById();

    ParticleImporter::FrameLoader::loadFile();
}

}  // namespace Ovito
