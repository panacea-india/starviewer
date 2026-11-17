/*@
    "name": "test_GantryTiltCorrector",
    "requirements": []
 */

#include "autotest.h"
#include "gantrytiltcorrector.h"

#include <cmath>

#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkDataArray.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace udg;

class test_GantryTiltCorrector : public QObject {

    Q_OBJECT

private slots:
    void analyse_ShouldFindNoTilt_WhenTheSlicesAreStackedAlongTheNormal();
    void analyse_ShouldGiveTheTiltAndTheOffsets_ForATiltedStack();
    void analyse_ShouldNotCallATiltedAFewHundredthsOfAPixel();
    void analyse_ShouldGiveNothing_ForOneSliceOrNoSpacing();

    void correct_ShouldPutAStraightLineBackInPlace();
    void correct_ShouldKeepAVolumeThatHasNoOffsets();
    void correct_ShouldFillWithTheLowestValue_WhereTheSliceHadNoData();
    void correct_ShouldFail_WhenTheSlicesDoNotMatchTheAnalysis();
};

namespace {

typedef GantryTiltCorrector::Vector Vector;

// The orientation of a head scan with a gantry tilted by `degrees`: rows along x, columns tilted in the y-z plane
void tiltedAxes(double degrees, Vector &row, Vector &column)
{
    double angle = degrees * M_PI / 180.0;
    row = {{1.0, 0.0, 0.0}};
    column = {{0.0, std::cos(angle), std::sin(angle)}};
}

vtkSmartPointer<vtkImageData> makeVolume(int nx, int ny, int nz, double value)
{
    vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(nx, ny, nz);
    image->SetSpacing(1.0, 1.0, 2.0);
    image->AllocateScalars(VTK_SHORT, 1);
    image->GetPointData()->GetScalars()->FillComponent(0, value);
    return image;
}

}

void test_GantryTiltCorrector::analyse_ShouldFindNoTilt_WhenTheSlicesAreStackedAlongTheNormal()
{
    std::vector<Vector> positions;

    for (int i = 0; i < 10; i++)
    {
        positions.push_back({{5.0, -7.0, 3.0 * i}});
    }

    GantryTiltCorrector::Analysis analysis = GantryTiltCorrector::analyse(positions, {{1, 0, 0}}, {{0, 1, 0}}, 1.0, 1.0);

    QVERIFY(!analysis.tilted);
    QCOMPARE(analysis.offsets.size(), static_cast<size_t>(10));
    QVERIFY(std::abs(analysis.tiltDegrees) < 1e-9);
    QVERIFY(std::abs(analysis.depths.back() - 27.0) < 1e-9);
}

void test_GantryTiltCorrector::analyse_ShouldGiveTheTiltAndTheOffsets_ForATiltedStack()
{
    // A tilt of 20 degrees: the table moves 3 mm along z between slices, the slices are tilted
    Vector row, column;
    tiltedAxes(20.0, row, column);
    std::vector<Vector> positions;

    for (int i = 0; i < 20; i++)
    {
        positions.push_back({{0.0, 0.0, 3.0 * i}});
    }

    GantryTiltCorrector::Analysis analysis = GantryTiltCorrector::analyse(positions, row, column, 0.5, 0.5);

    QVERIFY(analysis.tilted);
    QVERIFY(std::abs(analysis.tiltDegrees - 20.0) < 1e-6);

    // Between slices: along the normal 3 cos(20) = 2.819 mm, in the plane (along the column) 3 sin(20) = 1.026 mm; none along the row
    QVERIFY(std::abs(analysis.depths[1] - 3.0 * std::cos(20.0 * M_PI / 180.0)) < 1e-9);
    QVERIFY(std::abs(analysis.offsets[1][0]) < 1e-9);
    QVERIFY(std::abs(analysis.offsets[1][1] - 3.0 * std::sin(20.0 * M_PI / 180.0)) < 1e-9);
    QVERIFY(std::abs(analysis.offsets[19][1] - 19 * 3.0 * std::sin(20.0 * M_PI / 180.0)) < 1e-9);
}

void test_GantryTiltCorrector::analyse_ShouldNotCallATiltedAFewHundredthsOfAPixel()
{
    // A stack where the positions are off by rounding of 0.01 mm of a pixel of 0.5 mm
    std::vector<Vector> positions;

    for (int i = 0; i < 10; i++)
    {
        positions.push_back({{(i % 2) * 0.01, 0.0, 2.0 * i}});
    }

    QVERIFY(!GantryTiltCorrector::analyse(positions, {{1, 0, 0}}, {{0, 1, 0}}, 0.5, 0.5).tilted);
}

