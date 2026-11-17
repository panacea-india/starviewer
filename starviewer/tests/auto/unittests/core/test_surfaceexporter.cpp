/*@
    "name": "test_SurfaceExporter",
    "requirements": []
 */

#include "autotest.h"
#include "surfaceexporter.h"

#include <cmath>
#include <functional>

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <vtkImageData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkSTLReader.h>

using namespace udg;

namespace {

const double Pi = 3.14159265358979323846;

// A volume of floats whose origin makes its center the point 0, 0, 0, with the value function(x, y, z) in each voxel
vtkSmartPointer<vtkImageData> createVolume(int columns, int rows, int slices, double sx, double sy, double sz,
                                           const std::function<double(double, double, double)> &function)
{
    vtkSmartPointer<vtkImageData> volume = vtkSmartPointer<vtkImageData>::New();
    volume->SetDimensions(columns, rows, slices);
    volume->SetSpacing(sx, sy, sz);
    double x0 = -(columns - 1) * sx / 2.0;
    double y0 = -(rows - 1) * sy / 2.0;
    double z0 = -(slices - 1) * sz / 2.0;
    volume->SetOrigin(x0, y0, z0);
    volume->AllocateScalars(VTK_FLOAT, 1);
    float *pixels = static_cast<float*>(volume->GetScalarPointer());

    for (int k = 0; k < slices; k++)
    {
        for (int j = 0; j < rows; j++)
        {
            for (int i = 0; i < columns; i++)
            {
                pixels[(k * rows + j) * columns + i] = static_cast<float>(function(x0 + i * sx, y0 + j * sy, z0 + k * sz));
            }
        }
    }

    return volume;
}

// The value of a ball of the given radius at the given center, with a soft edge of one voxel so that the surface is where the radius says: 100 inside, 0 outside,
// 50 on the sphere
double ball(double x, double y, double z, double cx, double cy, double cz, double radius, double voxel)
{
    double distance = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy) + (z - cz) * (z - cz));
    return 100.0 * std::max(0.0, std::min(1.0, (radius - distance) / voxel + 0.5));
}

double sphereVolumeMilliliters(double radius)
{
    return 4.0 / 3.0 * Pi * radius * radius * radius / 1000.0;
}

}

class test_SurfaceExporter : public QObject {

    Q_OBJECT

private slots:
    void exportStl_ShouldGiveTheVolumeAndBoundsOfASphere();
    void exportStl_ShouldGiveTheVolumeOfASphere_WithVoxelsThatAreNotCubes();
    void exportStl_ShouldCloseTheSurfaceOfAnObjectThatReachesTheEdge();
    void exportStl_ShouldWriteABinaryFileThatReadsBack();
    void exportStl_ShouldWriteATextFileWhenAsked();
    void exportStl_ShouldKeepOnlyTheLargestPiece_WhenAsked();
    void exportStl_ShouldKeepTheVolume_WhenItSmooths();
    void exportStl_ShouldFailAndSayWhy_WhenNothingIsAboveTheThreshold();
    void exportStl_ShouldFailAndSayWhy_ForAVolumeOfSeveralComponents();
    void exportStl_ShouldFailAndSayWhy_WhenTheFileCannotBeWritten();
};

void test_SurfaceExporter::exportStl_ShouldGiveTheVolumeAndBoundsOfASphere()
{
    QTemporaryDir directory;
    vtkSmartPointer<vtkImageData> volume = createVolume(64, 64, 64, 1.0, 1.0, 1.0, [](double x, double y, double z) { return ball(x, y, z, 0, 0, 0, 20.0, 1.0); });

    SurfaceExporter::Options options;
    options.threshold = 50.0;
    SurfaceExporter::Result result = SurfaceExporter::exportStl(volume, directory.path() + "/sphere.stl", options);

    QVERIFY2(result.success, qPrintable(result.message));
    QVERIFY(result.closed);
    QVERIFY(result.triangles > 1000);
    double expected = sphereVolumeMilliliters(20.0);
    QVERIFY2(std::abs(result.volumeMilliliters - expected) / expected < 0.015, qPrintable(QString("volume %1 mL, expected %2").arg(result.volumeMilliliters).arg(expected)));

    for (int axis = 0; axis < 3; axis++)
    {
        QVERIFY2(std::abs(result.bounds[axis * 2] + 20.0) < 1.0 && std::abs(result.bounds[axis * 2 + 1] - 20.0) < 1.0,
                 qPrintable(QString("bounds on %1: %2 to %3").arg(axis).arg(result.bounds[axis * 2]).arg(result.bounds[axis * 2 + 1])));
    }

    double area = 4.0 * Pi * 20.0 * 20.0 / 100.0;
    QVERIFY2(std::abs(result.areaSquareCentimeters - area) / area < 0.03, qPrintable(QString("area %1 cm2, expected %2").arg(result.areaSquareCentimeters).arg(area)));
}

