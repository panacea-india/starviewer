/*@
    "name": "test_DigitalSubtraction",
    "requirements": []
 */

#include "autotest.h"
#include "digitalsubtraction.h"

#include <cmath>

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

using namespace udg;

class test_DigitalSubtraction : public QObject {

    Q_OBJECT

private slots:
    void subtract_ShouldTakeTheMaskAwayFromEveryImage();
    void subtract_ShouldKeepNegativeValuesAndClampTheRange();
    void subtract_ShouldMoveTheMaskByTheShift();
    void subtract_ShouldReturnNullForInvalidInput();
    void subtract_ShouldKeepGeometry();
    void findShift_ShouldFindTheMovementOfTheMask();
    void findShift_ShouldGiveZeroWhenNothingMoved();
    void findShift_ShouldGiveZeroForInvalidInput();
    void findShifts_ShouldGiveOneShiftPerSlice();

private:
    /// 40 x 40, one slice per movement, background pattern that varies with x and y so that a shift can be found; slice 0 is the mask, each other slice is the pattern moved by
    /// its movement, plus `vessel` in a 4 x 4 block at (20, 20) when vessel is not 0.
    vtkSmartPointer<vtkImageData> makeSeries(int type, const QVector<QPoint> &movement, int vessel);
    static double pattern(int x, int y);
};

double test_DigitalSubtraction::pattern(int x, int y)
{
    return 100 + 20 * ((x * 7 + y * 13) % 11) + ((x * x + y * 3) % 17);
}

vtkSmartPointer<vtkImageData> test_DigitalSubtraction::makeSeries(int type, const QVector<QPoint> &movement, int vessel)
{
    vtkSmartPointer<vtkImageData> data = vtkSmartPointer<vtkImageData>::New();
    data->SetDimensions(40, 40, movement.size());
    data->SetSpacing(0.5, 0.5, 2.0);
    data->SetOrigin(1.0, 2.0, 3.0);
    data->AllocateScalars(type, 1);

    for (int s = 0; s < movement.size(); s++)
    {
        for (int y = 0; y < 40; y++)
        {
            for (int x = 0; x < 40; x++)
            {
                double value = pattern(x - movement[s].x(), y - movement[s].y());

                if (s > 0 && vessel != 0 && x >= 20 && x < 24 && y >= 20 && y < 24)
                {
                    value += vessel;
                }

                data->SetScalarComponentFromDouble(x, y, s, 0, value);
            }
        }
    }

    return data;
}

void test_DigitalSubtraction::subtract_ShouldTakeTheMaskAwayFromEveryImage()
{
    vtkSmartPointer<vtkImageData> series = makeSeries(VTK_UNSIGNED_SHORT, {QPoint(0, 0), QPoint(0, 0), QPoint(0, 0)}, 50);
    vtkSmartPointer<vtkImageData> result = DigitalSubtraction::subtract(series, 0);

    QVERIFY(result);
    QCOMPARE(result->GetScalarType(), static_cast<int>(VTK_SHORT));

    for (int s = 0; s < 3; s++)
    {
        QCOMPARE(result->GetScalarComponentAsDouble(5, 5, s, 0), 0.0);
        QCOMPARE(result->GetScalarComponentAsDouble(21, 21, s, 0), s == 0 ? 0.0 : 50.0);
    }
}

void test_DigitalSubtraction::subtract_ShouldKeepNegativeValuesAndClampTheRange()
{
    vtkSmartPointer<vtkImageData> series = vtkSmartPointer<vtkImageData>::New();
    series->SetDimensions(2, 1, 2);
    series->AllocateScalars(VTK_UNSIGNED_SHORT, 1);
    series->SetScalarComponentFromDouble(0, 0, 0, 0, 1000);   // mask
    series->SetScalarComponentFromDouble(1, 0, 0, 0, 0);
    series->SetScalarComponentFromDouble(0, 0, 1, 0, 400);    // 400 - 1000 = -600
    series->SetScalarComponentFromDouble(1, 0, 1, 0, 65535);  // 65535 - 0 is more than the range

    vtkSmartPointer<vtkImageData> result = DigitalSubtraction::subtract(series, 0);

    QVERIFY(result);
    QCOMPARE(result->GetScalarComponentAsDouble(0, 0, 1, 0), -600.0);
    QCOMPARE(result->GetScalarComponentAsDouble(1, 0, 1, 0), 32767.0);
}

void test_DigitalSubtraction::subtract_ShouldMoveTheMaskByTheShift()
{
    // Slice 1 is the mask moved by (3, -2): with that shift, the subtraction is 0 where the mask has pixels
    vtkSmartPointer<vtkImageData> series = makeSeries(VTK_SHORT, {QPoint(0, 0), QPoint(3, -2)}, 0);
    QVector<QPoint> shifts = {QPoint(0, 0), QPoint(3, -2)};
    vtkSmartPointer<vtkImageData> result = DigitalSubtraction::subtract(series, 0, shifts);

    QVERIFY(result);
    QCOMPARE(result->GetScalarComponentAsDouble(10, 10, 1, 0), 0.0);
    QCOMPARE(result->GetScalarComponentAsDouble(30, 20, 1, 0), 0.0);

    // The first three columns have no mask pixel (x - 3 < 0): 0, not the image
    QCOMPARE(result->GetScalarComponentAsDouble(1, 10, 1, 0), 0.0);

    // Without the shift the images differ
    vtkSmartPointer<vtkImageData> unshifted = DigitalSubtraction::subtract(series, 0);
    bool differs = false;

    for (int x = 10; x < 30; x++)
    {
        differs = differs || unshifted->GetScalarComponentAsDouble(x, 20, 1, 0) != 0.0;
    }

    QVERIFY(differs);
}

