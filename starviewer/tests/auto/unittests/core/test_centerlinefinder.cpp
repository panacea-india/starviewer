/*@
    "name": "test_CenterlineFinder",
    "requirements": []
 */

#include "autotest.h"
#include "centerlinefinder.h"

#include <cmath>

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

using namespace udg;

class test_CenterlineFinder : public QObject {

    Q_OBJECT

private slots:
    void find_ShouldFollowAWindingTube();
    void find_ShouldStartAndEndAtTheGivenPoints();
    void find_ShouldMovePointsThatAreJustOutside();
    void find_ShouldFailForPointsOutsideOrNotConnected();
    void find_ShouldFailForInvalidInput();
    void find_ShouldWorkWithAnExtentThatDoesNotStartAtZero();
    void rangeAround_ShouldUseTheMedianAroundThePoint();
    void findAutomatically_ShouldFindThePathFromPointsAtTheEnds();
    void findAutomatically_ShouldFailWithAMessage();

private:
    /// 48 x 48 x 30 voxels of 1 x 1 x 2 mm, origin (5, -3, 10). A tube of radius 3 mm and value 300 along y = 24 + 8 sin(...) (in voxels) at the slice 15, and a separate blob of 300 in a corner
    vtkSmartPointer<vtkImageData> makeVolume(int extentShift = 0);
    static void centerOf(double x, double *y);
    static double distanceToTruth(const double point[3]);
    static void world(double x, double y, double z, double *p);
};

void test_CenterlineFinder::centerOf(double x, double *y)
{
    *y = 24.0 + 8.0 * std::sin((x - 6.0) / 36.0 * 2.0 * 3.14159265358979);
}

void test_CenterlineFinder::world(double x, double y, double z, double *p)
{
    p[0] = 5.0 + x;
    p[1] = -3.0 + y;
    p[2] = 10.0 + z;
}

double test_CenterlineFinder::distanceToTruth(const double point[3])
{
    double best = 1e9;

    for (double x = 6.0; x <= 42.0; x += 0.05)
    {
        double y;
        centerOf(x, &y);
        double p[3];
        world(x, y, 15.0, p);
        best = std::min(best, std::sqrt(std::pow(p[0] - point[0], 2) + std::pow(p[1] - point[1], 2) + std::pow(p[2] - point[2], 2)));
    }

    return best;
}

vtkSmartPointer<vtkImageData> test_CenterlineFinder::makeVolume(int extentShift)
{
    vtkSmartPointer<vtkImageData> data = vtkSmartPointer<vtkImageData>::New();
    data->SetExtent(extentShift, extentShift + 47, extentShift, extentShift + 47, extentShift, extentShift + 29);
    data->SetSpacing(1.0, 1.0, 1.0);
    data->SetOrigin(5.0, -3.0, 10.0);
    data->AllocateScalars(VTK_SHORT, 1);

    for (int z = 0; z < 30; z++)
    {
        for (int y = 0; y < 48; y++)
        {
            for (int x = 0; x < 48; x++)
            {
                double value = 0.0;

                // The tube: distance in the plane of the slice 15 (a voxel is 1 mm)
                double cy;
                centerOf(x, &cy);

                if (x >= 6 && x <= 42 && std::pow(y - cy, 2) + std::pow(z - 15, 2) <= 9.0)
                {
                    value = 300.0;
                }

                // The blob that is not connected to the tube
                if (x >= 0 && x <= 3 && y >= 0 && y <= 3 && z >= 0 && z <= 3)
                {
                    value = 300.0;
                }

                data->SetScalarComponentFromDouble(x + extentShift, y + extentShift, z + extentShift, 0, value);
            }
        }
    }

    return data;
}

void test_CenterlineFinder::find_ShouldFollowAWindingTube()
{
    vtkSmartPointer<vtkImageData> data = makeVolume();
    double start[3], end[3];
    double y0, y1;
    centerOf(6.0, &y0);
    centerOf(42.0, &y1);
    world(6.0, y0, 15.0, start);
    world(42.0, y1, 15.0, end);

    CenterlineFinder::Result result = CenterlineFinder::find(data, start, end, 200.0, 400.0);

    QVERIFY2(result.success, qPrintable(result.message));
    QVERIFY(result.points.size() >= 3 * 10);
    QVERIFY(result.structureVoxels > 100);

    // The length of the true curve
    double expected = 0.0;

    for (double x = 6.0; x < 42.0; x += 0.01)
    {
        double a, b;
        centerOf(x, &a);
        centerOf(x + 0.01, &b);
        expected += std::sqrt(0.01 * 0.01 + (b - a) * (b - a));
    }

    QVERIFY2(std::abs(result.lengthMm - expected) / expected < 0.04, qPrintable(QString("%1 mm, expected %2").arg(result.lengthMm).arg(expected)));

    double worst = 0.0;

    for (int i = 0; i + 2 < result.points.size(); i += 3)
    {
        double p[3] = {result.points[i], result.points[i + 1], result.points[i + 2]};
        worst = std::max(worst, distanceToTruth(p));
    }

    QVERIFY2(worst < 1.6, qPrintable(QString("farthest point %1 mm from the center line").arg(worst)));
}

