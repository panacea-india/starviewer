/*@
    "name": "test_VolumeRoiStatistics",
    "requirements": []
 */

#include "autotest.h"
#include "volumeroistatistics.h"

#include <cmath>
#include <functional>

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

using namespace udg;

namespace {

const double Pi = 3.14159265358979323846;

// A volume of floats with its center at 0, 0, 0 and the value function(x, y, z) in each voxel
vtkSmartPointer<vtkImageData> createVolume(int columns, int rows, int slices, double sx, double sy, double sz, const std::function<double(double, double, double)> &function)
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

// A PET like the corpus one: a background of SUV 1 and a sphere of the given radius at the center with SUV 8
double pet(double x, double y, double z, double radius = 20.0)
{
    return x * x + y * y + z * z <= radius * radius ? 8.0 : 1.0;
}

double sphereVolumeMilliliters(double radius)
{
    return 4.0 / 3.0 * Pi * radius * radius * radius / 1000.0;
}

}

class test_VolumeRoiStatistics : public QObject {

    Q_OBJECT

private slots:
    void compute_ShouldGiveTheVolumeAndTheValuesOfASphereInsideALesion();
    void compute_ShouldGiveTheMetabolicVolume_WithAThresholdOfHalfTheMaximum();
    void compute_ShouldScaleTheValues();
    void compute_ShouldGiveTheMeanAndDeviationOfTwoValues();
    void compute_ShouldUseTheSizeOfTheVoxels();
    void compute_ShouldGiveTheMaximumAndWhereItIs();
    void compute_ShouldGiveThePeakOfTheHottestMillilitre_NotTheMaximum();
    void compute_ShouldGiveThePeakOfALargeLesion_AsItsValue();
    void compute_ShouldGiveNothing_ForAnEmptyOrMismatchedMask();
};

void test_VolumeRoiStatistics::compute_ShouldGiveTheVolumeAndTheValuesOfASphereInsideALesion()
{
    vtkSmartPointer<vtkImageData> values = createVolume(64, 64, 64, 1.0, 1.0, 1.0, [](double x, double y, double z) { return pet(x, y, z); });
    const double center[3] = {0.0, 0.0, 0.0};
    vtkSmartPointer<vtkImageData> mask = VolumeRoiStatistics::createSphereMask(values, center, 18.0);

    VolumeRoiStatistics::Statistics statistics = VolumeRoiStatistics::compute(values, mask);

    double expected = sphereVolumeMilliliters(18.0);
    QVERIFY2(std::abs(statistics.volumeMilliliters - expected) / expected < 0.01, qPrintable(QString("volume %1 mL, expected %2").arg(statistics.volumeMilliliters).arg(expected)));
    QCOMPARE(statistics.mean, 8.0);
    QCOMPARE(statistics.minimum, 8.0);
    QCOMPARE(statistics.maximum, 8.0);
    QCOMPARE(statistics.standardDeviation, 0.0);
}

void test_VolumeRoiStatistics::compute_ShouldGiveTheMetabolicVolume_WithAThresholdOfHalfTheMaximum()
{
    vtkSmartPointer<vtkImageData> values = createVolume(64, 64, 64, 1.0, 1.0, 1.0, [](double x, double y, double z) { return pet(x, y, z); });
    const double center[3] = {0.0, 0.0, 0.0};
    // A sphere around the lesion that takes some of the background too
    vtkSmartPointer<vtkImageData> region = VolumeRoiStatistics::createSphereMask(values, center, 28.0);
    double regionMaximum = VolumeRoiStatistics::compute(values, region).maximum;
    vtkSmartPointer<vtkImageData> metabolic = VolumeRoiStatistics::createThresholdMask(values, region, 1.0, 0.5 * regionMaximum);

    VolumeRoiStatistics::Statistics statistics = VolumeRoiStatistics::compute(values, metabolic);

    double expected = sphereVolumeMilliliters(20.0);
    QVERIFY2(std::abs(statistics.volumeMilliliters - expected) / expected < 0.01, qPrintable(QString("metabolic volume %1 mL, expected %2").arg(statistics.volumeMilliliters).arg(expected)));
    QCOMPARE(statistics.mean, 8.0);
}

