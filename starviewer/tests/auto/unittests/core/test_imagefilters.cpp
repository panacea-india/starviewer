/*@
    "name": "test_ImageFilters",
    "requirements": []
 */

#include "autotest.h"
#include "imagefilters.h"

#include <cmath>

#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkPointData.h>

using namespace udg;

class test_ImageFilters : public QObject {

    Q_OBJECT

private slots:
    void gaussianSmooth_ShouldKeepAConstantImage();
    void gaussianSmooth_ShouldSpreadAnImpulseAndKeepItsSum();
    void gaussianSmooth_ShouldNotMixSlices();
    void gaussianSmooth_ShouldUseTheSpacing();
    void unsharpMask_ShouldKeepAConstantImageAndRaiseAnEdge();
    void unsharpMask_ShouldKeepTheRangeOfTheType();
    void filters_ShouldGiveNothing_ForBadInput();
    void filters_ShouldNotChangeTheOriginal();
};

namespace {

vtkSmartPointer<vtkImageData> makeImage(int n, int slices, double spacing, int type = VTK_SHORT)
{
    vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(n, n, slices);
    image->SetSpacing(spacing, spacing, 3.0);
    image->AllocateScalars(type, 1);
    image->GetPointData()->GetScalars()->FillComponent(0, 0.0);
    return image;
}

double sum(vtkImageData *image)
{
    double total = 0.0;
    vtkDataArray *s = image->GetPointData()->GetScalars();

    for (vtkIdType i = 0; i < s->GetNumberOfTuples(); i++)
    {
        total += s->GetTuple1(i);
    }

    return total;
}

}

void test_ImageFilters::gaussianSmooth_ShouldKeepAConstantImage()
{
    vtkSmartPointer<vtkImageData> image = makeImage(20, 2, 1.0);
    image->GetPointData()->GetScalars()->FillComponent(0, 300.0);

    vtkSmartPointer<vtkImageData> result = ImageFilters::gaussianSmooth(image, 2.0);

    QVERIFY(result);
    QCOMPARE(result->GetScalarComponentAsDouble(0, 0, 0, 0), 300.0);
    QCOMPARE(result->GetScalarComponentAsDouble(10, 10, 1, 0), 300.0);
}

void test_ImageFilters::gaussianSmooth_ShouldSpreadAnImpulseAndKeepItsSum()
{
    vtkSmartPointer<vtkImageData> image = makeImage(41, 1, 1.0, VTK_FLOAT);
    image->SetScalarComponentFromDouble(20, 20, 0, 0, 1000.0);

    vtkSmartPointer<vtkImageData> result = ImageFilters::gaussianSmooth(image, 2.0);

    QVERIFY(result->GetScalarComponentAsDouble(20, 20, 0, 0) < 100.0);
    QVERIFY(result->GetScalarComponentAsDouble(22, 20, 0, 0) > 1.0);
    QVERIFY(std::abs(sum(result) - 1000.0) < 1.0);
    // The peak of a Gaussian of sigma 2 in 2D: 1000 / (2 pi 4) = 39.8
    QVERIFY(std::abs(result->GetScalarComponentAsDouble(20, 20, 0, 0) - 39.8) < 1.0);
}

void test_ImageFilters::gaussianSmooth_ShouldNotMixSlices()
{
    vtkSmartPointer<vtkImageData> image = makeImage(21, 3, 1.0, VTK_FLOAT);
    image->SetScalarComponentFromDouble(10, 10, 1, 0, 1000.0);

    vtkSmartPointer<vtkImageData> result = ImageFilters::gaussianSmooth(image, 2.0);

    QCOMPARE(result->GetScalarComponentAsDouble(10, 10, 0, 0), 0.0);
    QCOMPARE(result->GetScalarComponentAsDouble(10, 10, 2, 0), 0.0);
}

