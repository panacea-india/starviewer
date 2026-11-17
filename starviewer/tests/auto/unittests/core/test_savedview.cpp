/*@
    "name": "test_SavedView",
    "requirements": []
 */

#include "autotest.h"
#include "savedview.h"
#include "savedviewstore.h"

#include <cmath>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

using namespace udg;

class test_SavedView : public QObject {

    Q_OBJECT

private slots:
    void json_ShouldRoundTrip();
    void fromJson_ShouldBeInvalid_WhenAFieldIsMissingOrWrong();

    void orbited_ShouldKeepTheDistanceAndTurnTheRightAngle();
    void orbited_ShouldGoBackAfterAFullTurn();

    void interpolate_ShouldGiveTheEndsAtTheEnds();
    void interpolate_ShouldFollowTheArc_BetweenTwoViewsOnACircle();
    void interpolate_ShouldNotFail_ForOppositeViews();

    void path_ShouldHaveTheRequestedFramesAndPassThroughTheKeys();
    void path_ShouldRepeatTheView_WithOneKey();
    void turntable_ShouldNotRepeatTheFirstFrame();

    void store_ShouldKeepTheViewsInOrder();
    void store_ShouldGiveNoViews_ForAMissingOrBrokenFile();
    void fileNameForSeries_ShouldNotContainTheUid();
};

namespace {

SavedView makeView(double x, double y, double z)
{
    SavedView view;
    view.name = "view";
    view.position = {{x, y, z}};
    view.focalPoint = {{0.0, 0.0, 0.0}};
    view.viewUp = {{0.0, 0.0, 1.0}};
    return view;
}

double distanceFromFocal(const SavedView &view)
{
    double d = 0.0;

    for (int i = 0; i < 3; i++)
    {
        d += (view.position[i] - view.focalPoint[i]) * (view.position[i] - view.focalPoint[i]);
    }

    return std::sqrt(d);
}

}

void test_SavedView::json_ShouldRoundTrip()
{
    SavedView view = makeView(1.5, -2.25, 300.0);
    view.name = "Liver";
    view.viewAngle = 45.0;
    view.parallelProjection = true;
    view.parallelScale = 120.5;

    bool valid = false;
    SavedView read = SavedView::fromJson(view.toJson(), &valid);

    QVERIFY(valid);
    QCOMPARE(read.name, QString("Liver"));
    QCOMPARE(read.position, view.position);
    QCOMPARE(read.focalPoint, view.focalPoint);
    QCOMPARE(read.viewUp, view.viewUp);
    QCOMPARE(read.viewAngle, 45.0);
    QCOMPARE(read.parallelProjection, true);
    QCOMPARE(read.parallelScale, 120.5);
}

void test_SavedView::fromJson_ShouldBeInvalid_WhenAFieldIsMissingOrWrong()
{
    QJsonObject good = makeView(1, 2, 3).toJson();
    bool valid = true;

    QJsonObject noName = good;
    noName.remove("name");
    SavedView::fromJson(noName, &valid);
    QVERIFY(!valid);

    QJsonObject shortArray = good;
    shortArray["position"] = QJsonArray{1.0, 2.0};
    SavedView::fromJson(shortArray, &valid);
    QVERIFY(!valid);

    QJsonObject textInArray = good;
    textInArray["viewUp"] = QJsonArray{0.0, "a", 1.0};
    SavedView::fromJson(textInArray, &valid);
    QVERIFY(!valid);

    QJsonObject zeroUp = good;
    zeroUp["viewUp"] = QJsonArray{0.0, 0.0, 0.0};
    SavedView::fromJson(zeroUp, &valid);
    QVERIFY(!valid);

    SavedView::fromJson(QJsonObject(), &valid);
    QVERIFY(!valid);
}

void test_SavedView::orbited_ShouldKeepTheDistanceAndTurnTheRightAngle()
{
    // Looking from +x, with z up: a quarter of a turn counterclockwise seen from above goes to +y
    SavedView view = makeView(100.0, 0.0, 0.0);
    SavedView turned = view.orbited(90.0);

    QVERIFY(std::abs(turned.position[0]) < 1e-9);
    QVERIFY(std::abs(turned.position[1] - 100.0) < 1e-9);
    QVERIFY(std::abs(turned.position[2]) < 1e-9);
    QVERIFY(std::abs(distanceFromFocal(turned) - 100.0) < 1e-9);
    QCOMPARE(turned.focalPoint, view.focalPoint);
    QCOMPARE(turned.viewUp, view.viewUp);

    // A position above the plane keeps its height
    SavedView high = makeView(60.0, 0.0, 80.0).orbited(180.0);
    QVERIFY(std::abs(high.position[0] + 60.0) < 1e-9);
    QVERIFY(std::abs(high.position[2] - 80.0) < 1e-9);
}

void test_SavedView::orbited_ShouldGoBackAfterAFullTurn()
{
    SavedView view = makeView(30.0, 40.0, 50.0);
    SavedView turned = view.orbited(360.0);

    for (int i = 0; i < 3; i++)
    {
        QVERIFY(std::abs(turned.position[i] - view.position[i]) < 1e-9);
    }
}

void test_SavedView::interpolate_ShouldGiveTheEndsAtTheEnds()
{
    SavedView a = makeView(100.0, 0.0, 0.0);
    SavedView b = makeView(0.0, 100.0, 0.0);
    SavedView start = SavedView::interpolate(a, b, 0.0);
    SavedView end = SavedView::interpolate(a, b, 1.0);

    for (int i = 0; i < 3; i++)
    {
        QVERIFY(std::abs(start.position[i] - a.position[i]) < 1e-9);
        QVERIFY(std::abs(end.position[i] - b.position[i]) < 1e-9);
    }

    // Outside 0 to 1 it stays at the ends
    QVERIFY(std::abs(SavedView::interpolate(a, b, 2.0).position[1] - 100.0) < 1e-9);
}

