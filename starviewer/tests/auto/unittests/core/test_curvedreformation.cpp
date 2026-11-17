/*@
    "name": "test_CurvedReformation",
    "requirements": []
 */

#include "autotest.h"
#include "curvedreformation.h"

#include <cmath>
#include <functional>

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

using namespace udg;

namespace {

const double Pi = 3.14159265358979323846;

// A volume of floats, with origin 0, whose value in each voxel is function(x, y, z)
vtkSmartPointer<vtkImageData> createVolume(int columns, int rows, int slices, double sx, double sy, double sz, const std::function<double(double, double, double)> &function)
{
    vtkSmartPointer<vtkImageData> volume = vtkSmartPointer<vtkImageData>::New();
    volume->SetDimensions(columns, rows, slices);
    volume->SetSpacing(sx, sy, sz);
    volume->SetOrigin(0.0, 0.0, 0.0);
    volume->AllocateScalars(VTK_FLOAT, 1);
    float *pixels = static_cast<float*>(volume->GetScalarPointer());

    for (int k = 0; k < slices; k++)
    {
        for (int j = 0; j < rows; j++)
        {
            for (int i = 0; i < columns; i++)
            {
                pixels[(k * rows + j) * columns + i] = static_cast<float>(function(i * sx, j * sy, k * sz));
            }
        }
    }

    return volume;
}

// The center line of the tube: it goes along x and winds in y, in the plane z = 30
double centerY(double x)
{
    return 50.0 + 15.0 * std::sin((x - 10.0) / 80.0 * 2.0 * Pi);
}

// A tube of the given radius around the center line, 100 inside and 0 outside, with a soft edge of one voxel. The distance is taken straight in y and z, which is
// close enough to the distance across the line for a gentle curve.
double tube(double x, double y, double z, double radius)
{
    if (x < 5.0 || x > 95.0)
    {
        return 0.0;
    }

    double slope = 15.0 * 2.0 * Pi / 80.0 * std::cos((x - 10.0) / 80.0 * 2.0 * Pi);
    double dy = (y - centerY(x)) / std::sqrt(1.0 + slope * slope);
    double distance = std::sqrt(dy * dy + (z - 30.0) * (z - 30.0));
    return 100.0 * std::max(0.0, std::min(1.0, (radius - distance) + 0.5));
}

// The points of the center line every 10 mm in x
QVector<double> centerPath()
{
    QVector<double> points;

    for (double x = 10.0; x <= 90.0 + 1e-9; x += 10.0)
    {
        points << x << centerY(x) << 30.0;
    }

    return points;
}

// The analytic length of the center line from x = 10 to 90
double analyticLength()
{
    double length = 0.0;
    double previousX = 10.0;
    double previousY = centerY(10.0);

    for (int i = 1; i <= 8000; i++)
    {
        double x = 10.0 + i * 0.01;
        double y = centerY(x);
        length += std::sqrt((x - previousX) * (x - previousX) + (y - previousY) * (y - previousY));
        previousX = x;
        previousY = y;
    }

    return length;
}

// The center of mass of the values of a row of the image, in mm from the center line, and the width of what is above half of the maximum
void rowStatistics(vtkImageData *image, int row, double &centroid, double &width)
{
    int dimensions[3];
    image->GetDimensions(dimensions);
    double spacing = image->GetSpacing()[0];
    double origin = image->GetOrigin()[0];
    double sum = 0.0;
    double weighted = 0.0;
    double maximum = 0.0;

    for (int c = 0; c < dimensions[0]; c++)
    {
        maximum = std::max(maximum, image->GetScalarComponentAsDouble(c, row, 0, 0));
    }

    int above = 0;

    for (int c = 0; c < dimensions[0]; c++)
    {
        double value = image->GetScalarComponentAsDouble(c, row, 0, 0);
        sum += value;
        weighted += value * (origin + c * spacing);
        above += value >= 0.5 * maximum ? 1 : 0;
    }

    centroid = sum > 0.0 ? weighted / sum : 1e9;
    width = above * spacing;
}

}

class test_CurvedReformation : public QObject {

    Q_OBJECT

private slots:
    void straighten_ShouldMakeAWindingTubeStraightAndCentered();
    void straighten_ShouldGiveTheLengthOfTheCurve();
    void straighten_ShouldKeepTheRowsAnEvenStepApart();
    void straighten_ShouldGiveTangentsThatAreUnitAndAlongThePath();
    void straighten_ShouldReadAStraightPathAsTheVolume();
    void straighten_ShouldWorkWithVoxelsThatAreNotCubes();
    void straighten_ShouldUseTheBackgroundOutsideTheVolume();

    void straighten_ShouldFailAndSayWhy_ForABadInput();
};

