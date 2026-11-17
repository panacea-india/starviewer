/*@
    "name": "test_RigidRegistration",
    "requirements": []
 */

#include "autotest.h"
#include "rigidregistration.h"

#include <cmath>
#include <functional>

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

using namespace udg;

namespace {

const double Pi = 3.14159265358979323846;
const int Size = 64;
const double Spacing = 2.0;

// A phantom with no symmetry, in mm around the center of the volume: a body, an ellipsoid, a sphere, a box and a small sphere
double phantom(double x, double y, double z)
{
    auto inEllipsoid = [&](double cx, double cy, double cz, double rx, double ry, double rz) {
        double dx = (x - cx) / rx;
        double dy = (y - cy) / ry;
        double dz = (z - cz) / rz;
        return dx * dx + dy * dy + dz * dz <= 1.0;
    };

    double value = 0.0;

    if (inEllipsoid(0, 0, 0, 45, 40, 35))
    {
        value = 30.0;
    }
    if (inEllipsoid(10, 0, 0, 25, 15, 12))
    {
        value = 100.0;
    }
    if (inEllipsoid(-20, 15, 5, 10, 10, 10))
    {
        value = 200.0;
    }
    if (std::abs(x - 0) <= 12 && std::abs(y + 25) <= 6 && std::abs(z + 10) <= 10)
    {
        value = 150.0;
    }
    if (inEllipsoid(25, 20, -15, 6, 6, 6))
    {
        value = 250.0;
    }

    return value;
}

struct RigidMotion {
    double matrix[3][3];
    double translation[3];

    // Rotation of the given angles in degrees about z and then about x, and a translation in mm
    RigidMotion(double zDegrees, double xDegrees, double tx, double ty, double tz)
    {
        double cz = std::cos(zDegrees * Pi / 180.0);
        double sz = std::sin(zDegrees * Pi / 180.0);
        double cx = std::cos(xDegrees * Pi / 180.0);
        double sx = std::sin(xDegrees * Pi / 180.0);
        double rz[3][3] = {{cz, -sz, 0}, {sz, cz, 0}, {0, 0, 1}};
        double rx[3][3] = {{1, 0, 0}, {0, cx, -sx}, {0, sx, cx}};

        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < 3; j++)
            {
                matrix[i][j] = 0.0;

                for (int k = 0; k < 3; k++)
                {
                    matrix[i][j] += rx[i][k] * rz[k][j];
                }
            }
        }

        translation[0] = tx;
        translation[1] = ty;
        translation[2] = tz;
    }

    void apply(const double in[3], double out[3]) const
    {
        for (int i = 0; i < 3; i++)
        {
            out[i] = matrix[i][0] * in[0] + matrix[i][1] * in[1] + matrix[i][2] * in[2] + translation[i];
        }
    }

    // The inverse: the transpose of the rotation, applied after taking away the translation
    void applyInverse(const double in[3], double out[3]) const
    {
        double shifted[3] = {in[0] - translation[0], in[1] - translation[1], in[2] - translation[2]};

        for (int i = 0; i < 3; i++)
        {
            out[i] = matrix[0][i] * shifted[0] + matrix[1][i] * shifted[1] + matrix[2][i] * shifted[2];
        }
    }
};

// Creates a volume of floats, with the origin so that the center of the volume is at 0, 0, 0, whose value in each point is function(x, y, z)
vtkSmartPointer<vtkImageData> createVolume(const std::function<double(double, double, double)> &function)
{
    vtkSmartPointer<vtkImageData> volume = vtkSmartPointer<vtkImageData>::New();
    volume->SetDimensions(Size, Size, Size);
    volume->SetSpacing(Spacing, Spacing, Spacing);
    double half = (Size - 1) * Spacing / 2.0;
    volume->SetOrigin(-half, -half, -half);
    volume->AllocateScalars(VTK_FLOAT, 1);

    float *pixels = static_cast<float*>(volume->GetScalarPointer());

    for (int k = 0; k < Size; k++)
    {
        for (int j = 0; j < Size; j++)
        {
            for (int i = 0; i < Size; i++)
            {
                pixels[(k * Size + j) * Size + i] = static_cast<float>(function(-half + i * Spacing, -half + j * Spacing, -half + k * Spacing));
            }
        }
    }

    return volume;
}

