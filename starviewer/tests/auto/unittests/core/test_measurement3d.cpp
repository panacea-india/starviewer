/*@
    "name": "test_Measurement3D",
    "requirements": []
 */

#include "autotest.h"
#include "measurement3d.h"

#include <cmath>

using namespace udg;

class test_Measurement3D : public QObject {

    Q_OBJECT

private slots:
    void distance_ShouldGiveTheLengthOfTheSegment_data();
    void distance_ShouldGiveTheLengthOfTheSegment();
    void distance_ShouldNotDependOnTheOrderOfThePoints();

    void angleDegrees_ShouldGiveTheAngleAtTheVertex_data();
    void angleDegrees_ShouldGiveTheAngleAtTheVertex();
    void angleDegrees_ShouldReturnMinusOne_WhenASegmentHasNoLength();

    void pathLength_ShouldAddTheSegments();
    void pathLength_ShouldBeZero_ForOneOrNoPoint();
};

void test_Measurement3D::distance_ShouldGiveTheLengthOfTheSegment_data()
{
    QTest::addColumn<double>("ax");
    QTest::addColumn<double>("ay");
    QTest::addColumn<double>("az");
    QTest::addColumn<double>("bx");
    QTest::addColumn<double>("by");
    QTest::addColumn<double>("bz");
    QTest::addColumn<double>("expected");

    QTest::newRow("the same point") << 1.0 << 2.0 << 3.0 << 1.0 << 2.0 << 3.0 << 0.0;
    QTest::newRow("along x") << 0.0 << 0.0 << 0.0 << 100.0 << 0.0 << 0.0 << 100.0;
    QTest::newRow("a 3, 4, 5 triangle in the plane") << 0.0 << 0.0 << 0.0 << 3.0 << 4.0 << 0.0 << 5.0;
    QTest::newRow("between slices") << -49.2 << 0.4 << 50.0 << -49.2 << 0.4 << 297.5 << 247.5;
    QTest::newRow("the diagonal of a box") << 0.0 << 0.0 << 0.0 << 2.0 << 3.0 << 6.0 << 7.0;
    QTest::newRow("with negative coordinates") << -40.0 << -20.0 << 125.0 << 40.0 << -20.0 << 125.0 << 80.0;
}

void test_Measurement3D::distance_ShouldGiveTheLengthOfTheSegment()
{
    QFETCH(double, ax);
    QFETCH(double, ay);
    QFETCH(double, az);
    QFETCH(double, bx);
    QFETCH(double, by);
    QFETCH(double, bz);
    QFETCH(double, expected);

    double a[3] = {ax, ay, az};
    double b[3] = {bx, by, bz};

    QVERIFY(std::abs(Measurement3D::distance(a, b) - expected) < 1e-9);
}

void test_Measurement3D::distance_ShouldNotDependOnTheOrderOfThePoints()
{
    double a[3] = {1.5, -2.5, 30.0};
    double b[3] = {-7.0, 8.0, 12.0};

    QCOMPARE(Measurement3D::distance(a, b), Measurement3D::distance(b, a));
}

void test_Measurement3D::angleDegrees_ShouldGiveTheAngleAtTheVertex_data()
{
    QTest::addColumn<double>("ax");
    QTest::addColumn<double>("ay");
    QTest::addColumn<double>("az");
    QTest::addColumn<double>("bx");
    QTest::addColumn<double>("by");
    QTest::addColumn<double>("bz");
    QTest::addColumn<double>("expected");

    // The vertex is at the origin
    QTest::newRow("a right angle") << 1.0 << 0.0 << 0.0 << 0.0 << 1.0 << 0.0 << 90.0;
    QTest::newRow("a right angle out of the plane") << 0.0 << 0.0 << 5.0 << 7.0 << 0.0 << 0.0 << 90.0;
    QTest::newRow("a straight line") << 1.0 << 0.0 << 0.0 << -3.0 << 0.0 << 0.0 << 180.0;
    QTest::newRow("the same direction") << 1.0 << 2.0 << 3.0 << 2.0 << 4.0 << 6.0 << 0.0;
    QTest::newRow("45 degrees") << 1.0 << 0.0 << 0.0 << 1.0 << 1.0 << 0.0 << 45.0;
    QTest::newRow("60 degrees") << 1.0 << 0.0 << 0.0 << 0.5 << std::sqrt(3.0) / 2.0 << 0.0 << 60.0;
    QTest::newRow("120 degrees in 3D") << 1.0 << 0.0 << 0.0 << -0.5 << 0.0 << std::sqrt(3.0) / 2.0 << 120.0;
}

void test_Measurement3D::angleDegrees_ShouldGiveTheAngleAtTheVertex()
{
    QFETCH(double, ax);
    QFETCH(double, ay);
    QFETCH(double, az);
    QFETCH(double, bx);
    QFETCH(double, by);
    QFETCH(double, bz);
    QFETCH(double, expected);

    double vertex[3] = {0.0, 0.0, 0.0};
    double a[3] = {ax, ay, az};
    double b[3] = {bx, by, bz};

    QVERIFY2(std::abs(Measurement3D::angleDegrees(vertex, a, b) - expected) < 1e-6, qPrintable(QString::number(Measurement3D::angleDegrees(vertex, a, b))));
}

void test_Measurement3D::angleDegrees_ShouldReturnMinusOne_WhenASegmentHasNoLength()
{
    double vertex[3] = {1.0, 1.0, 1.0};
    double same[3] = {1.0, 1.0, 1.0};
    double other[3] = {2.0, 1.0, 1.0};

    QCOMPARE(Measurement3D::angleDegrees(vertex, same, other), -1.0);
    QCOMPARE(Measurement3D::angleDegrees(vertex, other, same), -1.0);
}

void test_Measurement3D::pathLength_ShouldAddTheSegments()
{
    // Along x for 3, then along y for 4, then along z for 12: 3 + 4 + 12
    double points[] = {0, 0, 0, 3, 0, 0, 3, 4, 0, 3, 4, 12};

    QVERIFY(std::abs(Measurement3D::pathLength(points, 4) - 19.0) < 1e-9);
}

void test_Measurement3D::pathLength_ShouldBeZero_ForOneOrNoPoint()
{
    double points[] = {5, 5, 5};

    QCOMPARE(Measurement3D::pathLength(points, 1), 0.0);
    QCOMPARE(Measurement3D::pathLength(points, 0), 0.0);
}

DECLARE_TEST(test_Measurement3D)

#include "test_measurement3d.moc"