void test_CurvedReformation::straighten_ShouldMakeAWindingTubeStraightAndCentered()
{
    vtkSmartPointer<vtkImageData> volume = createVolume(100, 100, 60, 1.0, 1.0, 1.0, [](double x, double y, double z) { return tube(x, y, z, 4.0); });
    QVector<double> path = centerPath();

    CurvedReformation::Options options;
    options.width = 30.0;
    options.step = 1.0;
    CurvedReformation::Result result = CurvedReformation::straighten(volume, path.constData(), path.size() / 3, options);

    QVERIFY2(result.success, qPrintable(result.message));
    QCOMPARE(result.columns, 31);
    QVERIFY(result.rows > 80);

    // Every row, away from the two ends, has the tube in the middle and 8 mm wide, in both images
    for (int row = 4; row < result.rows - 4; row += 3)
    {
        double centroid;
        double width;

        rowStatistics(result.normalImage, row, centroid, width);
        QVERIFY2(std::abs(centroid) < 0.8, qPrintable(QString("normal image, row %1: centroid %2 mm").arg(row).arg(centroid)));
        QVERIFY2(std::abs(width - 8.0) < 2.0, qPrintable(QString("normal image, row %1: width %2 mm").arg(row).arg(width)));

        rowStatistics(result.binormalImage, row, centroid, width);
        QVERIFY2(std::abs(centroid) < 0.8, qPrintable(QString("binormal image, row %1: centroid %2 mm").arg(row).arg(centroid)));
        QVERIFY2(std::abs(width - 8.0) < 2.0, qPrintable(QString("binormal image, row %1: width %2 mm").arg(row).arg(width)));
    }
}

void test_CurvedReformation::straighten_ShouldGiveTheLengthOfTheCurve()
{
    vtkSmartPointer<vtkImageData> volume = createVolume(100, 100, 60, 1.0, 1.0, 1.0, [](double x, double y, double z) { return tube(x, y, z, 4.0); });
    QVector<double> path = centerPath();

    CurvedReformation::Result result = CurvedReformation::straighten(volume, path.constData(), path.size() / 3);
    double expected = analyticLength();

    QVERIFY2(result.success, qPrintable(result.message));
    QVERIFY2(std::abs(result.length - expected) / expected < 0.01, qPrintable(QString("length %1 mm, expected %2").arg(result.length).arg(expected)));
    QVERIFY(std::abs(CurvedReformation::curveLength(path.constData(), path.size() / 3) - result.length) < 1e-9);
}

void test_CurvedReformation::straighten_ShouldKeepTheRowsAnEvenStepApart()
{
    vtkSmartPointer<vtkImageData> volume = createVolume(100, 100, 60, 1.0, 1.0, 1.0, [](double x, double y, double z) { return tube(x, y, z, 4.0); });
    QVector<double> path = centerPath();

    CurvedReformation::Options options;
    options.step = 2.0;
    CurvedReformation::Result result = CurvedReformation::straighten(volume, path.constData(), path.size() / 3, options);

    QVERIFY2(result.success, qPrintable(result.message));
    QCOMPARE(result.normalImage->GetSpacing()[1], 2.0);

    // The points the rows are read from are 2 mm apart along the curve: the chord is a little shorter than the arc, by less than 2%
    for (int row = 1; row < result.rows; row++)
    {
        double dx = result.centers[3 * row] - result.centers[3 * (row - 1)];
        double dy = result.centers[3 * row + 1] - result.centers[3 * (row - 1) + 1];
        double dz = result.centers[3 * row + 2] - result.centers[3 * (row - 1) + 2];
        double chord = std::sqrt(dx * dx + dy * dy + dz * dz);
        QVERIFY2(chord <= 2.0 + 1e-6 && chord > 2.0 * 0.98, qPrintable(QString("row %1: chord %2").arg(row).arg(chord)));
    }
}

void test_CurvedReformation::straighten_ShouldGiveTangentsThatAreUnitAndAlongThePath()
{
    vtkSmartPointer<vtkImageData> volume = createVolume(100, 100, 60, 1.0, 1.0, 1.0, [](double x, double y, double z) { return tube(x, y, z, 4.0); });
    QVector<double> path = centerPath();

    CurvedReformation::Result result = CurvedReformation::straighten(volume, path.constData(), path.size() / 3);

    QVERIFY(result.success);
    QCOMPARE(result.tangents.size(), result.centers.size());

    for (int row = 3; row < result.rows - 3; row++)
    {
        double tx = result.tangents[3 * row];
        double ty = result.tangents[3 * row + 1];
        double tz = result.tangents[3 * row + 2];
        QVERIFY(std::abs(std::sqrt(tx * tx + ty * ty + tz * tz) - 1.0) < 1e-9);
        // The path goes in the direction of x, and is in the plane z = 30
        QVERIFY(tx > 0.5);
        QVERIFY(std::abs(tz) < 1e-6);
        // The tangent is the slope of the center line there, within what a spline through points 10 mm apart can give
        double x = result.centers[3 * row];
        double slope = 15.0 * 2.0 * Pi / 80.0 * std::cos((x - 10.0) / 80.0 * 2.0 * Pi);
        QVERIFY2(std::abs(ty / tx - slope) < 0.15, qPrintable(QString("row %1: slope %2, expected %3").arg(row).arg(ty / tx).arg(slope)));
    }
}

