/*@
    "name": "test_RoutingRules",
    "requirements": []
 */

#include "autotest.h"

#include <functional>

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <dcmtk/dcmdata/dctk.h>

#include "routingjob.h"
#include "routingrules.h"

using namespace udg;

class test_RoutingRules : public QObject {

    Q_OBJECT

private slots:
    void matches_ShouldApplyEachOperator_data();
    void matches_ShouldApplyEachOperator();
    void matches_ShouldCombineConditions();
    void matches_ShouldIgnoreDisabledAndEmptyRules();
    void matches_ShouldRespectCase();
    void destinationsFor_ShouldGiveEachDestinationOnceInOrder();

    void fromJson_ShouldReadWhatToJsonWrites();
    void fromJson_ShouldRefuseBadRules();
    void fromJson_ShouldKeepTheRulesIfTheNewOnesAreBad();

    void plan_ShouldSayWhereEachFileGoes();
    void send_ShouldSendOncePerDestinationWithTheFilesOfTheRules();
    void send_ShouldReportFailures();

private:
    static RoutingRules sampleRules();
    static void saveFile(const QString &path, const QString &modality, const QString &description, const QString &uid);
};

RoutingRules test_RoutingRules::sampleRules()
{
    RoutingRules rules;
    RoutingRules::Destination archive;
    archive.name = "archive";
    archive.host = "10.0.0.1";
    archive.port = 104;
    archive.calledAet = "ARCHIVE";
    RoutingRules::Destination research;
    research.name = "research";
    research.host = "10.0.0.2";
    research.port = 11112;
    research.calledAet = "RESEARCH";
    rules.destinations << archive << research;

    RoutingRules::Rule everyCt;
    everyCt.name = "all CT";
    everyCt.destination = "archive";
    RoutingRules::Condition ct;
    ct.attribute = "Modality";
    ct.value = "CT";
    everyCt.conditions << ct;

    RoutingRules::Rule chest;
    chest.name = "chest for research";
    chest.destination = "research";
    RoutingRules::Condition ctAgain = ct;
    RoutingRules::Condition chestCondition;
    chestCondition.attribute = "StudyDescription";
    chestCondition.op = RoutingRules::Operator::Contains;
    chestCondition.value = "chest";
    chest.conditions << ctAgain << chestCondition;

    RoutingRules::Rule mr;
    mr.name = "MR to the archive";
    mr.destination = "archive";
    RoutingRules::Condition mrCondition;
    mrCondition.attribute = "Modality";
    mrCondition.value = "MR";
    mr.conditions << mrCondition;

    rules.rules << everyCt << chest << mr;
    return rules;
}

void test_RoutingRules::saveFile(const QString &path, const QString &modality, const QString &description, const QString &uid)
{
    DcmFileFormat file;
    DcmDataset *ds = file.getDataset();
    ds->putAndInsertString(DCM_SOPClassUID, "1.2.840.10008.5.1.4.1.1.7");
    ds->putAndInsertString(DCM_SOPInstanceUID, uid.toLatin1().constData());
    ds->putAndInsertString(DCM_Modality, modality.toLatin1().constData());
    ds->putAndInsertString(DCM_StudyDescription, description.toLatin1().constData());
    ds->putAndInsertString(DCM_StudyInstanceUID, "1.2.3");
    file.saveFile(path.toLocal8Bit().constData(), EXS_LittleEndianExplicit);
}

void test_RoutingRules::matches_ShouldApplyEachOperator_data()
{
    QTest::addColumn<int>("op");
    QTest::addColumn<QString>("actual");
    QTest::addColumn<QString>("value");
    QTest::addColumn<bool>("expected");

    QTest::newRow("equals") << int(RoutingRules::Operator::Equals) << "CT" << "CT" << true;
    QTest::newRow("equals not") << int(RoutingRules::Operator::Equals) << "CT" << "MR" << false;
    QTest::newRow("not equals") << int(RoutingRules::Operator::NotEquals) << "CT" << "MR" << true;
    QTest::newRow("not equals same") << int(RoutingRules::Operator::NotEquals) << "CT" << "CT" << false;
    QTest::newRow("contains") << int(RoutingRules::Operator::Contains) << "Chest with contrast" << "with" << true;
    QTest::newRow("contains not") << int(RoutingRules::Operator::Contains) << "Chest" << "abdomen" << false;
    QTest::newRow("not contains") << int(RoutingRules::Operator::NotContains) << "Chest" << "abdomen" << true;
    QTest::newRow("starts with") << int(RoutingRules::Operator::StartsWith) << "Chest CT" << "Chest" << true;
    QTest::newRow("starts with not") << int(RoutingRules::Operator::StartsWith) << "CT Chest" << "Chest" << false;
    QTest::newRow("ends with") << int(RoutingRules::Operator::EndsWith) << "Chest CT" << "CT" << true;
    QTest::newRow("matches") << int(RoutingRules::Operator::Matches) << "CT chest 2" << "^CT .* \\d$" << true;
    QTest::newRow("matches not") << int(RoutingRules::Operator::Matches) << "MR chest" << "^CT" << false;
    QTest::newRow("matches bad regex") << int(RoutingRules::Operator::Matches) << "CT" << "(" << false;
    QTest::newRow("missing value as empty") << int(RoutingRules::Operator::Equals) << "" << "" << true;
}

