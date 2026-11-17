/*@
    "name": "test_VolumeSculptor",
    "requirements": []
 */

#include "autotest.h"
#include "volumesculptor.h"
#include "movieencoder.h"

#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkDataArray.h>

using namespace udg;

class test_VolumeSculptor : public QObject {

    Q_OBJECT

private slots:
    void keepLargestPiece_ShouldTakeOutTheSmallerGroups();
    void keepLargestPiece_ShouldNotChangeTheOriginal();
    void keepLargestPiece_ShouldKeepGroupsThatTouchOnlyByAFace();
    void keepLargestPiece_ShouldGiveNothing_WhenNothingReachesTheThreshold();

    void eraseSphere_ShouldTakeOutTheVoxelsInsideOnly();
    void keepBox_ShouldTakeOutTheVoxelsOutside();
    void operations_ShouldGiveNothing_WithoutAVolume();

    void frameFileName_ShouldSortAsTheMovie();
    void ffmpegArguments_ShouldMakeAnEvenSizedH264Video();
};

namespace {

vtkSmartPointer<vtkImageData> makeVolume(int nx, int ny, int nz, double spacing, double value)
{
    vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(nx, ny, nz);
    image->SetSpacing(spacing, spacing, spacing);
    image->AllocateScalars(VTK_SHORT, 1);
    image->GetPointData()->GetScalars()->FillComponent(0, value);
    return image;
}

void fillBlock(vtkImageData *image, int x0, int x1, int y0, int y1, int z0, int z1, double value)
{
    for (int z = z0; z <= z1; z++)
    {
        for (int y = y0; y <= y1; y++)
        {
            for (int x = x0; x <= x1; x++)
            {
                image->SetScalarComponentFromDouble(x, y, z, 0, value);
            }
        }
    }
}

}

void test_VolumeSculptor::keepLargestPiece_ShouldTakeOutTheSmallerGroups()
{
    vtkSmartPointer<vtkImageData> image = makeVolume(20, 20, 20, 1.0, -1000.0);
    fillBlock(image, 2, 9, 2, 9, 2, 9, 300.0);        // 512 voxels
    fillBlock(image, 14, 16, 14, 16, 14, 16, 300.0);  // 27 voxels

    VolumeSculptor::Result result = VolumeSculptor::keepLargestPiece(image, 100.0);

    QVERIFY(result.image);
    QCOMPARE(result.keptVoxels, 512LL);
    QCOMPARE(result.removedVoxels, 27LL);
    QCOMPARE(result.image->GetScalarComponentAsDouble(5, 5, 5, 0), 300.0);
    QCOMPARE(result.image->GetScalarComponentAsDouble(15, 15, 15, 0), -1000.0);
    QCOMPARE(result.image->GetScalarComponentAsDouble(0, 0, 0, 0), -1000.0);
}

void test_VolumeSculptor::keepLargestPiece_ShouldNotChangeTheOriginal()
{
    vtkSmartPointer<vtkImageData> image = makeVolume(10, 10, 10, 1.0, 0.0);
    fillBlock(image, 0, 4, 0, 4, 0, 4, 200.0);
    fillBlock(image, 8, 9, 8, 9, 8, 9, 200.0);

    VolumeSculptor::Result result = VolumeSculptor::keepLargestPiece(image, 100.0);

    QVERIFY(result.image);
    QVERIFY(result.image.GetPointer() != image.GetPointer());
    QCOMPARE(image->GetScalarComponentAsDouble(9, 9, 9, 0), 200.0);
    QCOMPARE(result.image->GetScalarComponentAsDouble(9, 9, 9, 0), 0.0);
}

void test_VolumeSculptor::keepLargestPiece_ShouldKeepGroupsThatTouchOnlyByAFace()
{
    vtkSmartPointer<vtkImageData> image = makeVolume(10, 10, 10, 1.0, 0.0);
    fillBlock(image, 0, 4, 0, 4, 0, 4, 200.0);
    // Touches the block by an edge only: it is another group
    fillBlock(image, 5, 5, 5, 5, 0, 4, 200.0);
    // Touches it by a face: it is the same group
    fillBlock(image, 5, 5, 0, 4, 0, 4, 200.0);

    VolumeSculptor::Result result = VolumeSculptor::keepLargestPiece(image, 100.0);

    QVERIFY(result.image);
    // The block 5x5x5 and the layer next to it are one group; the column that touches only by an edge is joined through the layer, as the layer touches it by a face
    QCOMPARE(result.removedVoxels, 0LL);
}

