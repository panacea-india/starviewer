/*@
    "name": "test_CalibrationTool",
    "requirements": []
 */

#include "autotest.h"
#include "calibrationtool.h"
#include "image.h"

using namespace udg;

class test_CalibrationTool : public QObject {

    Q_OBJECT

private slots:
    void pixelSizeFor_ShouldDivideTheRealLengthByThePixels();
    void pixelSizeFor_ShouldUseTheSpacingOfTheData();
    void pixelSizeFor_ShouldGiveZero_ForBadInput();
    void image_ShouldKnowThatItWasCalibratedByHand();
};

void test_CalibrationTool::pixelSizeFor_ShouldDivideTheRealLengthByThePixels()
{
    double spacing[3] = {1.0, 1.0, 1.0};
    double a[3] = {10.0, 10.0, 0.0};
    double b[3] = {110.0, 10.0, 0.0};
    double c[3] = {13.0, 14.0, 0.0};
    double d[3] = {13.0, 14.0, 0.0};

    // 100 pixels for 50 mm
    QCOMPARE(CalibrationTool::pixelSizeFor(a, b, spacing, 50.0), 0.5);
    // 3-4-5: 5 pixels for 10 mm, in a slanted direction
    double origin[3] = {10.0, 10.0, 0.0};
    QCOMPARE(CalibrationTool::pixelSizeFor(origin, c, spacing, 10.0), 10.0 / 5.0);
    Q_UNUSED(d);
}

void test_CalibrationTool::pixelSizeFor_ShouldUseTheSpacingOfTheData()
{
    // The coordinates are in the units of the volume: with 2 units per pixel, 100 units are 50 pixels
    double spacing[3] = {2.0, 2.0, 1.0};
    double a[3] = {0.0, 0.0, 0.0};
    double b[3] = {100.0, 0.0, 0.0};

    QCOMPARE(CalibrationTool::pixelSizeFor(a, b, spacing, 25.0), 0.5);
}

void test_CalibrationTool::pixelSizeFor_ShouldGiveZero_ForBadInput()
{
    double spacing[3] = {1.0, 1.0, 1.0};
    double zero[3] = {0.0, 0.0, 0.0};
    double a[3] = {5.0, 5.0, 0.0};
    double badSpacing[3] = {0.0, 1.0, 1.0};

    QCOMPARE(CalibrationTool::pixelSizeFor(a, a, spacing, 10.0), 0.0);
    QCOMPARE(CalibrationTool::pixelSizeFor(zero, a, spacing, 0.0), 0.0);
    QCOMPARE(CalibrationTool::pixelSizeFor(zero, a, spacing, -3.0), 0.0);
    QCOMPARE(CalibrationTool::pixelSizeFor(zero, a, badSpacing, 3.0), 0.0);
}

void test_CalibrationTool::image_ShouldKnowThatItWasCalibratedByHand()
{
    Image image;

    QVERIFY(!image.isManuallyCalibrated());
    image.setManuallyCalibrated(true);
    QVERIFY(image.isManuallyCalibrated());
}

DECLARE_TEST(test_CalibrationTool)

#include "test_calibrationtool.moc"