void test_RoutingRules::matches_ShouldApplyEachOperator()
{
    QFETCH(int, op);
    QFETCH(QString, actual);
    QFETCH(QString, value);
    QFETCH(bool, expected);

    RoutingRules::Rule rule;
    RoutingRules::Condition c;
    c.attribute = "StudyDescription";
    c.op = static_cast<RoutingRules::Operator>(op);
    c.value = value;
    rule.conditions << c;

    QMap<QString, QString> attributes;

    if (!actual.isEmpty())
    {
        attributes["StudyDescription"] = actual;
    }

    QCOMPARE(RoutingRules::matches(rule, attributes), expected);
}

void test_RoutingRules::matches_ShouldCombineConditions()
{
    RoutingRules::Rule rule;
    RoutingRules::Condition a;
    a.attribute = "Modality";
    a.value = "CT";
    RoutingRules::Condition b;
    b.attribute = "BodyPartExamined";
    b.value = "CHEST";
    rule.conditions << a << b;

    QMap<QString, QString> both = {{"Modality", "CT"}, {"BodyPartExamined", "CHEST"}};
    QMap<QString, QString> one = {{"Modality", "CT"}, {"BodyPartExamined", "HEAD"}};
    QMap<QString, QString> none = {{"Modality", "MR"}, {"BodyPartExamined", "HEAD"}};

    rule.matchAll = true;
    QVERIFY(RoutingRules::matches(rule, both));
    QVERIFY(!RoutingRules::matches(rule, one));

    rule.matchAll = false;
    QVERIFY(RoutingRules::matches(rule, both));
    QVERIFY(RoutingRules::matches(rule, one));
    QVERIFY(!RoutingRules::matches(rule, none));
}

void test_RoutingRules::matches_ShouldIgnoreDisabledAndEmptyRules()
{
    RoutingRules rules = sampleRules();
    QMap<QString, QString> ct = {{"Modality", "CT"}};

    QVERIFY(RoutingRules::matches(rules.rules[0], ct));
    rules.rules[0].enabled = false;
    QVERIFY(!RoutingRules::matches(rules.rules[0], ct));

    RoutingRules::Rule empty;
    QVERIFY(!RoutingRules::matches(empty, ct));
}

void test_RoutingRules::matches_ShouldRespectCase()
{
    RoutingRules::Rule rule;
    RoutingRules::Condition c;
    c.attribute = "Modality";
    c.value = "ct";
    rule.conditions << c;
    QMap<QString, QString> ct = {{"Modality", "CT"}};

    QVERIFY(RoutingRules::matches(rule, ct));
    rule.conditions[0].caseSensitive = true;
    QVERIFY(!RoutingRules::matches(rule, ct));
}

void test_RoutingRules::destinationsFor_ShouldGiveEachDestinationOnceInOrder()
{
    RoutingRules rules = sampleRules();
    // A second rule for the archive: a CT of the chest matches "all CT" and "chest for research"
    QMap<QString, QString> chestCt = {{"Modality", "CT"}, {"StudyDescription", "CT of the chest"}};
    QStringList matched;
    QList<RoutingRules::Destination> destinations = rules.destinationsFor(chestCt, &matched);

    QCOMPARE(destinations.size(), 2);
    QCOMPARE(destinations[0].name, QString("archive"));
    QCOMPARE(destinations[1].name, QString("research"));
    QCOMPARE(matched, QStringList() << "all CT" << "chest for research");

    // Two rules for the same destination give it once
    RoutingRules::Rule again = rules.rules[2];
    again.name = "also CT to archive";
    again.conditions[0].value = "CT";
    rules.rules << again;
    QCOMPARE(rules.destinationsFor(chestCt).size(), 2);

    QVERIFY(rules.destinationsFor(QMap<QString, QString>{{"Modality", "US"}}).isEmpty());
}