void test_VolumeSculptor::keepLargestPiece_ShouldGiveNothing_WhenNothingReachesTheThreshold()
{
    vtkSmartPointer<vtkImageData> image = makeVolume(5, 5, 5, 1.0, 10.0);
    QVERIFY(!VolumeSculptor::keepLargestPiece(image, 100.0).image);
}

void test_VolumeSculptor::eraseSphere_ShouldTakeOutTheVoxelsInsideOnly()
{
    // 21 voxels of 2 mm: positions 0 to 40 mm
    vtkSmartPointer<vtkImageData> image = makeVolume(21, 21, 21, 2.0, 500.0);
    image->GetPointData()->GetScalars()->SetTuple1(0, -1000.0);  // a lowest value to fill with
    double center[3] = {20.0, 20.0, 20.0};

    VolumeSculptor::Result result = VolumeSculptor::eraseSphere(image, center, 5.0);

    QVERIFY(result.image);
    QCOMPARE(result.image->GetScalarComponentAsDouble(10, 10, 10, 0), -1000.0);  // the center
    QCOMPARE(result.image->GetScalarComponentAsDouble(12, 10, 10, 0), -1000.0);  // 4 mm away
    QCOMPARE(result.image->GetScalarComponentAsDouble(13, 10, 10, 0), 500.0);    // 6 mm away
    QCOMPARE(result.image->GetScalarComponentAsDouble(20, 20, 20, 0), 500.0);
    QCOMPARE(result.removedVoxels + result.keptVoxels, 21LL * 21 * 21);

    // The lattice points with i2 + j2 + k2 <= 6 (steps of 2 mm, radius 5 mm) are exactly 81
    QCOMPARE(result.removedVoxels, 81LL);
}

void test_VolumeSculptor::keepBox_ShouldTakeOutTheVoxelsOutside()
{
    vtkSmartPointer<vtkImageData> image = makeVolume(10, 10, 10, 1.0, 100.0);
    image->GetPointData()->GetScalars()->SetTuple1(0, -50.0);
    double bounds[6] = {2.0, 5.0, 3.0, 6.0, 0.0, 9.0};

    VolumeSculptor::Result result = VolumeSculptor::keepBox(image, bounds);

    QVERIFY(result.image);
    QCOMPARE(result.keptVoxels, 4LL * 4 * 10);
    QCOMPARE(result.image->GetScalarComponentAsDouble(3, 4, 5, 0), 100.0);
    QCOMPARE(result.image->GetScalarComponentAsDouble(1, 4, 5, 0), -50.0);
    QCOMPARE(result.image->GetScalarComponentAsDouble(3, 7, 5, 0), -50.0);
}

void test_VolumeSculptor::operations_ShouldGiveNothing_WithoutAVolume()
{
    double center[3] = {0, 0, 0};
    double bounds[6] = {0, 1, 0, 1, 0, 1};

    QVERIFY(!VolumeSculptor::keepLargestPiece(nullptr, 0.0).image);
    QVERIFY(!VolumeSculptor::eraseSphere(nullptr, center, 1.0).image);
    QVERIFY(!VolumeSculptor::keepBox(nullptr, bounds).image);
}

void test_VolumeSculptor::frameFileName_ShouldSortAsTheMovie()
{
    QCOMPARE(MovieEncoder::frameFileName(0), QString("frame_000000.png"));
    QCOMPARE(MovieEncoder::frameFileName(123), QString("frame_000123.png"));
    QVERIFY(MovieEncoder::frameFileName(9) < MovieEncoder::frameFileName(10));
}

void test_VolumeSculptor::ffmpegArguments_ShouldMakeAnEvenSizedH264Video()
{
    QStringList arguments = MovieEncoder::ffmpegArguments("movies/one", 25, "out.mp4");

    QVERIFY(arguments.contains("libx264"));
    QVERIFY(arguments.contains("yuv420p"));
    QCOMPARE(arguments.at(arguments.indexOf("-framerate") + 1), QString("25"));
    QVERIFY(arguments.at(arguments.indexOf("-i") + 1).endsWith("frame_%06d.png"));
    QCOMPARE(arguments.last(), QString("out.mp4"));
    QVERIFY(arguments.join(' ').contains("ceil(iw/2)*2"));
}

DECLARE_TEST(test_VolumeSculptor)

#include "test_volumesculptor.moc"