// The volume of the phantom seen after the motion: its value at x is the phantom at the point that the motion sends to x
vtkSmartPointer<vtkImageData> createMovedVolume(const RigidMotion &motion, const std::function<double(double)> &intensity = [](double value) { return value; })
{
    return createVolume([&](double x, double y, double z) {
        double point[3] = {x, y, z};
        double original[3];
        motion.applyInverse(point, original);
        return intensity(phantom(original[0], original[1], original[2]));
    });
}

// The largest distance between where the result and the true motion send some points, in mm
double largestError(const RigidRegistration::Result &result, const RigidMotion &motion)
{
    double worst = 0.0;

    for (int i = -1; i <= 1; i++)
    {
        for (int j = -1; j <= 1; j++)
        {
            for (int k = -1; k <= 1; k++)
            {
                double point[3] = {20.0 * i, 20.0 * j, 20.0 * k};
                double expected[3];
                double found[3];
                motion.apply(point, expected);
                RigidRegistration::transformPoint(result.matrix, point, found);
                double distance = std::sqrt((expected[0] - found[0]) * (expected[0] - found[0]) + (expected[1] - found[1]) * (expected[1] - found[1])
                                            + (expected[2] - found[2]) * (expected[2] - found[2]));
                worst = std::max(worst, distance);
            }
        }
    }

    return worst;
}

}

class test_RigidRegistration : public QObject {

    Q_OBJECT

private slots:
    void run_ShouldFindNoMotion_ForTheSameVolume();
    void run_ShouldRecoverARotationAndATranslation_WithMeanSquares();
    void run_ShouldRecoverARotationAndATranslation_WithMutualInformation();
    void run_ShouldRecoverTheMotion_BetweenVolumesWithOppositeContrast();
    void run_ShouldTellWhenTheMotionIsLarge_ThroughTheResult();

    void run_ShouldFailAndSayWhy_WithoutAVolume();
    void run_ShouldFailAndSayWhy_WithAVolumeOfSeveralComponents();

    void transformPoint_ShouldApplyTheMatrix();
};

void test_RigidRegistration::run_ShouldFindNoMotion_ForTheSameVolume()
{
    RigidMotion none(0, 0, 0, 0, 0);
    vtkSmartPointer<vtkImageData> fixed = createMovedVolume(none);
    vtkSmartPointer<vtkImageData> moving = createMovedVolume(none);

    RigidRegistration::Options options;
    options.metric = RigidRegistration::Metric::MeanSquares;
    RigidRegistration::Result result = RigidRegistration::run(fixed, moving, options);

    QVERIFY2(result.success, qPrintable(result.message));
    QVERIFY2(largestError(result, none) < 0.5, qPrintable(QString("error %1 mm").arg(largestError(result, none))));
    QVERIFY(result.rotationDegrees < 0.3);
    QVERIFY(result.translationMillimeters < 0.5);
}

void test_RigidRegistration::run_ShouldRecoverARotationAndATranslation_WithMeanSquares()
{
    RigidMotion motion(6.0, 3.0, 5.0, -4.0, 3.0);
    vtkSmartPointer<vtkImageData> fixed = createMovedVolume(RigidMotion(0, 0, 0, 0, 0));
    vtkSmartPointer<vtkImageData> moving = createMovedVolume(motion);

    RigidRegistration::Options options;
    options.metric = RigidRegistration::Metric::MeanSquares;
    RigidRegistration::Result result = RigidRegistration::run(fixed, moving, options);

    QVERIFY2(result.success, qPrintable(result.message));
    double error = largestError(result, motion);
    QVERIFY2(error < 1.5, qPrintable(QString("error %1 mm").arg(error)));
}