void test_CenterlineFinder::find_ShouldStartAndEndAtTheGivenPoints()
{
    vtkSmartPointer<vtkImageData> data = makeVolume();
    double start[3], end[3];
    double y0, y1;
    centerOf(10.0, &y0);
    centerOf(30.0, &y1);
    world(10.0, y0, 15.0, start);
    world(30.0, y1, 15.0, end);

    CenterlineFinder::Result result = CenterlineFinder::find(data, start, end, 200.0, 400.0);

    QVERIFY2(result.success, qPrintable(result.message));
    int n = result.points.size();
    QVERIFY(std::abs(result.points[0] - start[0]) < 1.0 && std::abs(result.points[1] - start[1]) < 1.0 && std::abs(result.points[2] - start[2]) < 2.0);
    QVERIFY(std::abs(result.points[n - 3] - end[0]) < 1.0 && std::abs(result.points[n - 2] - end[1]) < 1.0 && std::abs(result.points[n - 1] - end[2]) < 2.0);
}

void test_CenterlineFinder::find_ShouldMovePointsThatAreJustOutside()
{
    vtkSmartPointer<vtkImageData> data = makeVolume();
    double start[3], end[3];
    double y0, y1;
    centerOf(8.0, &y0);
    centerOf(40.0, &y1);
    // 8 mm above the center line (radius 3): 5 voxels outside the border, more than the 3 that are searched
    world(8.0, y0, 23.0, start);
    world(40.0, y1, 15.0, end);

    QVERIFY(!CenterlineFinder::find(data, start, end, 200.0, 400.0).success);

    // 5 mm above: 2 voxels outside the border, which is moved in
    world(8.0, y0, 20.0, start);
    CenterlineFinder::Result result = CenterlineFinder::find(data, start, end, 200.0, 400.0);
    QVERIFY2(result.success, qPrintable(result.message));
}

void test_CenterlineFinder::find_ShouldFailForPointsOutsideOrNotConnected()
{
    vtkSmartPointer<vtkImageData> data = makeVolume();
    double tube[3], blob[3], far[3];
    double y;
    centerOf(20.0, &y);
    world(20.0, y, 15.0, tube);
    world(1.0, 1.0, 1.0, blob);
    world(24.0, 40.0, 25.0, far);

    CenterlineFinder::Result notConnected = CenterlineFinder::find(data, tube, blob, 200.0, 400.0);
    QVERIFY(!notConnected.success);
    QVERIFY(notConnected.message.contains("connected"));

    QVERIFY(!CenterlineFinder::find(data, far, tube, 200.0, 400.0).success);
    QVERIFY(!CenterlineFinder::find(data, tube, far, 200.0, 400.0).success);

    // A range that has no voxels
    QVERIFY(!CenterlineFinder::find(data, tube, tube, 1000.0, 2000.0).success);
}

void test_CenterlineFinder::find_ShouldFailForInvalidInput()
{
    vtkSmartPointer<vtkImageData> data = makeVolume();
    double a[3] = {20, 20, 20};

    QVERIFY(!CenterlineFinder::find(nullptr, a, a, 0, 1).success);
    QVERIFY(!CenterlineFinder::find(data, a, a, 500.0, 100.0).success);

    vtkSmartPointer<vtkImageData> color = vtkSmartPointer<vtkImageData>::New();
    color->SetDimensions(4, 4, 4);
    color->AllocateScalars(VTK_UNSIGNED_CHAR, 3);
    QVERIFY(!CenterlineFinder::find(color, a, a, 0, 1).success);
}