void test_RoutingRules::fromJson_ShouldReadWhatToJsonWrites()
{
    RoutingRules rules = sampleRules();
    rules.rules[1].matchAll = false;
    rules.rules[2].enabled = false;
    rules.rules[0].conditions[0].caseSensitive = true;

    QJsonObject json = rules.toJson();
    RoutingRules read;
    QStringList problems;
    QVERIFY2(read.fromJson(QJsonDocument::fromJson(QJsonDocument(json).toJson()).object(), &problems), qPrintable(problems.join("; ")));

    QCOMPARE(read.rules.size(), 3);
    QCOMPARE(read.destinations.size(), 2);
    QVERIFY(!read.rules[1].matchAll);
    QVERIFY(!read.rules[2].enabled);
    QVERIFY(read.rules[0].conditions[0].caseSensitive);
    QCOMPARE(read.rules[1].conditions[1].op, RoutingRules::Operator::Contains);
    QCOMPARE(read.destinations[1].port, 11112);
    QCOMPARE(read.destinations[0].callingAet, QString("STARVIEWER"));
    QVERIFY(read.validate().isEmpty());
}

void test_RoutingRules::fromJson_ShouldRefuseBadRules()
{
    auto problemsOf = [](const std::function<void(QJsonObject&)> &change) {
        QJsonObject json = sampleRules().toJson();
        change(json);
        RoutingRules rules;
        QStringList problems;
        bool ok = rules.fromJson(json, &problems);
        return ok ? QStringList() : problems;
    };

    QVERIFY(!problemsOf([](QJsonObject &j) { j.remove("rules"); }).isEmpty());
    QVERIFY(!problemsOf([](QJsonObject &j) { QJsonArray a = j["rules"].toArray(); QJsonObject r = a[0].toObject(); r["destination"] = "nowhere"; a[0] = r; j["rules"] = a; }).isEmpty());
    QVERIFY(!problemsOf([](QJsonObject &j) { QJsonArray a = j["rules"].toArray(); QJsonObject r = a[0].toObject(); QJsonArray c = r["conditions"].toArray(); QJsonObject co = c[0].toObject(); co["attribute"] = "PatientName"; c[0] = co; r["conditions"] = c; a[0] = r; j["rules"] = a; }).isEmpty());
    QVERIFY(!problemsOf([](QJsonObject &j) { QJsonArray a = j["rules"].toArray(); QJsonObject r = a[0].toObject(); QJsonArray c = r["conditions"].toArray(); QJsonObject co = c[0].toObject(); co["operator"] = "like"; c[0] = co; r["conditions"] = c; a[0] = r; j["rules"] = a; }).isEmpty());
    QVERIFY(!problemsOf([](QJsonObject &j) { QJsonArray a = j["rules"].toArray(); QJsonObject r = a[0].toObject(); QJsonArray c = r["conditions"].toArray(); QJsonObject co = c[0].toObject(); co["operator"] = "matches"; co["value"] = "("; c[0] = co; r["conditions"] = c; a[0] = r; j["rules"] = a; }).isEmpty());
    QVERIFY(!problemsOf([](QJsonObject &j) { QJsonArray a = j["rules"].toArray(); QJsonObject r = a[0].toObject(); r["conditions"] = QJsonArray(); a[0] = r; j["rules"] = a; }).isEmpty());
    QVERIFY(!problemsOf([](QJsonObject &j) { QJsonArray a = j["rules"].toArray(); QJsonObject r = a[0].toObject(); r["match"] = "most"; a[0] = r; j["rules"] = a; }).isEmpty());
    QVERIFY(!problemsOf([](QJsonObject &j) { QJsonArray a = j["destinations"].toArray(); QJsonObject d = a[0].toObject(); d["port"] = 70000; a[0] = d; j["destinations"] = a; }).isEmpty());
    QVERIFY(!problemsOf([](QJsonObject &j) { QJsonArray a = j["destinations"].toArray(); QJsonObject d = a[0].toObject(); d["calledAet"] = "THIS_AE_TITLE_IS_TOO_LONG"; a[0] = d; j["destinations"] = a; }).isEmpty());
    QVERIFY(!problemsOf([](QJsonObject &j) { QJsonArray a = j["destinations"].toArray(); a.append(a[0]); j["destinations"] = a; }).isEmpty());
    QVERIFY(!problemsOf([](QJsonObject &j) { QJsonArray a = j["rules"].toArray(); a.append(a[0]); j["rules"] = a; }).isEmpty());
    QVERIFY(problemsOf([](QJsonObject &) {}).isEmpty());
}

void test_RoutingRules::fromJson_ShouldKeepTheRulesIfTheNewOnesAreBad()
{
    RoutingRules rules = sampleRules();
    QJsonObject bad = rules.toJson();
    bad.remove("destinations");

    QStringList problems;
    QVERIFY(!rules.fromJson(bad, &problems));
    QVERIFY(!problems.isEmpty());
    QCOMPARE(rules.rules.size(), 3);
    QCOMPARE(rules.destinations.size(), 2);
}

