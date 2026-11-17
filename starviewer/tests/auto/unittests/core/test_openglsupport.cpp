/*@
    "name": "test_OpenGLSupport",
    "requirements": []
 */

#include "autotest.h"
#include "openglsupport.h"

using namespace udg;

class test_OpenGLSupport : public QObject {

    Q_OBJECT

private slots:
    void isVersionAtLeast_ShouldCompareMajorThenMinor_data();
    void isVersionAtLeast_ShouldCompareMajorThenMinor();

    void getRequiredVersion_ShouldBeThreeDotTwo();
};

void test_OpenGLSupport::isVersionAtLeast_ShouldCompareMajorThenMinor_data()
{
    QTest::addColumn<int>("major");
    QTest::addColumn<int>("minor");
    QTest::addColumn<QString>("required");
    QTest::addColumn<bool>("expected");

    QTest::newRow("same version") << 3 << 2 << "3.2" << true;
    QTest::newRow("newer minor") << 3 << 3 << "3.2" << true;
    QTest::newRow("newer major, lower minor") << 4 << 0 << "3.2" << true;
    QTest::newRow("older minor") << 3 << 1 << "3.2" << false;
    QTest::newRow("older major, higher minor") << 2 << 9 << "3.2" << false;
    QTest::newRow("the software renderer of Windows") << 1 << 1 << "3.2" << false;
    QTest::newRow("no context") << 0 << 0 << "3.2" << false;
}

void test_OpenGLSupport::isVersionAtLeast_ShouldCompareMajorThenMinor()
{
    QFETCH(int, major);
    QFETCH(int, minor);
    QFETCH(QString, required);
    QFETCH(bool, expected);

    QCOMPARE(OpenGLSupport::isVersionAtLeast(major, minor, required), expected);
}

void test_OpenGLSupport::getRequiredVersion_ShouldBeThreeDotTwo()
{
    QCOMPARE(OpenGLSupport::getRequiredVersion(), QString("3.2"));
}

DECLARE_TEST(test_OpenGLSupport)

#include "test_openglsupport.moc"