void test_VolumeRoiStatistics::compute_ShouldScaleTheValues()
{
    // The values as a PET stores them (activity), and the factor that makes them SUV
    vtkSmartPointer<vtkImageData> stored = createVolume(48, 48, 48, 1.0, 1.0, 1.0, [](double x, double y, double z) { return pet(x, y, z, 12.0) * 250.0; });
    const double center[3] = {0.0, 0.0, 0.0};
    vtkSmartPointer<vtkImageData> mask = VolumeRoiStatistics::createSphereMask(stored, center, 10.0);

    VolumeRoiStatistics::Statistics statistics = VolumeRoiStatistics::compute(stored, mask, 1.0 / 250.0);

    QCOMPARE(statistics.mean, 8.0);
    QCOMPARE(statistics.maximum, 8.0);
}

void test_VolumeRoiStatistics::compute_ShouldGiveTheMeanAndDeviationOfTwoValues()
{
    // The whole of a cube: part of it is 8 and the rest is 1
    vtkSmartPointer<vtkImageData> values = createVolume(40, 40, 40, 1.0, 1.0, 1.0, [](double x, double y, double z) { return pet(x, y, z, 10.0); });
    const double center[3] = {0.0, 0.0, 0.0};
    vtkSmartPointer<vtkImageData> everything = VolumeRoiStatistics::createSphereMask(values, center, 1000.0);

    VolumeRoiStatistics::Statistics statistics = VolumeRoiStatistics::compute(values, everything);

    // The number of voxels with the value 8, counted apart
    long long hot = 0;
    const float *pixels = static_cast<const float*>(values->GetScalarPointer());

    for (int i = 0; i < 40 * 40 * 40; i++)
    {
        hot += pixels[i] == 8.0f ? 1 : 0;
    }

    double total = 40.0 * 40.0 * 40.0;
    double mean = (hot * 8.0 + (total - hot) * 1.0) / total;
    double deviation = std::sqrt((hot * 64.0 + (total - hot) * 1.0) / total - mean * mean);

    QCOMPARE(statistics.voxels, 40LL * 40 * 40);
    QVERIFY(std::abs(statistics.mean - mean) < 1e-9);
    QVERIFY(std::abs(statistics.standardDeviation - deviation) < 1e-9);
    QCOMPARE(statistics.minimum, 1.0);
    QCOMPARE(statistics.maximum, 8.0);
}

void test_VolumeRoiStatistics::compute_ShouldUseTheSizeOfTheVoxels()
{
    vtkSmartPointer<vtkImageData> values = createVolume(20, 20, 10, 0.8, 0.8, 2.5, [](double, double, double) { return 1.0; });
    const double center[3] = {0.0, 0.0, 0.0};
    vtkSmartPointer<vtkImageData> everything = VolumeRoiStatistics::createSphereMask(values, center, 1000.0);

    VolumeRoiStatistics::Statistics statistics = VolumeRoiStatistics::compute(values, everything);

    QCOMPARE(statistics.voxels, 20LL * 20 * 10);
    QVERIFY(std::abs(statistics.volumeMilliliters - 20 * 20 * 10 * 0.8 * 0.8 * 2.5 / 1000.0) < 1e-9);
}

