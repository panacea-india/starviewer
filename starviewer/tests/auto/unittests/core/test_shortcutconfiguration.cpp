/*@
    "name": "test_ShortcutConfiguration",
    "requirements": []
 */

#include "autotest.h"
#include "shortcutconfiguration.h"

using namespace udg;

class test_ShortcutConfiguration : public QObject {

    Q_OBJECT

private slots:
    void displayName_ShouldSplitTheWords_data();
    void displayName_ShouldSplitTheWords();

    void conflicts_ShouldFindTheKeysThatMoreThanOneActionUses();
    void conflicts_ShouldIgnoreEmptyKeysAndOneActionOnly();
    void conflicts_ShouldNotCountTheSameActionTwice();
};

void test_ShortcutConfiguration::displayName_ShouldSplitTheWords_data()
{
    QTest::addColumn<QString>("key");
    QTest::addColumn<QString>("expected");

    QTest::newRow("tool") << "Shortcuts/SlicingMouseTool" << "Slicing mouse tool";
    QTest::newRow("one word") << "Shortcuts/Open" << "Open";
    QTest::newRow("digits") << "Shortcuts/ExternalApplication4" << "External application4";
    QTest::newRow("without prefix") << "ZoomTool" << "Zoom tool";
    QTest::newRow("empty") << "" << "";
}

void test_ShortcutConfiguration::displayName_ShouldSplitTheWords()
{
    QFETCH(QString, key);
    QFETCH(QString, expected);

    QCOMPARE(ShortcutConfiguration::displayName(key), expected);
}

void test_ShortcutConfiguration::conflicts_ShouldFindTheKeysThatMoreThanOneActionUses()
{
    QMap<QString, QList<QKeySequence>> shortcuts;
    shortcuts["Zoom"] = {QKeySequence("Z")};
    shortcuts["Seed"] = {QKeySequence("Z"), QKeySequence("X")};
    shortcuts["Angle"] = {QKeySequence("A")};

    QMap<QString, QStringList> conflicts = ShortcutConfiguration::conflicts(shortcuts);

    QCOMPARE(conflicts.size(), 1);
    QVERIFY(conflicts.contains("Z"));
    QCOMPARE(conflicts["Z"].size(), 2);
    QVERIFY(conflicts["Z"].contains("Zoom") && conflicts["Z"].contains("Seed"));
}

void test_ShortcutConfiguration::conflicts_ShouldIgnoreEmptyKeysAndOneActionOnly()
{
    QMap<QString, QList<QKeySequence>> shortcuts;
    shortcuts["One"] = {QKeySequence()};
    shortcuts["Two"] = {QKeySequence()};
    shortcuts["Three"] = {QKeySequence("Ctrl+O")};

    QVERIFY(ShortcutConfiguration::conflicts(shortcuts).isEmpty());
    QVERIFY(ShortcutConfiguration::conflicts(QMap<QString, QList<QKeySequence>>()).isEmpty());
}

void test_ShortcutConfiguration::conflicts_ShouldNotCountTheSameActionTwice()
{
    QMap<QString, QList<QKeySequence>> shortcuts;
    shortcuts["Zoom"] = {QKeySequence("Z"), QKeySequence("Z")};

    QVERIFY(ShortcutConfiguration::conflicts(shortcuts).isEmpty());
}

DECLARE_TEST(test_ShortcutConfiguration)

#include "test_shortcutconfiguration.moc"
