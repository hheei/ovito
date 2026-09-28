////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//
////////////////////////////////////////////////////////////////////////////////////////

#include <QTest>
#include <ovito/stdobj/simcell/SimulationCell.h>

using namespace Ovito;

// Use double precision for all tests.
using Cell = SimulationCellDataT<double>;
using Vec3d = Vector_3<double>;
using Pt3d = Point_3<double>;
using Box3d = Box_3<double>;
static constexpr double EPS = 1e-12;

static bool near(double a, double b, double eps = EPS) { return std::abs(a - b) <= eps; }
static bool near(const Pt3d& a, const Pt3d& b, double eps = EPS) {
    return near(a.x(), b.x(), eps) && near(a.y(), b.y(), eps) && near(a.z(), b.z(), eps);
}
static bool near(const Vec3d& a, const Vec3d& b, double eps = EPS) {
    return near(a.x(), b.x(), eps) && near(a.y(), b.y(), eps) && near(a.z(), b.z(), eps);
}

class SimulationCellTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------

    void default_construction() {
        Cell cell;
        // Default cell is zero (degenerate).
        QVERIFY(cell.isDegenerate());
        QVERIFY(!cell.hasPbc());
        QVERIFY(!cell.is2D());
    }

    void box_construction() {
        Box3d box(Pt3d(0,0,0), Pt3d(10,10,10));
        Cell cell(box);
        QVERIFY(cell.isAxisAligned());
        QVERIFY(near(cell.volume3D(), 1000.0));
        QVERIFY(near(cell.cellVector1().length(), 10.0));
    }

    void pbc_flags() {
        Box3d box(Pt3d(0,0,0), Pt3d(5,5,5));
        Cell cell(box);
        cell.setPbcFlags({true, true, false});
        QVERIFY(cell.hasPbc(0));
        QVERIFY(cell.hasPbc(1));
        QVERIFY(!cell.hasPbc(2));
        QVERIFY(cell.hasPbc());
    }

    // -----------------------------------------------------------------------
    // Coordinate conversion
    // -----------------------------------------------------------------------

    void reduced_to_absolute_roundtrip() {
        Box3d box(Pt3d(0,0,0), Pt3d(8,6,4));
        Cell cell(box);

        Pt3d reduced(0.5, 0.5, 0.5);
        Pt3d absolute = cell.reducedToAbsolute(reduced);
        QVERIFY(near(absolute, Pt3d(4.0, 3.0, 2.0)));

        Pt3d back = cell.absoluteToReduced(absolute);
        QVERIFY(near(back, reduced, 1e-10));
    }

    void reduced_to_absolute_origin() {
        Box3d box(Pt3d(1,2,3), Pt3d(11,12,13));
        Cell cell(box);

        Pt3d reduced_origin(0.0, 0.0, 0.0);
        Pt3d absolute = cell.reducedToAbsolute(reduced_origin);
        QVERIFY(near(absolute, Pt3d(1.0, 2.0, 3.0)));
    }

    // -----------------------------------------------------------------------
    // PBC wrapping — orthogonal cell
    // -----------------------------------------------------------------------

    void wrap_point_orthogonal_inside() {
        Box3d box(Pt3d(0,0,0), Pt3d(10,10,10));
        Cell cell(box);
        cell.setPbcFlags({true, true, true});

        // Point inside should not be changed.
        Pt3d p(5.0, 5.0, 5.0);
        Pt3d wrapped = cell.wrapPoint(p);
        QVERIFY(near(wrapped, p));
    }

    void wrap_point_orthogonal_outside_positive() {
        Box3d box(Pt3d(0,0,0), Pt3d(10,10,10));
        Cell cell(box);
        cell.setPbcFlags({true, true, true});

        Pt3d p(11.0, 0.0, 0.0);   // one cell length past the +x boundary
        Pt3d wrapped = cell.wrapPoint(p);
        QVERIFY(near(wrapped, Pt3d(1.0, 0.0, 0.0), 1e-10));
    }

    void wrap_point_orthogonal_outside_negative() {
        Box3d box(Pt3d(0,0,0), Pt3d(10,10,10));
        Cell cell(box);
        cell.setPbcFlags({true, true, true});

        Pt3d p(-1.0, 0.0, 0.0);
        Pt3d wrapped = cell.wrapPoint(p);
        QVERIFY(near(wrapped, Pt3d(9.0, 0.0, 0.0), 1e-10));
    }

    void wrap_point_no_pbc_unchanged() {
        Box3d box(Pt3d(0,0,0), Pt3d(10,10,10));
        Cell cell(box);
        // No PBC — wrapping should leave the point unchanged.

        Pt3d p(15.0, -3.0, 100.0);
        Pt3d wrapped = cell.wrapPoint(p);
        QVERIFY(near(wrapped, p));
    }

    void wrap_point_partial_pbc() {
        Box3d box(Pt3d(0,0,0), Pt3d(10,10,10));
        Cell cell(box);
        cell.setPbcFlags({true, false, false});

        // x: wrapped; y,z: not wrapped.
        Pt3d p(12.0, 15.0, -3.0);
        Pt3d wrapped = cell.wrapPoint(p);
        QVERIFY(near(wrapped.x(), 2.0, 1e-10));
        QVERIFY(near(wrapped.y(), 15.0));
        QVERIFY(near(wrapped.z(), -3.0));
    }

    // -----------------------------------------------------------------------
    // PBC wrapping — wrapVector (minimum image convention)
    // -----------------------------------------------------------------------

    void wrap_vector_orthogonal_short_vector() {
        Box3d box(Pt3d(0,0,0), Pt3d(10,10,10));
        Cell cell(box);
        cell.setPbcFlags({true, true, true});

        Vec3d v(1.0, 2.0, 3.0);  // well inside [-5, 5] range
        QVERIFY(near(cell.wrapVector(v), v));
    }

    void wrap_vector_orthogonal_long_vector() {
        Box3d box(Pt3d(0,0,0), Pt3d(10,10,10));
        Cell cell(box);
        cell.setPbcFlags({true, true, true});

        // 8 > 10/2 → minimum image should be 8 - 10 = -2.
        Vec3d v(8.0, 0.0, 0.0);
        Vec3d wrapped = cell.wrapVector(v);
        QVERIFY(near(wrapped, Vec3d(-2.0, 0.0, 0.0), 1e-10));
    }

};

QTEST_MAIN(SimulationCellTest)
#include "tst_simulation_cell.moc"