void test_SurfaceExporter::exportStl_ShouldGiveTheVolumeOfASphere_WithVoxelsThatAreNotCubes()
{
    QTemporaryDir directory;
    // The voxels of a CT: small in the plane and long between slices
    vtkSmartPointer<vtkImageData> volume = createVolume(96, 96, 36, 0.8, 0.8, 2.5, [](double x, double y, double z) { return ball(x, y, z, 5, -3, 2, 18.0, 1.0); });

    SurfaceExporter::Options options;
    options.threshold = 50.0;
    SurfaceExporter::Result result = SurfaceExporter::exportStl(volume, directory.path() + "/sphere.stl", options);

    QVERIFY2(result.success, qPrintable(result.message));
    double expected = sphereVolumeMilliliters(18.0);
    QVERIFY2(std::abs(result.volumeMilliliters - expected) / expected < 0.04, qPrintable(QString("volume %1 mL, expected %2").arg(result.volumeMilliliters).arg(expected)));
    // The center is where it was put, in mm
    QVERIFY(std::abs((result.bounds[0] + result.bounds[1]) / 2.0 - 5.0) < 1.0);
    QVERIFY(std::abs((result.bounds[2] + result.bounds[3]) / 2.0 + 3.0) < 1.0);
}

void test_SurfaceExporter::exportStl_ShouldCloseTheSurfaceOfAnObjectThatReachesTheEdge()
{
    QTemporaryDir directory;
    // A ball bigger than the volume: without padding the surface would have holes where it leaves the volume
    vtkSmartPointer<vtkImageData> volume = createVolume(32, 32, 32, 1.0, 1.0, 1.0, [](double x, double y, double z) { return ball(x, y, z, 0, 0, 0, 40.0, 1.0); });

    SurfaceExporter::Options options;
    options.threshold = 50.0;
    SurfaceExporter::Result result = SurfaceExporter::exportStl(volume, directory.path() + "/cube.stl", options);

    QVERIFY2(result.success, qPrintable(result.message));
    QVERIFY(result.closed);
    // The whole volume is inside, so the surface is a box around it: at most one voxel outside the 31 mm of the voxel centers, on each side
    QVERIFY2(result.volumeMilliliters >= 31.0 * 31.0 * 31.0 / 1000.0 && result.volumeMilliliters <= 33.0 * 33.0 * 33.0 / 1000.0, qPrintable(QString("volume %1 mL").arg(result.volumeMilliliters)));
}

void test_SurfaceExporter::exportStl_ShouldWriteABinaryFileThatReadsBack()
{
    QTemporaryDir directory;
    QString path = directory.path() + "/sphere.stl";
    vtkSmartPointer<vtkImageData> volume = createVolume(48, 48, 48, 1.0, 1.0, 1.0, [](double x, double y, double z) { return ball(x, y, z, 0, 0, 0, 15.0, 1.0); });

    SurfaceExporter::Options options;
    options.threshold = 50.0;
    SurfaceExporter::Result result = SurfaceExporter::exportStl(volume, path, options);
    QVERIFY2(result.success, qPrintable(result.message));

    // A binary STL file has 80 bytes of header, 4 of count and 50 for each triangle
    QCOMPARE(QFileInfo(path).size(), static_cast<qint64>(84 + 50 * result.triangles));

    vtkSmartPointer<vtkSTLReader> reader = vtkSmartPointer<vtkSTLReader>::New();
    reader->SetFileName(path.toLocal8Bit().constData());
    reader->Update();
    QCOMPARE(static_cast<int>(reader->GetOutput()->GetNumberOfPolys()), result.triangles);

    double bounds[6];
    reader->GetOutput()->GetBounds(bounds);

    for (int i = 0; i < 6; i++)
    {
        QVERIFY2(std::abs(bounds[i] - result.bounds[i]) < 1e-3, qPrintable(QString("bound %1: %2 against %3").arg(i).arg(bounds[i]).arg(result.bounds[i])));
    }
}

void test_SurfaceExporter::exportStl_ShouldWriteATextFileWhenAsked()
{
    QTemporaryDir directory;
    QString path = directory.path() + "/sphere.stl";
    vtkSmartPointer<vtkImageData> volume = createVolume(24, 24, 24, 1.0, 1.0, 1.0, [](double x, double y, double z) { return ball(x, y, z, 0, 0, 0, 6.0, 1.0); });

    SurfaceExporter::Options options;
    options.threshold = 50.0;
    options.binary = false;
    QVERIFY(SurfaceExporter::exportStl(volume, path, options).success);

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVERIFY(file.read(5).toLower() == "solid");
}