void test_GantryTiltCorrector::analyse_ShouldGiveNothing_ForOneSliceOrNoSpacing()
{
    std::vector<Vector> one = {{{0.0, 0.0, 0.0}}};
    QVERIFY(GantryTiltCorrector::analyse(one, {{1, 0, 0}}, {{0, 1, 0}}, 1.0, 1.0).offsets.empty());

    std::vector<Vector> two = {{{0.0, 0.0, 0.0}}, {{0.0, 5.0, 2.0}}};
    QVERIFY(GantryTiltCorrector::analyse(two, {{1, 0, 0}}, {{0, 1, 0}}, 0.0, 1.0).offsets.empty());
    // The same row and column (no normal)
    QVERIFY(GantryTiltCorrector::analyse(two, {{1, 0, 0}}, {{1, 0, 0}}, 1.0, 1.0).offsets.empty());
}

void test_GantryTiltCorrector::correct_ShouldPutAStraightLineBackInPlace()
{
    // A line that goes straight up in the patient is, in a tilted stack, at a pixel that goes down by 2 pixels each slice: y = 20 - 2 k
    const int slices = 8;
    vtkSmartPointer<vtkImageData> image = makeVolume(30, 40, slices, 0.0);

    for (int k = 0; k < slices; k++)
    {
        image->SetScalarComponentFromDouble(10, 20 - 2 * k, k, 0, 1000.0);
    }

    // The slice k is shifted by 2 pixels (spacing 1 mm) from the one before
    GantryTiltCorrector::Analysis analysis;
    analysis.tilted = true;

    for (int k = 0; k < slices; k++)
    {
        analysis.offsets.push_back({{0.0, 2.0 * k}});
        analysis.depths.push_back(2.0 * k);
    }

    QVERIFY(GantryTiltCorrector::correct(image, analysis));

    // The shift is by 2 k pixels, so that the line is at y = 20 - 2k + 2k = 20 in every slice
    for (int k = 0; k < slices; k++)
    {
        QCOMPARE(image->GetScalarComponentAsDouble(10, 20, k, 0), 1000.0);
        QCOMPARE(image->GetScalarComponentAsDouble(10, 21, k, 0), 0.0);
    }
}

void test_GantryTiltCorrector::correct_ShouldKeepAVolumeThatHasNoOffsets()
{
    vtkSmartPointer<vtkImageData> image = makeVolume(5, 5, 3, 7.0);
    image->SetScalarComponentFromDouble(2, 2, 1, 0, 99.0);

    GantryTiltCorrector::Analysis analysis;
    analysis.offsets.assign(3, {{0.0, 0.0}});
    analysis.depths.assign(3, 0.0);

    QVERIFY(GantryTiltCorrector::correct(image, analysis));
    QCOMPARE(image->GetScalarComponentAsDouble(2, 2, 1, 0), 99.0);
    QCOMPARE(image->GetScalarComponentAsDouble(0, 0, 0, 0), 7.0);
}

void test_GantryTiltCorrector::correct_ShouldFillWithTheLowestValue_WhereTheSliceHadNoData()
{
    vtkSmartPointer<vtkImageData> image = makeVolume(10, 10, 2, 500.0);
    image->SetScalarComponentFromDouble(0, 0, 0, 0, -1000.0);

    GantryTiltCorrector::Analysis analysis;
    analysis.offsets.push_back({{0.0, 0.0}});
    analysis.offsets.push_back({{0.0, 3.0}});
    analysis.depths.assign(2, 0.0);

    QVERIFY(GantryTiltCorrector::correct(image, analysis));
    // The second slice moved up by 3 pixels: its first 3 rows come from outside
    QCOMPARE(image->GetScalarComponentAsDouble(5, 0, 1, 0), -1000.0);
    QCOMPARE(image->GetScalarComponentAsDouble(5, 2, 1, 0), -1000.0);
    QCOMPARE(image->GetScalarComponentAsDouble(5, 3, 1, 0), 500.0);
}

void test_GantryTiltCorrector::correct_ShouldFail_WhenTheSlicesDoNotMatchTheAnalysis()
{
    vtkSmartPointer<vtkImageData> image = makeVolume(5, 5, 3, 1.0);
    GantryTiltCorrector::Analysis analysis;
    analysis.offsets.assign(2, {{0.0, 0.0}});

    QVERIFY(!GantryTiltCorrector::correct(image, analysis));
    QVERIFY(!GantryTiltCorrector::correct(nullptr, analysis));
}

DECLARE_TEST(test_GantryTiltCorrector)

#include "test_gantrytiltcorrector.moc"