void test_VolumeRoiStatistics::compute_ShouldGiveTheMaximumAndWhereItIs()
{
    // One hot voxel at 5, -7, 3 in a volume of whole millimeters
    vtkSmartPointer<vtkImageData> values = createVolume(33, 33, 33, 1.0, 1.0, 1.0, [](double x, double y, double z) {
        return std::abs(x - 5.0) < 0.1 && std::abs(y + 7.0) < 0.1 && std::abs(z - 3.0) < 0.1 ? 20.0 : 1.0;
    });
    const double center[3] = {0.0, 0.0, 0.0};
    vtkSmartPointer<vtkImageData> mask = VolumeRoiStatistics::createSphereMask(values, center, 1000.0);

    VolumeRoiStatistics::Statistics statistics = VolumeRoiStatistics::compute(values, mask);

    QCOMPARE(statistics.maximum, 20.0);
    QVERIFY(std::abs(statistics.maximumPosition[0] - 5.0) < 1e-6);
    QVERIFY(std::abs(statistics.maximumPosition[1] + 7.0) < 1e-6);
    QVERIFY(std::abs(statistics.maximumPosition[2] - 3.0) < 1e-6);
}

void test_VolumeRoiStatistics::compute_ShouldGiveThePeakOfTheHottestMillilitre_NotTheMaximum()
{
    // A single hot voxel, noise in the practice: the maximum is 20, but the hottest millilitre is almost all background
    vtkSmartPointer<vtkImageData> values = createVolume(41, 41, 41, 1.0, 1.0, 1.0, [](double x, double y, double z) {
        return std::abs(x) < 0.1 && std::abs(y) < 0.1 && std::abs(z) < 0.1 ? 20.0 : 1.0;
    });
    const double center[3] = {0.0, 0.0, 0.0};
    vtkSmartPointer<vtkImageData> mask = VolumeRoiStatistics::createSphereMask(values, center, 1000.0);

    VolumeRoiStatistics::Statistics statistics = VolumeRoiStatistics::compute(values, mask, 1.0, true);

    QCOMPARE(statistics.maximum, 20.0);
    QVERIFY2(statistics.peak > 1.0 && statistics.peak < 1.2, qPrintable(QString("peak %1").arg(statistics.peak)));
}

void test_VolumeRoiStatistics::compute_ShouldGiveThePeakOfALargeLesion_AsItsValue()
{
    vtkSmartPointer<vtkImageData> values = createVolume(64, 64, 64, 1.0, 1.0, 1.0, [](double x, double y, double z) { return pet(x, y, z); });
    const double center[3] = {0.0, 0.0, 0.0};
    vtkSmartPointer<vtkImageData> mask = VolumeRoiStatistics::createSphereMask(values, center, 1000.0);

    VolumeRoiStatistics::Statistics statistics = VolumeRoiStatistics::compute(values, mask, 1.0, true);

    // The lesion is much larger than 1 mL, so a millilitre of it is all at 8
    QVERIFY2(std::abs(statistics.peak - 8.0) < 1e-6, qPrintable(QString("peak %1").arg(statistics.peak)));
}

void test_VolumeRoiStatistics::compute_ShouldGiveNothing_ForAnEmptyOrMismatchedMask()
{
    vtkSmartPointer<vtkImageData> values = createVolume(16, 16, 16, 1.0, 1.0, 1.0, [](double, double, double) { return 1.0; });
    const double faraway[3] = {1000.0, 0.0, 0.0};
    vtkSmartPointer<vtkImageData> empty = VolumeRoiStatistics::createSphereMask(values, faraway, 5.0);
    vtkSmartPointer<vtkImageData> other = createVolume(8, 8, 8, 1.0, 1.0, 1.0, [](double, double, double) { return 1.0; });

    QCOMPARE(VolumeRoiStatistics::compute(values, empty).voxels, 0LL);
    QCOMPARE(VolumeRoiStatistics::compute(values, other).voxels, 0LL);
    QCOMPARE(VolumeRoiStatistics::compute(values, nullptr).voxels, 0LL);
    QCOMPARE(VolumeRoiStatistics::compute(nullptr, values).voxels, 0LL);
}

DECLARE_TEST(test_VolumeRoiStatistics)

#include "test_volumeroistatistics.moc"