void test_RigidRegistration::run_ShouldRecoverARotationAndATranslation_WithMutualInformation()
{
    RigidMotion motion(6.0, 3.0, 5.0, -4.0, 3.0);
    vtkSmartPointer<vtkImageData> fixed = createMovedVolume(RigidMotion(0, 0, 0, 0, 0));
    vtkSmartPointer<vtkImageData> moving = createMovedVolume(motion);

    RigidRegistration::Result result = RigidRegistration::run(fixed, moving);

    QVERIFY2(result.success, qPrintable(result.message));
    double error = largestError(result, motion);
    QVERIFY2(error < 1.5, qPrintable(QString("error %1 mm").arg(error)));
}

void test_RigidRegistration::run_ShouldRecoverTheMotion_BetweenVolumesWithOppositeContrast()
{
    // Another modality: what is bright in one is dark in the other. The mean of squares cannot match them, mutual information can.
    RigidMotion motion(-5.0, 4.0, -6.0, 3.0, -2.0);
    vtkSmartPointer<vtkImageData> fixed = createMovedVolume(RigidMotion(0, 0, 0, 0, 0));
    vtkSmartPointer<vtkImageData> moving = createMovedVolume(motion, [](double value) { return 300.0 - value; });

    RigidRegistration::Result result = RigidRegistration::run(fixed, moving);

    QVERIFY2(result.success, qPrintable(result.message));
    double error = largestError(result, motion);
    QVERIFY2(error < 2.0, qPrintable(QString("error %1 mm").arg(error)));
}

void test_RigidRegistration::run_ShouldTellWhenTheMotionIsLarge_ThroughTheResult()
{
    // The result carries the size of the motion, so that the interface can show how far the volume was moved
    RigidMotion motion(0.0, 0.0, 8.0, 0.0, 0.0);
    vtkSmartPointer<vtkImageData> fixed = createMovedVolume(RigidMotion(0, 0, 0, 0, 0));
    vtkSmartPointer<vtkImageData> moving = createMovedVolume(motion);

    RigidRegistration::Options options;
    options.metric = RigidRegistration::Metric::MeanSquares;
    RigidRegistration::Result result = RigidRegistration::run(fixed, moving, options);

    QVERIFY2(result.success, qPrintable(result.message));
    QVERIFY2(std::abs(result.translationMillimeters - 8.0) < 1.0, qPrintable(QString("translation %1 mm").arg(result.translationMillimeters)));
    QVERIFY(result.rotationDegrees < 1.0);
    QVERIFY(result.iterations > 0);
}

void test_RigidRegistration::run_ShouldFailAndSayWhy_WithoutAVolume()
{
    vtkSmartPointer<vtkImageData> volume = createMovedVolume(RigidMotion(0, 0, 0, 0, 0));

    RigidRegistration::Result result = RigidRegistration::run(nullptr, volume);
    QVERIFY(!result.success);
    QVERIFY(!result.message.isEmpty());

    result = RigidRegistration::run(volume, nullptr);
    QVERIFY(!result.success);
    QVERIFY(!result.message.isEmpty());
}

void test_RigidRegistration::run_ShouldFailAndSayWhy_WithAVolumeOfSeveralComponents()
{
    vtkSmartPointer<vtkImageData> volume = createMovedVolume(RigidMotion(0, 0, 0, 0, 0));
    vtkSmartPointer<vtkImageData> color = vtkSmartPointer<vtkImageData>::New();
    color->SetDimensions(4, 4, 4);
    color->AllocateScalars(VTK_UNSIGNED_CHAR, 3);

    RigidRegistration::Result result = RigidRegistration::run(volume, color);

    QVERIFY(!result.success);
    QCOMPARE(result.message, QString("The volumes must have a single component."));
}

void test_RigidRegistration::transformPoint_ShouldApplyTheMatrix()
{
    // A rotation of 90 degrees about z, and a translation
    double matrix[16] = {0, -1, 0, 5,
                         1, 0, 0, 6,
                         0, 0, 1, 7,
                         0, 0, 0, 1};
    double point[3] = {1, 2, 3};
    double result[3];

    RigidRegistration::transformPoint(matrix, point, result);

    QCOMPARE(result[0], 3.0);
    QCOMPARE(result[1], 7.0);
    QCOMPARE(result[2], 10.0);
}

DECLARE_TEST(test_RigidRegistration)

#include "test_rigidregistration.moc"