void test_RoutingRules::plan_ShouldSayWhereEachFileGoes()
{
    QTemporaryDir dir;
    QDir().mkpath(dir.filePath("sub"));
    saveFile(dir.filePath("a.dcm"), "CT", "CT of the chest", "1.1");
    saveFile(dir.filePath("sub/b.dcm"), "CT", "CT of the head", "1.2");
    saveFile(dir.filePath("c.dcm"), "MR", "MR knee", "1.3");
    saveFile(dir.filePath("d.dcm"), "US", "Abdomen", "1.4");
    QFile text(dir.filePath("readme.txt"));
    QVERIFY(text.open(QIODevice::WriteOnly));
    text.write("not dicom");
    text.close();

    RoutingJob::Plan plan = RoutingJob::plan(dir.path(), sampleRules());

    QCOMPARE(plan.routed.size(), 3);
    QCOMPARE(plan.unrouted.size(), 1);
    QVERIFY(plan.unrouted.first().endsWith("d.dcm"));
    QCOMPARE(plan.notDicom, 1);
    QCOMPARE(plan.perDestination.value("archive"), 3);
    QCOMPARE(plan.perDestination.value("research"), 1);

    RoutingJob::PlanEntry chest;

    for (const RoutingJob::PlanEntry &e : plan.routed)
    {
        if (e.file.endsWith("a.dcm"))
        {
            chest = e;
        }
    }

    QCOMPARE(chest.destinations, QStringList() << "archive" << "research");
    QCOMPARE(chest.rules, QStringList() << "all CT" << "chest for research");

    bool dicom = false;
    QMap<QString, QString> attributes = RoutingJob::readAttributes(dir.filePath("c.dcm"), &dicom);
    QVERIFY(dicom);
    QCOMPARE(attributes.value("Modality"), QString("MR"));
    QVERIFY(!attributes.contains("BodyPartExamined"));
    RoutingJob::readAttributes(dir.filePath("readme.txt"), &dicom);
    QVERIFY(!dicom);
}

void test_RoutingRules::send_ShouldSendOncePerDestinationWithTheFilesOfTheRules()
{
    QTemporaryDir dir;
    saveFile(dir.filePath("a.dcm"), "CT", "CT of the chest", "1.1");
    saveFile(dir.filePath("b.dcm"), "CT", "CT of the head", "1.2");
    saveFile(dir.filePath("c.dcm"), "MR", "MR knee", "1.3");
    RoutingRules rules = sampleRules();
    RoutingJob::Plan plan = RoutingJob::plan(dir.path(), rules);

    QMap<QString, QStringList> sentTo;
    int calls = 0;
    RoutingJob::SendResult result = RoutingJob::send(plan, rules, [&](const RoutingRules::Destination &d, const QStringList &files, int *sent, QStringList *) {
        calls++;
        sentTo[d.name] = files;
        *sent = files.size();
        return true;
    });

    QVERIFY(result.success);
    QCOMPARE(calls, 2);
    QCOMPARE(result.sent, 4);
    QCOMPARE(sentTo["archive"].size(), 3);
    QCOMPARE(sentTo["research"].size(), 1);
    QVERIFY(sentTo["research"].first().endsWith("a.dcm"));
}

void test_RoutingRules::send_ShouldReportFailures()
{
    QTemporaryDir dir;
    saveFile(dir.filePath("a.dcm"), "CT", "CT of the chest", "1.1");
    saveFile(dir.filePath("c.dcm"), "MR", "MR knee", "1.3");
    RoutingRules rules = sampleRules();
    RoutingJob::Plan plan = RoutingJob::plan(dir.path(), rules);

    // The archive refuses one file, and the research PACS cannot be reached
    RoutingJob::SendResult result = RoutingJob::send(plan, rules, [](const RoutingRules::Destination &d, const QStringList &files, int *sent, QStringList *errors) {
        if (d.name == "archive")
        {
            *sent = files.size() - 1;
            *errors << "c.dcm: status c000";
            return true;
        }

        *sent = 0;
        *errors << "connection refused";
        return false;
    });

    QVERIFY(!result.success);
    QCOMPARE(result.sent, 1);
    QCOMPARE(result.failed, 2);
    QCOMPARE(result.errors.size(), 2);
    QVERIFY(result.errors[0].startsWith("archive: "));

    QVERIFY(!RoutingJob::send(plan, rules, nullptr).success);
}

DECLARE_TEST(test_RoutingRules)

#include "test_routingrules.moc"
