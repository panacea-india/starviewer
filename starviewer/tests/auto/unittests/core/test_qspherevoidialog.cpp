/*@
    "name": "test_QSphereVoiDialog",
    "requirements": []
 */

#include "autotest.h"
#include "qspherevoidialog.h"

#include <cmath>

#include <QDoubleSpinBox>
#include <QTableWidget>

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

using namespace udg;

namespace {

// A volume of 1 mm voxels with a background of 1 and a sphere of 8 and radius 20 at the center
vtkSmartPointer<vtkImageData> createPet()
{
    vtkSmartPointer<vtkImageData> volume = vtkSmartPointer<vtkImageData>::New();
    volume->SetDimensions(64, 64, 64);
    volume->SetSpacing(1.0, 1.0, 1.0);
    volume->SetOrigin(-31.5, -31.5, -31.5);
    volume->AllocateScalars(VTK_FLOAT, 1);
    float *pixels = static_cast<float*>(volume->GetScalarPointer());

    for (int k = 0; k < 64; k++)
    {
        for (int j = 0; j < 64; j++)
        {
            for (int i = 0; i < 64; i++)
            {
                double x = -31.5 + i;
                double y = -31.5 + j;
                double z = -31.5 + k;
                pixels[(k * 64 + j) * 64 + i] = x * x + y * y + z * z <= 400.0 ? 800.0f : 100.0f;
            }
        }
    }

    return volume;
}

}

class test_QSphereVoiDialog : public QObject {

    Q_OBJECT

private slots:
    void constructor_ShouldShowTheStatisticsOfTheSphere_InTheUnitsOfTheInput();
    void changingTheRadius_ShouldComputeTheStatisticsAgain();
    void changingTheThreshold_ShouldGiveTheMetabolicVolume();
    void constructor_ShouldSaySoWhenTheSphereHasNoVoxel();
};

void test_QSphereVoiDialog::constructor_ShouldShowTheStatisticsOfTheSphere_InTheUnitsOfTheInput()
{
    vtkSmartPointer<vtkImageData> pet = createPet();
    QSphereVoiDialog::Input input;
    input.name = "PET";
    input.values = pet;
    input.scale = 0.01;      // the factor that turns the stored values into SUV
    input.units = "SUVbw";
    input.center[0] = input.center[1] = input.center[2] = 0.0;

    QSphereVoiDialog dialog({input}, 10.0);
    QTableWidget *table = dialog.findChild<QTableWidget*>("statisticsTable");

    QVERIFY(table);
    QCOMPARE(table->horizontalHeaderItem(0)->text(), QString("PET (SUVbw)"));
    QCOMPARE(table->item(1, 0)->text(), QString("8.00"));     // mean
    QCOMPARE(table->item(2, 0)->text(), QString("8.00"));     // maximum
    QCOMPARE(table->item(3, 0)->text(), QString("8.00"));     // peak
    QCOMPARE(table->item(5, 0)->text(), QString("0.00"));     // deviation
    double expected = 4.0 / 3.0 * 3.14159265358979 * 1000.0 / 1000.0;
    QVERIFY(std::abs(table->item(0, 0)->text().toDouble() - expected) / expected < 0.03);
}

void test_QSphereVoiDialog::changingTheRadius_ShouldComputeTheStatisticsAgain()
{
    vtkSmartPointer<vtkImageData> pet = createPet();
    QSphereVoiDialog::Input input;
    input.name = "PET";
    input.values = pet;
    input.scale = 0.01;
    input.center[0] = input.center[1] = input.center[2] = 0.0;
    QSphereVoiDialog dialog({input}, 10.0);
    QTableWidget *table = dialog.findChild<QTableWidget*>("statisticsTable");

    // A sphere larger than the lesion takes background too: the mean falls, the maximum does not
    dialog.findChild<QDoubleSpinBox*>("radiusSpinBox")->setValue(28.0);

    QVERIFY(table->item(1, 0)->text().toDouble() < 7.0);
    QCOMPARE(table->item(2, 0)->text(), QString("8.00"));
    QCOMPARE(table->item(4, 0)->text(), QString("1.00"));
    QCOMPARE(dialog.getRadius(), 28.0);
}

void test_QSphereVoiDialog::changingTheThreshold_ShouldGiveTheMetabolicVolume()
{
    vtkSmartPointer<vtkImageData> pet = createPet();
    QSphereVoiDialog::Input input;
    input.name = "PET";
    input.values = pet;
    input.scale = 0.01;
    input.center[0] = input.center[1] = input.center[2] = 0.0;
    QSphereVoiDialog dialog({input}, 28.0);
    QTableWidget *table = dialog.findChild<QTableWidget*>("statisticsTable");

    dialog.findChild<QDoubleSpinBox*>("thresholdSpinBox")->setValue(50.0);

    double expected = 4.0 / 3.0 * 3.14159265358979 * 8000.0 / 1000.0;
    QVERIFY2(std::abs(table->item(0, 0)->text().toDouble() - expected) / expected < 0.02, qPrintable(table->item(0, 0)->text()));
    QCOMPARE(table->item(1, 0)->text(), QString("8.00"));
}

void test_QSphereVoiDialog::constructor_ShouldSaySoWhenTheSphereHasNoVoxel()
{
    vtkSmartPointer<vtkImageData> pet = createPet();
    QSphereVoiDialog::Input input;
    input.name = "PET";
    input.values = pet;
    input.scale = 1.0;
    input.center[0] = 1000.0;
    input.center[1] = input.center[2] = 0.0;
    QSphereVoiDialog dialog({input}, 10.0);
    QTableWidget *table = dialog.findChild<QTableWidget*>("statisticsTable");

    QCOMPARE(table->item(0, 0)->text(), QString("-"));
}

DECLARE_TEST(test_QSphereVoiDialog)

#include "test_qspherevoidialog.moc"
