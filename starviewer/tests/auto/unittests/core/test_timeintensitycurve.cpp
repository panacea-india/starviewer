/*@
    "name": "test_TimeIntensityCurve",
    "requirements": []
 */

#include "autotest.h"
#include <cmath>
#include "timeintensitycurve.h"

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

using namespace udg;

class test_TimeIntensityCurve : public QObject {

    Q_OBJECT

private slots:
    void compute_ShouldGiveTheMeanOfEachPhase();
    void compute_ShouldUseTheRequestedSlice();
    void compute_ShouldGiveTheStandardDeviation();
    void compute_ShouldIgnoreThePixelsOutsideTheImage();
    void compute_ShouldGiveEmptyCurveForInvalidInput();
    void peakAndRise_ShouldFindThePhases();
    void toCsv_ShouldHaveHeaderAndOneLinePerPhase();

private:
    /// 8 x 8 pixels, 2 slices of 3 phases (z = slice * 3 + phase); the value is 100 * slice + 10 * phase, plus 5 for the pixel x = 3, y = 3
    vtkSmartPointer<vtkImageData> makeData(bool spike = false);
};

vtkSmartPointer<vtkImageData> test_TimeIntensityCurve::makeData(bool spike)
{
    vtkSmartPointer<vtkImageData> data = vtkSmartPointer<vtkImageData>::New();
    data->SetDimensions(8, 8, 6);
    data->AllocateScalars(VTK_SHORT, 1);

    for (int z = 0; z < 6; z++)
    {
        int slice = z / 3;
        int phase = z % 3;

        for (int y = 0; y < 8; y++)
        {
            for (int x = 0; x < 8; x++)
            {
                double value = 100 * slice + 10 * phase;

                if (spike && x == 3 && y == 3)
                {
                    value += 5;
                }

                data->SetScalarComponentFromDouble(x, y, z, 0, value);
            }
        }
    }

    return data;
}

void test_TimeIntensityCurve::compute_ShouldGiveTheMeanOfEachPhase()
{
    TimeIntensityCurve curve = TimeIntensityCurve::compute(makeData(), 3, 0, 4.0, 4.0, 2.0, 1.0, 1.0);

    QCOMPARE(curve.mean.size(), 3);
    QCOMPARE(curve.mean[0], 0.0);
    QCOMPARE(curve.mean[1], 10.0);
    QCOMPARE(curve.mean[2], 20.0);
    QVERIFY(curve.pixels > 5);
}

void test_TimeIntensityCurve::compute_ShouldUseTheRequestedSlice()
{
    TimeIntensityCurve curve = TimeIntensityCurve::compute(makeData(), 3, 1, 4.0, 4.0, 1.0, 1.0, 1.0);

    QCOMPARE(curve.mean.size(), 3);
    QCOMPARE(curve.mean[0], 100.0);
    QCOMPARE(curve.mean[2], 120.0);
}

void test_TimeIntensityCurve::compute_ShouldGiveTheStandardDeviation()
{
    // Radius 0 is the pixel at the center only: no deviation
    TimeIntensityCurve single = TimeIntensityCurve::compute(makeData(true), 3, 0, 3.0, 3.0, 0.0, 1.0, 1.0);
    QCOMPARE(single.pixels, 1);
    QCOMPARE(single.mean[0], 5.0);
    QCOMPARE(single.standardDeviation[0], 0.0);

    // The pixel and its four neighbours: values 5, 0, 0, 0, 0 -> mean 1, deviation 2
    TimeIntensityCurve cross = TimeIntensityCurve::compute(makeData(true), 3, 0, 3.0, 3.0, 1.0, 1.0, 1.0);
    QCOMPARE(cross.pixels, 5);
    QCOMPARE(cross.mean[0], 1.0);
    QCOMPARE(cross.standardDeviation[0], 2.0);
}

void test_TimeIntensityCurve::compute_ShouldIgnoreThePixelsOutsideTheImage()
{
    // A disc at the corner has only the pixels that are inside: the quarter of a disc of radius 2 (pixel 0,0 and its neighbours inside)
    TimeIntensityCurve corner = TimeIntensityCurve::compute(makeData(), 3, 0, 0.0, 0.0, 2.0, 1.0, 1.0);
    QVERIFY(!corner.isEmpty());
    QCOMPARE(corner.pixels, 6);

    // A disc that is entirely outside has nothing to measure
    QVERIFY(TimeIntensityCurve::compute(makeData(), 3, 0, 50.0, 50.0, 2.0, 1.0, 1.0).isEmpty());
}

void test_TimeIntensityCurve::compute_ShouldGiveEmptyCurveForInvalidInput()
{
    vtkSmartPointer<vtkImageData> data = makeData();

    QVERIFY(TimeIntensityCurve::compute(nullptr, 3, 0, 4, 4, 2, 1, 1).isEmpty());
    QVERIFY(TimeIntensityCurve::compute(data, 0, 0, 4, 4, 2, 1, 1).isEmpty());
    QVERIFY(TimeIntensityCurve::compute(data, 3, -1, 4, 4, 2, 1, 1).isEmpty());
    QVERIFY(TimeIntensityCurve::compute(data, 3, 2, 4, 4, 2, 1, 1).isEmpty());   // only 2 slices of 3 phases
    QVERIFY(TimeIntensityCurve::compute(data, 7, 0, 4, 4, 2, 1, 1).isEmpty());   // more phases than planes
    QVERIFY(TimeIntensityCurve::compute(data, 3, 0, 4, 4, -1, 1, 1).isEmpty());
    QVERIFY(TimeIntensityCurve::compute(data, 3, 0, 4, 4, 2, 0, 1).isEmpty());
    QVERIFY(TimeIntensityCurve::compute(data, 3, 0, std::nan(""), 4, 2, 1, 1).isEmpty());

    TimeIntensityCurve empty;
    QCOMPARE(empty.peakPhase(), -1);
    QCOMPARE(empty.firstPhaseReaching(0.5), -1);
}

void test_TimeIntensityCurve::peakAndRise_ShouldFindThePhases()
{
    TimeIntensityCurve curve;
    curve.mean = {10, 20, 60, 100, 100, 70};
    curve.standardDeviation = QVector<double>(6, 0.0);

    QCOMPARE(curve.peakPhase(), 3);
    QCOMPARE(curve.firstPhaseReaching(0.5), 2);
    QCOMPARE(curve.firstPhaseReaching(1.0), 3);
    QCOMPARE(curve.firstPhaseReaching(0.0), 0);
    QCOMPARE(curve.firstPhaseReaching(1.5), -1);

    curve.mean = {5, 5, 5};
    curve.standardDeviation = QVector<double>(3, 0.0);
    QCOMPARE(curve.firstPhaseReaching(0.5), -1);
}

void test_TimeIntensityCurve::toCsv_ShouldHaveHeaderAndOneLinePerPhase()
{
    TimeIntensityCurve curve = TimeIntensityCurve::compute(makeData(), 3, 0, 4.0, 4.0, 1.0, 1.0, 1.0);
    QStringList lines = curve.toCsv().split('\n', QString::SkipEmptyParts);

    QCOMPARE(lines.size(), 4);
    QCOMPARE(lines[0], QString("phase,mean,standard deviation"));
    QCOMPARE(lines[2], QString("2,10.0000,0.0000"));
}

DECLARE_TEST(test_TimeIntensityCurve)

#include "test_timeintensitycurve.moc"