void test_SurfaceExporter::exportStl_ShouldKeepOnlyTheLargestPiece_WhenAsked()
{
    QTemporaryDir directory;
    // A big ball and a small one apart, like a lesion and the noise next to it
    vtkSmartPointer<vtkImageData> volume = createVolume(64, 64, 64, 1.0, 1.0, 1.0, [](double x, double y, double z) {
        return std::max(ball(x, y, z, -12, 0, 0, 12.0, 1.0), ball(x, y, z, 22, 0, 0, 5.0, 1.0));
    });

    SurfaceExporter::Options options;
    options.threshold = 50.0;
    SurfaceExporter::Result both = SurfaceExporter::exportStl(volume, directory.path() + "/both.stl", options);
    options.largestPieceOnly = true;
    SurfaceExporter::Result largest = SurfaceExporter::exportStl(volume, directory.path() + "/largest.stl", options);

    QVERIFY(both.success && largest.success);
    QVERIFY2(std::abs(both.volumeMilliliters - (sphereVolumeMilliliters(12.0) + sphereVolumeMilliliters(5.0))) < 0.15, qPrintable(QString("both: %1 mL").arg(both.volumeMilliliters)));
    QVERIFY2(std::abs(largest.volumeMilliliters - sphereVolumeMilliliters(12.0)) / sphereVolumeMilliliters(12.0) < 0.03, qPrintable(QString("largest: %1 mL").arg(largest.volumeMilliliters)));
    QVERIFY(largest.triangles < both.triangles);
}

void test_SurfaceExporter::exportStl_ShouldKeepTheVolume_WhenItSmooths()
{
    QTemporaryDir directory;
    vtkSmartPointer<vtkImageData> volume = createVolume(64, 64, 64, 1.0, 1.0, 1.0, [](double x, double y, double z) { return ball(x, y, z, 0, 0, 0, 20.0, 1.0); });

    SurfaceExporter::Options options;
    options.threshold = 50.0;
    options.smoothingIterations = 20;
    SurfaceExporter::Result result = SurfaceExporter::exportStl(volume, directory.path() + "/smooth.stl", options);

    QVERIFY2(result.success, qPrintable(result.message));
    double expected = sphereVolumeMilliliters(20.0);
    QVERIFY2(std::abs(result.volumeMilliliters - expected) / expected < 0.03, qPrintable(QString("volume %1 mL, expected %2").arg(result.volumeMilliliters).arg(expected)));
}

void test_SurfaceExporter::exportStl_ShouldFailAndSayWhy_WhenNothingIsAboveTheThreshold()
{
    QTemporaryDir directory;
    vtkSmartPointer<vtkImageData> volume = createVolume(16, 16, 16, 1.0, 1.0, 1.0, [](double, double, double) { return 10.0; });

    SurfaceExporter::Options options;
    options.threshold = 50.0;
    SurfaceExporter::Result result = SurfaceExporter::exportStl(volume, directory.path() + "/none.stl", options);

    QVERIFY(!result.success);
    QCOMPARE(result.message, QString("No voxel is above the threshold, so there is no surface."));
    QVERIFY(!QFile::exists(directory.path() + "/none.stl"));
}

void test_SurfaceExporter::exportStl_ShouldFailAndSayWhy_ForAVolumeOfSeveralComponents()
{
    QTemporaryDir directory;
    vtkSmartPointer<vtkImageData> color = vtkSmartPointer<vtkImageData>::New();
    color->SetDimensions(8, 8, 8);
    color->AllocateScalars(VTK_UNSIGNED_CHAR, 3);

    SurfaceExporter::Result result = SurfaceExporter::exportStl(color, directory.path() + "/color.stl", SurfaceExporter::Options());

    QVERIFY(!result.success);
    QCOMPARE(result.message, QString("The volume must have a single component."));
}

void test_SurfaceExporter::exportStl_ShouldFailAndSayWhy_WhenTheFileCannotBeWritten()
{
    QTemporaryDir directory;
    vtkSmartPointer<vtkImageData> volume = createVolume(24, 24, 24, 1.0, 1.0, 1.0, [](double x, double y, double z) { return ball(x, y, z, 0, 0, 0, 6.0, 1.0); });

    SurfaceExporter::Options options;
    options.threshold = 50.0;
    // The folder does not exist
    SurfaceExporter::Result result = SurfaceExporter::exportStl(volume, directory.path() + "/missing/sphere.stl", options);

    QVERIFY(!result.success);
    QVERIFY(!result.message.isEmpty());
}

DECLARE_TEST(test_SurfaceExporter)

#include "test_surfaceexporter.moc"
