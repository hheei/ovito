// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <QTest>
#include <ovito/core/Core.h>

using namespace Ovito;

// Use double throughout for test clarity.
using Vec3 = Vector_3<double>;
using Vec3I = Vector_3<int>;
using Mat3 = Matrix_3<double>;
using AffT = AffineTransformationT<double>;
using Box3d = Box_3<double>;
using Pt3 = Point_3<double>;
using Quat = QuaternionT<double>;
using Rot = RotationT<double>;

static constexpr double EPS = 1e-12;

/******************************************************************************
* Fuzzy comparison helpers.
******************************************************************************/
static bool near(double a, double b, double eps = EPS) { return std::abs(a - b) <= eps; }
static bool near(const Vec3& a, const Vec3& b, double eps = EPS) {
    return near(a.x(), b.x(), eps) && near(a.y(), b.y(), eps) && near(a.z(), b.z(), eps);
}
static bool near(const Pt3& a, const Pt3& b, double eps = EPS) {
    return near(a.x(), b.x(), eps) && near(a.y(), b.y(), eps) && near(a.z(), b.z(), eps);
}
static bool near(const Mat3& a, const Mat3& b, double eps = EPS) {
    for(int i = 0; i < 3; ++i)
        for(int j = 0; j < 3; ++j)
            if(!near(a(i,j), b(i,j), eps)) return false;
    return true;
}

class CoreMathTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    // -----------------------------------------------------------------------
    // Vector3
    // -----------------------------------------------------------------------

    void vector3_construction() {
        Vec3 z = Vec3::Zero();
        QVERIFY(z.x() == 0.0 && z.y() == 0.0 && z.z() == 0.0);

        Vec3 v(1.0, 2.0, 3.0);
        QCOMPARE(v[0], 1.0);
        QCOMPARE(v[1], 2.0);
        QCOMPARE(v[2], 3.0);
        QCOMPARE(v.x(), 1.0);
        QCOMPARE(v.y(), 2.0);
        QCOMPARE(v.z(), 3.0);
    }

    void vector3_arithmetic() {
        Vec3 a(1.0, 2.0, 3.0);
        Vec3 b(4.0, 5.0, 6.0);

        QVERIFY(near(a + b, Vec3(5.0, 7.0, 9.0)));
        QVERIFY(near(b - a, Vec3(3.0, 3.0, 3.0)));
        QVERIFY(near(a * 2.0, Vec3(2.0, 4.0, 6.0)));
        QVERIFY(near(2.0 * a, Vec3(2.0, 4.0, 6.0)));
        QVERIFY(near(a / 2.0, Vec3(0.5, 1.0, 1.5)));
        QVERIFY(near(-a, Vec3(-1.0, -2.0, -3.0)));
    }

    void vector3_dot_product() {
        Vec3 a(1.0, 0.0, 0.0);
        Vec3 b(0.0, 1.0, 0.0);
        QCOMPARE(a.dot(b), 0.0);
        QCOMPARE(a.dot(a), 1.0);

        Vec3 c(1.0, 2.0, 3.0);
        Vec3 d(4.0, 5.0, 6.0);
        QCOMPARE(c.dot(d), 1*4 + 2*5 + 3*6);  // = 32
    }

    void vector3_cross_product() {
        Vec3 x(1.0, 0.0, 0.0);
        Vec3 y(0.0, 1.0, 0.0);
        Vec3 z(0.0, 0.0, 1.0);

        QVERIFY(near(x.cross(y), z));
        QVERIFY(near(y.cross(z), x));
        QVERIFY(near(z.cross(x), y));
        QVERIFY(near(x.cross(x), Vec3::Zero()));
    }

    void vector3_length() {
        Vec3 v(3.0, 4.0, 0.0);
        QCOMPARE(v.length(), 5.0);
        QCOMPARE(v.squaredLength(), 25.0);

        Vec3 unit = v.normalized();
        QVERIFY(near(unit.length(), 1.0));
        QVERIFY(near(unit, Vec3(0.6, 0.8, 0.0)));
    }

    // -----------------------------------------------------------------------
    // Matrix3
    // -----------------------------------------------------------------------

    void matrix3_identity() {
        Mat3 I = Mat3::Identity();
        Vec3 v(1.0, 2.0, 3.0);
        QVERIFY(near(I * v, v));
        QVERIFY(near(I.determinant(), 1.0));
    }

    void matrix3_zero() {
        Mat3 Z = Mat3::Zero();
        Vec3 v(1.0, 2.0, 3.0);
        QVERIFY(near(Z * v, Vec3::Zero()));
    }

    void matrix3_multiplication() {
        // Rotation by 90° around Z × rotation by 90° around Z = rotation by 180° around Z.
        Mat3 rz90 = Mat3::rotation(Rot(Vec3(0,0,1), M_PI/2.0));
        Mat3 rz180 = rz90 * rz90;
        Vec3 x(1.0, 0.0, 0.0);
        QVERIFY(near(rz180 * x, Vec3(-1.0, 0.0, 0.0), 1e-10));
    }

    void matrix3_determinant() {
        Mat3 I = Mat3::Identity();
        QVERIFY(near(I.determinant(), 1.0));

        Mat3 m(Vec3(1,0,0), Vec3(0,2,0), Vec3(0,0,3));
        QVERIFY(near(m.determinant(), 6.0));
    }

    void matrix3_inverse() {
        Mat3 m(Vec3(1,2,0), Vec3(3,4,0), Vec3(0,0,1));
        Mat3 inv = m.inverse();
        Mat3 product = m * inv;
        QVERIFY(near(product, Mat3::Identity(), 1e-10));
    }

    void matrix3_transpose() {
        Mat3 m(Vec3(1,2,3), Vec3(4,5,6), Vec3(7,8,9));
        Mat3 t = m.transposed();
        for(int i = 0; i < 3; ++i)
            for(int j = 0; j < 3; ++j)
                QCOMPARE(m(i,j), t(j,i));
    }

    void matrix3_rotation() {
        // Rotate x-axis by 90° around z — should give y-axis.
        Mat3 r = Mat3::rotation(Rot(Vec3(0,0,1), M_PI/2.0));
        QVERIFY(near(r * Vec3(1, 0, 0), Vec3(0, 1, 0), 1e-10));
    }

    // -----------------------------------------------------------------------
    // Box3
    // -----------------------------------------------------------------------

    void box3_construction() {
        Box3d b(Pt3(0,0,0), Pt3(1,1,1));
        QVERIFY(near(b.sizeX(), 1.0));
        QVERIFY(near(b.sizeY(), 1.0));
        QVERIFY(near(b.sizeZ(), 1.0));
        QVERIFY(!b.isEmpty());
    }

    void box3_contains_point() {
        Box3d b(Pt3(0,0,0), Pt3(2,2,2));
        QVERIFY(b.contains(Pt3(1,1,1)));
        QVERIFY(!b.contains(Pt3(3,1,1)));
    }

    void box3_extend() {
        Box3d b;
        b.addPoint(Pt3(1,2,3));
        b.addPoint(Pt3(-1,-2,-3));
        QVERIFY(near(b.minc.x(), -1.0));
        QVERIFY(near(b.maxc.x(),  1.0));
        QVERIFY(near(b.minc.y(), -2.0));
        QVERIFY(near(b.maxc.y(),  2.0));
    }

    // -----------------------------------------------------------------------
    // AffineTransformation
    // -----------------------------------------------------------------------

    void affine_identity() {
        AffT I = AffT::Identity();
        Pt3 p(1.0, 2.0, 3.0);
        QVERIFY(near(I * p, p));

        Vec3 v(1.0, 2.0, 3.0);
        QVERIFY(near(I * v, v));
    }

    void affine_translation() {
        Vec3 t(5.0, 0.0, 0.0);
        AffT T = AffT::translation(t);
        Pt3 p(1.0, 0.0, 0.0);
        QVERIFY(near(T * p, Pt3(6.0, 0.0, 0.0)));

        // Vectors are not affected by translation.
        Vec3 v(1.0, 0.0, 0.0);
        QVERIFY(near(T * v, v));
    }

    void affine_inverse() {
        AffT T = AffT::translation(Vec3(3.0, 1.0, -2.0));
        AffT inv = T.inverse();
        AffT product = T * inv;

        // T * inv should be the identity.
        Pt3 p(7.0, 8.0, 9.0);
        QVERIFY(near(product * p, p, 1e-10));
    }

    // -----------------------------------------------------------------------
    // Quaternion
    // -----------------------------------------------------------------------

    void quaternion_identity() {
        Quat q = Quat::Identity();
        Vec3 v(1.0, 2.0, 3.0);
        Mat3 m = Mat3::rotation(q);
        QVERIFY(near(m * v, v, 1e-10));
    }

    void quaternion_rotation_180() {
        // 180° rotation around z-axis should flip x → -x, y → -y.
        Quat q = static_cast<Quat>(Rot(Vec3(0,0,1), M_PI));
        Mat3 m = Mat3::rotation(q);
        QVERIFY(near(m * Vec3(1, 0, 0), Vec3(-1.0, 0.0, 0.0), 1e-10));
        QVERIFY(near(m * Vec3(0, 1, 0), Vec3( 0.0,-1.0, 0.0), 1e-10));
    }

    void quaternion_composition() {
        // Two 90° rotations around z compose to one 180° rotation.
        Quat q90  = static_cast<Quat>(Rot(Vec3(0,0,1), M_PI/2.0));
        Quat q180 = q90 * q90;
        Mat3 m90  = Mat3::rotation(q90);
        Mat3 m180 = Mat3::rotation(q180);

        Vec3 x(1.0, 0.0, 0.0);
        QVERIFY(near(m180 * x, Vec3(-1.0, 0.0, 0.0), 1e-10));
        QVERIFY(near(m90 * (m90 * x), m180 * x, 1e-10));
    }
};

QTEST_MAIN(CoreMathTest)
#include "tst_core_math.moc"