void test_ImageFilters::gaussianSmooth_ShouldUseTheSpacing()
{
    // The same 2 mm blur is twice as wide in pixels when the pixels are half the size
    vtkSmartPointer<vtkImageData> coarse = makeImage(41, 1, 1.0, VTK_FLOAT);
    vtkSmartPointer<vtkImageData> fine = makeImage(41, 1, 0.5, VTK_FLOAT);
    coarse->SetScalarComponentFromDouble(20, 20, 0, 0, 1000.0);
    fine->SetScalarComponentFromDouble(20, 20, 0, 0, 1000.0);

    double coarsePeak = ImageFilters::gaussianSmooth(coarse, 2.0)->GetScalarComponentAsDouble(20, 20, 0, 0);
    double finePeak = ImageFilters::gaussianSmooth(fine, 2.0)->GetScalarComponentAsDouble(20, 20, 0, 0);

    QVERIFY(finePeak < coarsePeak / 3.0);
}

void test_ImageFilters::unsharpMask_ShouldKeepAConstantImageAndRaiseAnEdge()
{
    vtkSmartPointer<vtkImageData> image = makeImage(30, 1, 1.0, VTK_FLOAT);

    for (int y = 0; y < 30; y++)
    {
        for (int x = 15; x < 30; x++)
        {
            image->SetScalarComponentFromDouble(x, y, 0, 0, 100.0);
        }
    }

    vtkSmartPointer<vtkImageData> result = ImageFilters::unsharpMask(image, 2.0, 1.0);

    // Far from the edge nothing changes; on the bright side of the edge it overshoots and on the dark side it undershoots
    QVERIFY(std::abs(result->GetScalarComponentAsDouble(2, 10, 0, 0)) < 1e-6);
    QVERIFY(std::abs(result->GetScalarComponentAsDouble(27, 10, 0, 0) - 100.0) < 1e-6);
    QVERIFY(result->GetScalarComponentAsDouble(15, 10, 0, 0) > 100.0);
    QVERIFY(result->GetScalarComponentAsDouble(14, 10, 0, 0) < 0.0);
}

void test_ImageFilters::unsharpMask_ShouldKeepTheRangeOfTheType()
{
    vtkSmartPointer<vtkImageData> image = makeImage(20, 1, 1.0, VTK_UNSIGNED_CHAR);

    for (int x = 10; x < 20; x++)
    {
        for (int y = 0; y < 20; y++)
        {
            image->SetScalarComponentFromDouble(x, y, 0, 0, 255.0);
        }
    }

    vtkSmartPointer<vtkImageData> result = ImageFilters::unsharpMask(image, 1.5, 3.0);

    QCOMPARE(result->GetScalarComponentAsDouble(10, 5, 0, 0), 255.0);
    QCOMPARE(result->GetScalarComponentAsDouble(9, 5, 0, 0), 0.0);
    QCOMPARE(result->GetScalarType(), static_cast<int>(VTK_UNSIGNED_CHAR));
}

void test_ImageFilters::filters_ShouldGiveNothing_ForBadInput()
{
    vtkSmartPointer<vtkImageData> image = makeImage(5, 1, 1.0);

    QVERIFY(!ImageFilters::gaussianSmooth(nullptr, 1.0));
    QVERIFY(!ImageFilters::gaussianSmooth(image, 0.0));
    QVERIFY(!ImageFilters::gaussianSmooth(image, -1.0));
    QVERIFY(!ImageFilters::unsharpMask(image, 1.0, -0.5));

    vtkSmartPointer<vtkImageData> color = vtkSmartPointer<vtkImageData>::New();
    color->SetDimensions(4, 4, 1);
    color->AllocateScalars(VTK_UNSIGNED_CHAR, 3);
    QVERIFY(!ImageFilters::gaussianSmooth(color, 1.0));
}

void test_ImageFilters::filters_ShouldNotChangeTheOriginal()
{
    vtkSmartPointer<vtkImageData> image = makeImage(11, 1, 1.0, VTK_FLOAT);
    image->SetScalarComponentFromDouble(5, 5, 0, 0, 500.0);

    ImageFilters::gaussianSmooth(image, 1.0);
    ImageFilters::unsharpMask(image, 1.0, 1.0);

    QCOMPARE(image->GetScalarComponentAsDouble(5, 5, 0, 0), 500.0);
    QCOMPARE(image->GetScalarComponentAsDouble(6, 5, 0, 0), 0.0);
}

DECLARE_TEST(test_ImageFilters)

#include "test_imagefilters.moc"
