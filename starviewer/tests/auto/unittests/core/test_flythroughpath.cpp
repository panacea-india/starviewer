/*@
    "name": "test_FlythroughPath",
    "requirements": []
 */

#include "autotest.h"
#include "flythroughpath.h"

#include <cmath>

using namespace udg;

class test_FlythroughPath : public QObject {

    Q_OBJECT

private slots:
    void views_ShouldGoAlongAStraightPathAtAnEvenSpeed();
    void views_ShouldLookAheadAlongThePath();
    void views_ShouldKeepTheUpDirectionPerpendicularAndUnit();
    void views_ShouldTurnTheUpDirectionLittleOnACurve();
    void views_ShouldChooseAnotherUpWhenThePathIsVertical();
    void views_ShouldIgnoreRepeatedPoints();
    void views_ShouldBeEmptyForInvalidInput();
    void length_ShouldBeTheLengthOfTheCurve();
    void lastPath_ShouldKeepThePath();

private:
    static QVector<double> circle(int points, double radius);
    static double distance(const std::array<double, 3> &a, const std::array<double, 3> &b);
};

double test_FlythroughPath::distance(const std::array<double, 3> &a, const std::array<double, 3> &b)
{
    return std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]));
}

QVector<double> test_FlythroughPath::circle(int points, double radius)
{
    QVector<double> result;

    for (int i = 0; i < points; i++)
    {
        double a = 3.14159265358979 * 1.5 * i / (points - 1);
        result << radius * std::cos(a) << radius * std::sin(a) << i * 0.5;
    }

    return result;
}

void test_FlythroughPath::views_ShouldGoAlongAStraightPathAtAnEvenSpeed()
{
    QVector<double> path = {0, 0, 0, 10, 0, 0, 30, 0, 0, 60, 0, 0};
    QList<SavedView> views = FlythroughPath::views(path, 7, 5.0, 90.0);

    QCOMPARE(views.size(), 7);
    QVERIFY(distance(views.first().position, {0, 0, 0}) < 1e-6);
    QVERIFY(distance(views.last().position, {60, 0, 0}) < 1e-6);

    for (int i = 0; i < views.size(); i++)
    {
        QVERIFY(std::abs(views[i].position[0] - 10.0 * i) < 1e-6);
        QVERIFY(std::abs(views[i].position[1]) < 1e-9 && std::abs(views[i].position[2]) < 1e-9);
        QVERIFY(!views[i].parallelProjection);
        QCOMPARE(views[i].viewAngle, 90.0);
    }
}

void test_FlythroughPath::views_ShouldLookAheadAlongThePath()
{
    QVector<double> path = {0, 0, 0, 0, 20, 0, 0, 40, 0};
    QList<SavedView> views = FlythroughPath::views(path, 5, 4.0, 60.0);

    for (const SavedView &view : views)
    {
        QVERIFY(std::abs(view.focalPoint[1] - view.position[1] - 4.0) < 1e-6);
        QVERIFY(std::abs(view.focalPoint[0]) < 1e-9);
    }
}

void test_FlythroughPath::views_ShouldKeepTheUpDirectionPerpendicularAndUnit()
{
    QList<SavedView> views = FlythroughPath::views(circle(12, 40.0), 60, 3.0, 90.0);
    QCOMPARE(views.size(), 60);

    for (const SavedView &view : views)
    {
        double dot = 0.0;
        double up2 = 0.0;
        double f2 = 0.0;

        for (int k = 0; k < 3; k++)
        {
            double forward = view.focalPoint[k] - view.position[k];
            dot += forward * view.viewUp[k];
            up2 += view.viewUp[k] * view.viewUp[k];
            f2 += forward * forward;
        }

        QVERIFY(std::abs(dot) < 1e-6);
        QVERIFY(std::abs(up2 - 1.0) < 1e-9);
        QVERIFY(std::abs(std::sqrt(f2) - 3.0) < 1e-6);
    }
}