void test_CenterlineFinder::find_ShouldWorkWithAnExtentThatDoesNotStartAtZero()
{
    vtkSmartPointer<vtkImageData> data = makeVolume(7);
    double start[3], end[3];
    double y0, y1;
    centerOf(6.0, &y0);
    centerOf(42.0, &y1);

    // With the extent shifted by 7 the origin is the position of the voxel (7, 7, 7), so the world positions move with it
    start[0] = 5.0 + (6.0 + 7.0);
    start[1] = -3.0 + (y0 + 7.0);
    start[2] = 10.0 + (15.0 + 7.0);
    end[0] = 5.0 + (42.0 + 7.0);
    end[1] = -3.0 + (y1 + 7.0);
    end[2] = 10.0 + (15.0 + 7.0);

    CenterlineFinder::Result result = CenterlineFinder::find(data, start, end, 200.0, 400.0);
    QVERIFY2(result.success, qPrintable(result.message));
    QVERIFY(result.lengthMm > 36.0);
}

void test_CenterlineFinder::rangeAround_ShouldUseTheMedianAroundThePoint()
{
    vtkSmartPointer<vtkImageData> data = makeVolume();
    double inTube[3], outside[3];
    double y;
    centerOf(20.0, &y);
    world(20.0, y, 15.0, inTube);
    world(20.0, 2.0, 15.0, outside);
    double lower = 0.0, upper = 0.0;

    QVERIFY(CenterlineFinder::rangeAround(data, inTube, 100.0, &lower, &upper));
    QVERIFY(std::abs(lower - 200.0) < 80.0 && std::abs(upper - 400.0) < 80.0);
    QVERIFY(upper - lower == 200.0);

    QVERIFY(CenterlineFinder::rangeAround(data, outside, 50.0, &lower, &upper));
    QCOMPARE(lower, -50.0);
    QCOMPARE(upper, 50.0);

    // At the end of the tube, where less than half of the voxels around the point are in it, the median is the one of the structure that has most of them
    double atEnd[3];
    double yEnd;
    centerOf(6.0, &yEnd);
    world(6.0, yEnd, 15.0, atEnd);
    QVERIFY(CenterlineFinder::rangeAround(data, atEnd, 50.0, &lower, &upper));
    QVERIFY(std::abs((lower + upper) / 2.0 - 300.0) < 1e-9);

    double far[3] = {1000, 0, 0};
    QVERIFY(!CenterlineFinder::rangeAround(data, far, 50.0, &lower, &upper));
    QVERIFY(!CenterlineFinder::rangeAround(nullptr, inTube, 50.0, &lower, &upper));
    QVERIFY(!CenterlineFinder::rangeAround(data, inTube, -1.0, &lower, &upper));
}

void test_CenterlineFinder::findAutomatically_ShouldFindThePathFromPointsAtTheEnds()
{
    vtkSmartPointer<vtkImageData> data = makeVolume();
    double start[3], end[3];
    double y0, y1;
    centerOf(6.0, &y0);
    centerOf(42.0, &y1);

    // The start is at the very cap of the tube (one voxel beyond its end), where most of the voxels around are the body
    world(5.0, y0, 15.0, start);
    world(42.0, y1, 15.0, end);

    CenterlineFinder::Result result = CenterlineFinder::findAutomatically(data, start, end, 70.0);
    QVERIFY2(result.success, qPrintable(result.message));
    QVERIFY2(result.lengthMm > 45.0 && result.lengthMm < 55.0, qPrintable(QString("length %1").arg(result.lengthMm)));

    // With a point of the tube at each end it works too
    world(8.0, y0, 15.0, start);
    QVERIFY(CenterlineFinder::findAutomatically(data, start, end, 70.0).success);
}

void test_CenterlineFinder::findAutomatically_ShouldFailWithAMessage()
{
    vtkSmartPointer<vtkImageData> data = makeVolume();
    double tube[3], blob[3], outside[3] = {1000, 0, 0};
    double y;
    centerOf(20.0, &y);
    world(20.0, y, 15.0, tube);
    world(1.0, 1.0, 1.0, blob);

    CenterlineFinder::Result notConnected = CenterlineFinder::findAutomatically(data, tube, blob, 70.0);
    QVERIFY(!notConnected.success);
    QVERIFY(!notConnected.message.isEmpty());

    CenterlineFinder::Result outsideVolume = CenterlineFinder::findAutomatically(data, outside, outside, 70.0);
    QVERIFY(!outsideVolume.success);
    QVERIFY(!outsideVolume.message.isEmpty());
    QVERIFY(!CenterlineFinder::findAutomatically(nullptr, tube, tube, 70.0).success);
}

DECLARE_TEST(test_CenterlineFinder)

#include "test_centerlinefinder.moc"