void test_SavedView::interpolate_ShouldFollowTheArc_BetweenTwoViewsOnACircle()
{
    SavedView a = makeView(100.0, 0.0, 0.0);
    SavedView b = makeView(0.0, 100.0, 0.0);
    SavedView middle = SavedView::interpolate(a, b, 0.5);

    // On the circle of radius 100, at 45 degrees; a straight line between the two would be at 70.7 from the center
    QVERIFY(std::abs(distanceFromFocal(middle) - 100.0) < 1e-9);
    QVERIFY(std::abs(middle.position[0] - 100.0 / std::sqrt(2.0)) < 1e-9);
    QVERIFY(std::abs(middle.position[1] - 100.0 / std::sqrt(2.0)) < 1e-9);
}

void test_SavedView::interpolate_ShouldNotFail_ForOppositeViews()
{
    SavedView a = makeView(100.0, 0.0, 0.0);
    SavedView b = makeView(-100.0, 0.0, 0.0);

    for (double t : {0.0, 0.25, 0.5, 0.75, 1.0})
    {
        SavedView view = SavedView::interpolate(a, b, t);

        for (int i = 0; i < 3; i++)
        {
            QVERIFY(std::isfinite(view.position[i]));
        }
    }
}

void test_SavedView::path_ShouldHaveTheRequestedFramesAndPassThroughTheKeys()
{
    QList<SavedView> keys;
    keys << makeView(100.0, 0.0, 0.0) << makeView(0.0, 100.0, 0.0) << makeView(-100.0, 0.0, 0.0);

    QList<SavedView> frames = SavedView::path(keys, 9);

    QCOMPARE(frames.size(), 9);
    // The first, the middle (the second key) and the last frame are the keys
    QVERIFY(std::abs(frames.at(0).position[0] - 100.0) < 1e-9);
    QVERIFY(std::abs(frames.at(4).position[1] - 100.0) < 1e-9);
    QVERIFY(std::abs(frames.at(8).position[0] + 100.0) < 1e-9);

    // And the frames in between are on the circle
    for (const SavedView &frame : frames)
    {
        QVERIFY(std::abs(distanceFromFocal(frame) - 100.0) < 1e-6);
    }

    QCOMPARE(SavedView::path(keys, 0).size(), 0);
    QCOMPARE(SavedView::path(QList<SavedView>(), 5).size(), 0);
}

void test_SavedView::path_ShouldRepeatTheView_WithOneKey()
{
    QList<SavedView> frames = SavedView::path(QList<SavedView>() << makeView(1, 2, 3), 4);

    QCOMPARE(frames.size(), 4);
    QCOMPARE(frames.last().position, makeView(1, 2, 3).position);
}

void test_SavedView::turntable_ShouldNotRepeatTheFirstFrame()
{
    SavedView view = makeView(100.0, 0.0, 0.0);
    QList<SavedView> frames = view.turntable(8);

    QCOMPARE(frames.size(), 8);
    QCOMPARE(frames.first().position, view.position);
    // The last frame is 315 degrees, one step before the first, so that a loop does not stop at the seam
    QVERIFY(std::abs(frames.last().position[0] - 100.0 / std::sqrt(2.0)) < 1e-9);
    QVERIFY(std::abs(frames.last().position[1] + 100.0 / std::sqrt(2.0)) < 1e-9);
}

void test_SavedView::store_ShouldKeepTheViewsInOrder()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString file = dir.filePath("deeper/views.json");

    SavedView first = makeView(1, 2, 3);
    first.name = "First";
    SavedView second = makeView(4, 5, 6);
    second.name = "Second";

    QVERIFY(SavedViewStore::save(file, QList<SavedView>() << first << second));

    QList<SavedView> loaded = SavedViewStore::load(file);
    QCOMPARE(loaded.size(), 2);
    QCOMPARE(loaded.at(0).name, QString("First"));
    QCOMPARE(loaded.at(1).name, QString("Second"));
    QCOMPARE(loaded.at(1).position, second.position);
}

void test_SavedView::store_ShouldGiveNoViews_ForAMissingOrBrokenFile()
{
    QTemporaryDir dir;
    QCOMPARE(SavedViewStore::load(dir.filePath("none.json")).size(), 0);

    QFile broken(dir.filePath("broken.json"));
    QVERIFY(broken.open(QIODevice::WriteOnly));
    broken.write("{ this is not json");
    broken.close();
    QCOMPARE(SavedViewStore::load(dir.filePath("broken.json")).size(), 0);

    // A file with one good view and one without fields keeps the good one
    QFile mixed(dir.filePath("mixed.json"));
    QVERIFY(mixed.open(QIODevice::WriteOnly));
    mixed.write(QJsonDocument(QJsonObject{{"views", QJsonArray{makeView(1, 2, 3).toJson(), QJsonObject{{"name", "bad"}}}}}).toJson());
    mixed.close();
    QCOMPARE(SavedViewStore::load(dir.filePath("mixed.json")).size(), 1);
}

void test_SavedView::fileNameForSeries_ShouldNotContainTheUid()
{
    QString name = SavedViewStore::fileNameForSeries("1.2.840.113619.2.55.3");

    QVERIFY(!name.contains("1.2.840"));
    QVERIFY(name.endsWith(".json"));
    QCOMPARE(name, SavedViewStore::fileNameForSeries("1.2.840.113619.2.55.3"));
    QVERIFY(name != SavedViewStore::fileNameForSeries("1.2.840.113619.2.55.4"));
}

DECLARE_TEST(test_SavedView)

#include "test_savedview.moc"