void test_CurvedReformation::straighten_ShouldReadAStraightPathAsTheVolume()
{
    // A volume where the value is the x coordinate plus 100 times the y coordinate, and a path along x at y = 20, z = 10
    vtkSmartPointer<vtkImageData> volume = createVolume(60, 60, 30, 1.0, 1.0, 1.0, [](double x, double y, double) { return x + 100.0 * y; });
    double path[] = {5, 20, 10, 25, 20, 10, 45, 20, 10};

    CurvedReformation::Options options;
    options.step = 1.0;
    options.width = 10.0;
    CurvedReformation::Result result = CurvedReformation::straighten(volume, path, 3, options);

    QVERIFY2(result.success, qPrintable(result.message));
    QVERIFY(std::abs(result.length - 40.0) < 1e-6);

    // The normal image goes in z, which does not change the value. The binormal image goes in y, which changes it by 100 for each mm, in one direction or the other.
    for (int row = 0; row < result.rows; row += 5)
    {
        double x = 5.0 + row;
        QVERIFY(std::abs(result.normalImage->GetScalarComponentAsDouble(5, row, 0, 0) - (x + 2000.0)) < 1e-3);
        double offsetColumn = result.binormalImage->GetScalarComponentAsDouble(10, row, 0, 0);
        QVERIFY(std::abs(offsetColumn - (x + 2000.0 + 500.0)) < 1e-3 || std::abs(offsetColumn - (x + 2000.0 - 500.0)) < 1e-3);
    }
}

void test_CurvedReformation::straighten_ShouldWorkWithVoxelsThatAreNotCubes()
{
    // The same tube with the voxels of a CT, in a volume of the same size in mm
    vtkSmartPointer<vtkImageData> volume = createVolume(125, 125, 12, 0.8, 0.8, 5.0, [](double x, double y, double z) { return tube(x, y, z, 5.0); });
    QVector<double> path = centerPath();

    CurvedReformation::Options options;
    options.width = 30.0;
    CurvedReformation::Result result = CurvedReformation::straighten(volume, path.constData(), path.size() / 3, options);

    QVERIFY2(result.success, qPrintable(result.message));
    QCOMPARE(result.normalImage->GetSpacing()[1], 0.8);

    for (int row = 6; row < result.rows - 6; row += 10)
    {
        double centroid;
        double width;
        rowStatistics(result.binormalImage, row, centroid, width);
        QVERIFY2(std::abs(centroid) < 1.0, qPrintable(QString("row %1: centroid %2 mm").arg(row).arg(centroid)));
        QVERIFY2(std::abs(width - 10.0) < 2.5, qPrintable(QString("row %1: width %2 mm").arg(row).arg(width)));
    }
}

void test_CurvedReformation::straighten_ShouldUseTheBackgroundOutsideTheVolume()
{
    vtkSmartPointer<vtkImageData> volume = createVolume(20, 20, 20, 1.0, 1.0, 1.0, [](double, double, double) { return 500.0; });
    // A path that goes out of the volume
    double path[] = {10, 10, 10, 30, 10, 10};

    CurvedReformation::Options options;
    options.step = 1.0;
    options.width = 4.0;
    CurvedReformation::Result result = CurvedReformation::straighten(volume, path, 2, options);

    QVERIFY2(result.success, qPrintable(result.message));
    // The volume is all 500, so the background (the smallest value) is 500 as well
    QCOMPARE(result.normalImage->GetScalarComponentAsDouble(2, result.rows - 1, 0, 0), 500.0);
}

void test_CurvedReformation::straighten_ShouldFailAndSayWhy_ForABadInput()
{
    vtkSmartPointer<vtkImageData> volume = createVolume(20, 20, 20, 1.0, 1.0, 1.0, [](double, double, double) { return 1.0; });
    double one[] = {1, 1, 1};
    double same[] = {5, 5, 5, 5, 5, 5};

    QCOMPARE(CurvedReformation::straighten(volume, one, 1).message, QString("The path needs at least two points."));
    QCOMPARE(CurvedReformation::straighten(nullptr, same, 2).message, QString("The volume must have a single component."));
    CurvedReformation::Result noLength = CurvedReformation::straighten(volume, same, 2);
    QVERIFY(!noLength.success);
    QVERIFY(!noLength.message.isEmpty());

    vtkSmartPointer<vtkImageData> color = vtkSmartPointer<vtkImageData>::New();
    color->SetDimensions(4, 4, 4);
    color->AllocateScalars(VTK_UNSIGNED_CHAR, 3);
    double path[] = {0, 0, 0, 3, 3, 3};
    QCOMPARE(CurvedReformation::straighten(color, path, 2).message, QString("The volume must have a single component."));
}

DECLARE_TEST(test_CurvedReformation)

#include "test_curvedreformation.moc"
