/*@
    "name": "test_RtRegionStatistics",
    "requirements": []
 */

#include "autotest.h"
#include "rtregionstatistics.h"

#include <cmath>

using namespace udg;

class test_RtRegionStatistics : public QObject {

    Q_OBJECT

private slots:
    void area_ShouldBeTheAreaOfThePolygon();
    void area_ShouldNotDependOnDirectionOrPlane();
    void compute_ShouldGiveTheVolumeOfASphere();
    void compute_ShouldGiveTheVolumeOfABox();
    void compute_ShouldUseTheMiddleDistanceBetweenPlanes();
    void compute_ShouldAddContoursOfTheSamePlane();
    void compute_ShouldLeaveOutOpenContoursAndGiveNoVolumeForOnePlane();
    void compute_ShouldBeEmptyForARegionWithoutContours();

private:
    static RtStructureSet::Contour circle(double radius, double z, int segments = 360, bool closed = true);
    static RtStructureSet::Contour rectangle(double w, double h, double z);
};

RtStructureSet::Contour test_RtRegionStatistics::circle(double radius, double z, int segments, bool closed)
{
    RtStructureSet::Contour contour;
    contour.closed = closed;

    for (int i = 0; i < segments; i++)
    {
        double a = 2.0 * 3.14159265358979 * i / segments;
        contour.points << std::array<double, 3>{{radius * std::cos(a), radius * std::sin(a), z}};
    }

    return contour;
}

RtStructureSet::Contour test_RtRegionStatistics::rectangle(double w, double h, double z)
{
    RtStructureSet::Contour contour;
    contour.closed = true;
    contour.points << std::array<double, 3>{{0, 0, z}} << std::array<double, 3>{{w, 0, z}} << std::array<double, 3>{{w, h, z}} << std::array<double, 3>{{0, h, z}};
    return contour;
}

void test_RtRegionStatistics::area_ShouldBeTheAreaOfThePolygon()
{
    QVERIFY(std::abs(RtRegionStatistics::area(rectangle(4, 5, 7).points) - 20.0) < 1e-9);
    QVERIFY(std::abs(RtRegionStatistics::area(circle(10, 0).points) - 314.159) < 0.1);
    QCOMPARE(RtRegionStatistics::area(QList<std::array<double, 3>>()), 0.0);
    QCOMPARE(RtRegionStatistics::area(rectangle(4, 5, 0).points.mid(0, 2)), 0.0);
}

void test_RtRegionStatistics::area_ShouldNotDependOnDirectionOrPlane()
{
    RtStructureSet::Contour box = rectangle(4, 5, 0);
    QList<std::array<double, 3>> reversed;

    for (int i = box.points.size() - 1; i >= 0; i--)
    {
        reversed << box.points[i];
    }

    QVERIFY(std::abs(RtRegionStatistics::area(reversed) - 20.0) < 1e-9);

    // The same rectangle standing in the plane x = 3 (y and z are its sides)
    QList<std::array<double, 3>> standing;

    for (const std::array<double, 3> &p : box.points)
    {
        standing << std::array<double, 3>{{3.0, p[0], p[1]}};
    }

    QVERIFY(std::abs(RtRegionStatistics::area(standing) - 20.0) < 1e-9);
}

void test_RtRegionStatistics::compute_ShouldGiveTheVolumeOfASphere()
{
    RtStructureSet::Roi roi;
    double radius = 20.0;

    for (double z = -20.0; z <= 20.0 + 1e-9; z += 2.5)
    {
        double r = std::sqrt(std::max(0.0, radius * radius - z * z));

        if (r > 0.5)
        {
            roi.contours << circle(r, z);
        }
    }

    RtRegionStatistics::Result result = RtRegionStatistics::compute(roi);
    double expected = 4.0 / 3.0 * 3.14159265358979 * radius * radius * radius / 1000.0;

    QCOMPARE(result.planes, roi.contours.size());
    QVERIFY(std::abs(result.planeSpacingMm - 2.5) < 1e-6);
    QVERIFY2(std::abs(result.volumeMl - expected) / expected < 0.03, qPrintable(QString("%1 mL, expected %2").arg(result.volumeMl).arg(expected)));
}

void test_RtRegionStatistics::compute_ShouldGiveTheVolumeOfABox()
{
    RtStructureSet::Roi roi;

    for (int i = 0; i < 5; i++)
    {
        roi.contours << rectangle(10, 20, i * 3.0);
    }

    // 5 planes 3 mm apart, each of 200 mm2: 5 x 200 x 3 mm3 = 3 mL
    RtRegionStatistics::Result result = RtRegionStatistics::compute(roi);
    QVERIFY(std::abs(result.volumeMl - 3.0) < 1e-9);
    QCOMPARE(result.contours, 5);
}

void test_RtRegionStatistics::compute_ShouldUseTheMiddleDistanceBetweenPlanes()
{
    RtStructureSet::Roi roi;

    // Planes at 0, 3, 6, 12 (one plane left out): the middle distance is 3
    for (double z : {0.0, 3.0, 6.0, 12.0})
    {
        roi.contours << rectangle(10, 10, z);
    }

    QVERIFY(std::abs(RtRegionStatistics::compute(roi).planeSpacingMm - 3.0) < 1e-9);
}

void test_RtRegionStatistics::compute_ShouldAddContoursOfTheSamePlane()
{
    RtStructureSet::Roi roi;

    // Two separate pieces on each of two planes
    for (double z : {0.0, 2.0})
    {
        roi.contours << rectangle(10, 10, z) << rectangle(5, 4, z);
    }

    RtRegionStatistics::Result result = RtRegionStatistics::compute(roi);
    QCOMPARE(result.planes, 2);
    QCOMPARE(result.contours, 4);
    QVERIFY(std::abs(result.volumeMl - 2 * 120.0 * 2.0 / 1000.0) < 1e-9);
}

void test_RtRegionStatistics::compute_ShouldLeaveOutOpenContoursAndGiveNoVolumeForOnePlane()
{
    RtStructureSet::Roi roi;
    roi.contours << circle(10, 0, 36, false);

    RtRegionStatistics::Result open = RtRegionStatistics::compute(roi);
    QCOMPARE(open.contours, 0);
    QCOMPARE(open.planes, 0);
    QCOMPARE(open.volumeMl, 0.0);

    roi.contours << circle(10, 0);
    RtRegionStatistics::Result single = RtRegionStatistics::compute(roi);
    QCOMPARE(single.contours, 1);
    QCOMPARE(single.planes, 1);
    QCOMPARE(single.volumeMl, 0.0);
}

void test_RtRegionStatistics::compute_ShouldBeEmptyForARegionWithoutContours()
{
    RtRegionStatistics::Result result = RtRegionStatistics::compute(RtStructureSet::Roi());

    QCOMPARE(result.contours, 0);
    QCOMPARE(result.planes, 0);
    QCOMPARE(result.volumeMl, 0.0);
}

DECLARE_TEST(test_RtRegionStatistics)

#include "test_rtregionstatistics.moc"
