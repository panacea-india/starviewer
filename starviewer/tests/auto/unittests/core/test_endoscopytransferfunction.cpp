/*@
    "name": "test_EndoscopyTransferFunction",
    "requirements": []
 */

#include "autotest.h"

#include <cmath>
#include "endoscopytransferfunction.h"

using namespace udg;

class test_EndoscopyTransferFunction : public QObject {

    Q_OBJECT

private slots:
    void airFilledLumen_ShouldMakeAirTransparentAndTissueOpaque();
    void airFilledLumen_ShouldFollowTheThreshold();
    void contrastFilledLumen_ShouldMakeContrastAndOutsideAirTransparentAndTissueOpaque();
    void contrastFilledLumen_ShouldFollowTheThreshold();
    void bothStyles_ShouldHaveColorsForTheWholeRange();
};

void test_EndoscopyTransferFunction::airFilledLumen_ShouldMakeAirTransparentAndTissueOpaque()
{
    TransferFunction function = EndoscopyTransferFunction::airFilledLumen();

    QCOMPARE(function.getOpacity(-1000.0), 0.0);
    QCOMPARE(function.getOpacity(-600.0), 0.0);
    QCOMPARE(function.getOpacity(-400.0), 0.0);
    QVERIFY(function.getOpacity(-360.0) > 0.0 && function.getOpacity(-360.0) < 0.9);
    QVERIFY(std::abs(function.getOpacity(-320.0) - 0.9) < 1e-9);
    QVERIFY(std::abs(function.getOpacity(40.0) - 0.9) < 1e-9);
    QVERIFY(std::abs(function.getOpacity(1500.0) - 0.9) < 1e-9);
}

void test_EndoscopyTransferFunction::airFilledLumen_ShouldFollowTheThreshold()
{
    TransferFunction function = EndoscopyTransferFunction::airFilledLumen(-200.0);

    QCOMPARE(function.getOpacity(-300.0), 0.0);
    QCOMPARE(function.getOpacity(-200.0), 0.0);
    QVERIFY(std::abs(function.getOpacity(-100.0) - 0.9) < 1e-9);
}

void test_EndoscopyTransferFunction::contrastFilledLumen_ShouldMakeContrastAndOutsideAirTransparentAndTissueOpaque()
{
    TransferFunction function = EndoscopyTransferFunction::contrastFilledLumen();

    QCOMPARE(function.getOpacity(-1000.0), 0.0);
    QCOMPARE(function.getOpacity(-150.0), 0.0);
    QVERIFY(std::abs(function.getOpacity(-100.0) - 0.9) < 1e-9);
    QVERIFY(std::abs(function.getOpacity(40.0) - 0.9) < 1e-9);
    QVERIFY(std::abs(function.getOpacity(170.0) - 0.9) < 1e-9);
    QVERIFY(function.getOpacity(210.0) > 0.0 && function.getOpacity(210.0) < 0.9);
    QCOMPARE(function.getOpacity(250.0), 0.0);
    QCOMPARE(function.getOpacity(300.0), 0.0);
    QCOMPARE(function.getOpacity(2000.0), 0.0);
}

void test_EndoscopyTransferFunction::contrastFilledLumen_ShouldFollowTheThreshold()
{
    TransferFunction function = EndoscopyTransferFunction::contrastFilledLumen(400.0);

    QVERIFY(std::abs(function.getOpacity(300.0) - 0.9) < 1e-9);
    QCOMPARE(function.getOpacity(400.0), 0.0);
}

void test_EndoscopyTransferFunction::bothStyles_ShouldHaveColorsForTheWholeRange()
{
    for (const TransferFunction &function : {EndoscopyTransferFunction::airFilledLumen(), EndoscopyTransferFunction::contrastFilledLumen()})
    {
        QVERIFY(!function.name().isEmpty());

        for (double x : {-1024.0, -500.0, 0.0, 300.0, 3000.0})
        {
            QVERIFY(function.getColor(x).isValid());
        }
    }
}

DECLARE_TEST(test_EndoscopyTransferFunction)

#include "test_endoscopytransferfunction.moc"