void test_FlythroughPath::views_ShouldTurnTheUpDirectionLittleOnACurve()
{
    QList<SavedView> views = FlythroughPath::views(circle(12, 40.0), 200, 3.0, 90.0);

    for (int i = 1; i < views.size(); i++)
    {
        double dot = 0.0;

        for (int k = 0; k < 3; k++)
        {
            dot += views[i].viewUp[k] * views[i - 1].viewUp[k];
        }

        QVERIFY2(dot > 0.99, qPrintable(QString("frame %1: %2").arg(i).arg(dot)));
    }
}

void test_FlythroughPath::views_ShouldChooseAnotherUpWhenThePathIsVertical()
{
    QList<SavedView> views = FlythroughPath::views({0, 0, 0, 0, 0, 10, 0, 0, 30}, 5, 2.0, 90.0);

    QCOMPARE(views.size(), 5);
    QVERIFY(std::abs(views[0].viewUp[2]) < 1e-9);
    QVERIFY(std::abs(views[0].viewUp[1] - 1.0) < 1e-9);
}

void test_FlythroughPath::views_ShouldIgnoreRepeatedPoints()
{
    QList<SavedView> views = FlythroughPath::views({0, 0, 0, 0, 0, 0, 10, 0, 0, 10, 0, 0, 20, 0, 0}, 3, 1.0, 90.0);

    QCOMPARE(views.size(), 3);
    QVERIFY(std::abs(views[1].position[0] - 10.0) < 1e-6);
    QVERIFY(std::abs(FlythroughPath::length({0, 0, 0, 0, 0, 0, 10, 0, 0}) - 10.0) < 1e-6);
}

void test_FlythroughPath::views_ShouldBeEmptyForInvalidInput()
{
    QVector<double> ok = {0, 0, 0, 10, 0, 0};

    QVERIFY(FlythroughPath::views(QVector<double>(), 10, 1, 90).isEmpty());
    QVERIFY(FlythroughPath::views({0, 0, 0}, 10, 1, 90).isEmpty());
    QVERIFY(FlythroughPath::views({1, 1, 1, 1, 1, 1}, 10, 1, 90).isEmpty());
    QVERIFY(FlythroughPath::views(ok, 1, 1, 90).isEmpty());
    QVERIFY(FlythroughPath::views(ok, 10, 0, 90).isEmpty());
    QVERIFY(FlythroughPath::views(ok, 10, 1, 0).isEmpty());
    QVERIFY(FlythroughPath::views(ok, 10, std::nan(""), 90).isEmpty());
    QVERIFY(FlythroughPath::views({0, 0, 0, std::nan(""), 0, 0}, 10, 1, 90).isEmpty());
    QVERIFY(!FlythroughPath::views(ok, 2, 1, 90).isEmpty());
}

void test_FlythroughPath::length_ShouldBeTheLengthOfTheCurve()
{
    QVERIFY(std::abs(FlythroughPath::length({0, 0, 0, 3, 4, 0}) - 5.0) < 1e-6);
    QCOMPARE(FlythroughPath::length({0, 0, 0}), 0.0);

    // A curve through points on a helix is about as long as the helix
    QVector<double> helix = circle(30, 40.0);
    double expected = 0.0;

    for (int i = 1; i < 30; i++)
    {
        double a = 3.14159265358979 * 1.5 / 29;
        expected += std::sqrt(std::pow(2 * 40.0 * std::sin(a / 2), 2) + 0.25);
    }

    double length = FlythroughPath::length(helix);
    QVERIFY(length > expected * 0.99 && length < expected * 1.05);
}

void test_FlythroughPath::lastPath_ShouldKeepThePath()
{
    FlythroughPath::setLastPath({1, 2, 3, 4, 5, 6});
    QCOMPARE(FlythroughPath::lastPath(), QVector<double>({1, 2, 3, 4, 5, 6}));
    FlythroughPath::setLastPath(QVector<double>());
    QVERIFY(FlythroughPath::lastPath().isEmpty());
}

DECLARE_TEST(test_FlythroughPath)

#include "test_flythroughpath.moc"