void test_DigitalSubtraction::subtract_ShouldReturnNullForInvalidInput()
{
    vtkSmartPointer<vtkImageData> series = makeSeries(VTK_SHORT, {QPoint(0, 0), QPoint(0, 0)}, 0);

    QVERIFY(!DigitalSubtraction::subtract(nullptr, 0));
    QVERIFY(!DigitalSubtraction::subtract(series, -1));
    QVERIFY(!DigitalSubtraction::subtract(series, 2));
    QVERIFY(!DigitalSubtraction::subtract(series, 0, {QPoint(0, 0)}));

    vtkSmartPointer<vtkImageData> color = vtkSmartPointer<vtkImageData>::New();
    color->SetDimensions(4, 4, 2);
    color->AllocateScalars(VTK_UNSIGNED_CHAR, 3);
    QVERIFY(!DigitalSubtraction::subtract(color, 0));
}

void test_DigitalSubtraction::subtract_ShouldKeepGeometry()
{
    vtkSmartPointer<vtkImageData> series = makeSeries(VTK_SHORT, {QPoint(0, 0), QPoint(0, 0)}, 0);
    vtkSmartPointer<vtkImageData> result = DigitalSubtraction::subtract(series, 0);

    QVERIFY(result);
    QCOMPARE(result->GetSpacing()[0], 0.5);
    QCOMPARE(result->GetSpacing()[2], 2.0);
    QCOMPARE(result->GetOrigin()[1], 2.0);
    QCOMPARE(result->GetDimensions()[2], 2);
}

void test_DigitalSubtraction::findShift_ShouldFindTheMovementOfTheMask()
{
    vtkSmartPointer<vtkImageData> series = makeSeries(VTK_UNSIGNED_SHORT, {QPoint(0, 0), QPoint(3, -2), QPoint(-4, 1)}, 80);

    QCOMPARE(DigitalSubtraction::findShift(series, 1, 0, 6), QPoint(3, -2));
    QCOMPARE(DigitalSubtraction::findShift(series, 2, 0, 6), QPoint(-4, 1));

    // If the movement is more than the limit, the shift is not found, but it stays inside the limit
    QPoint limited = DigitalSubtraction::findShift(series, 2, 0, 2);
    QVERIFY(std::abs(limited.x()) <= 2 && std::abs(limited.y()) <= 2);
}

void test_DigitalSubtraction::findShift_ShouldGiveZeroWhenNothingMoved()
{
    vtkSmartPointer<vtkImageData> series = makeSeries(VTK_SHORT, {QPoint(0, 0), QPoint(0, 0)}, 80);

    QCOMPARE(DigitalSubtraction::findShift(series, 1, 0, 5), QPoint(0, 0));
}

void test_DigitalSubtraction::findShift_ShouldGiveZeroForInvalidInput()
{
    vtkSmartPointer<vtkImageData> series = makeSeries(VTK_SHORT, {QPoint(0, 0), QPoint(2, 2)}, 0);

    QCOMPARE(DigitalSubtraction::findShift(nullptr, 1, 0, 5), QPoint(0, 0));
    QCOMPARE(DigitalSubtraction::findShift(series, 5, 0, 5), QPoint(0, 0));
    QCOMPARE(DigitalSubtraction::findShift(series, 1, 1, 5), QPoint(0, 0));
    QCOMPARE(DigitalSubtraction::findShift(series, 1, 0, -1), QPoint(0, 0));
    QCOMPARE(DigitalSubtraction::findShift(series, 1, 0, 30), QPoint(0, 0));   // the shifts leave no room in the 40 pixel image
}

void test_DigitalSubtraction::findShifts_ShouldGiveOneShiftPerSlice()
{
    vtkSmartPointer<vtkImageData> series = makeSeries(VTK_SHORT, {QPoint(0, 0), QPoint(1, 1), QPoint(-2, 0)}, 60);
    QVector<QPoint> shifts = DigitalSubtraction::findShifts(series, 0, 4);

    QCOMPARE(shifts.size(), 3);
    QCOMPARE(shifts[0], QPoint(0, 0));
    QCOMPARE(shifts[1], QPoint(1, 1));
    QCOMPARE(shifts[2], QPoint(-2, 0));
    QVERIFY(DigitalSubtraction::findShifts(nullptr, 0, 4).isEmpty());
}

DECLARE_TEST(test_DigitalSubtraction)

#include "test_digitalsubtraction.moc"
