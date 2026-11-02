/*@
    "name": "test_LocalDatabaseStudyDAL",
    "requirements": ["archive.local_storage"]
 */

#include "autotest.h"
#include "localdatabasestudydal.h"

#include "databaseconnection.h"
#include "databasetesthelper.h"
#include "dicommask.h"
#include "localdatabasepatientdal.h"
#include "patient.h"
#include "study.h"

#include <QDate>

using namespace udg;
using namespace testing;

namespace {

const QString StudyUid("1.2.3");

// Inserts a patient with one study in the given database and returns the patient.
Patient* insertPatientWithStudy(DatabaseConnection &databaseConnection, bool partiallyRetrieved = false)
{
    Patient *patient = new Patient();
    patient->setID("1");
    patient->setFullName("Test");
    LocalDatabasePatientDAL(databaseConnection).insert(patient);

    Study *study = new Study();
    study->setInstanceUID(StudyUid);
    study->setPartiallyRetrieved(partiallyRetrieved);
    patient->addStudy(study);
    LocalDatabaseStudyDAL(databaseConnection).insert(study, QDate::currentDate());

    return patient;
}

// Returns the stored mark of the only study, read through the query used by the study lists.
bool isStoredAsPartiallyRetrieved(DatabaseConnection &databaseConnection)
{
    DicomMask mask;
    mask.setStudyInstanceUID(StudyUid);
    QList<Study*> studies = LocalDatabaseStudyDAL(databaseConnection).query(mask);
    bool partiallyRetrieved = studies.at(0)->isPartiallyRetrieved();
    qDeleteAll(studies);
    return partiallyRetrieved;
}

}

class test_LocalDatabaseStudyDAL : public QObject {
    Q_OBJECT

private slots:
    void insert_ShouldStoreTheMark_data();
    void insert_ShouldStoreTheMark();

    void recordRetrieveOutcome_ShouldUpdateTheMarkAsExpected_data();
    void recordRetrieveOutcome_ShouldUpdateTheMarkAsExpected();

    void update_ShouldNotClearTheMark();

    void queryPatientStudy_ShouldReturnTheMark();
};

void test_LocalDatabaseStudyDAL::insert_ShouldStoreTheMark_data()
{
    QTest::addColumn<bool>("partiallyRetrieved");

    QTest::newRow("complete") << false;
    QTest::newRow("partial") << true;
}

void test_LocalDatabaseStudyDAL::insert_ShouldStoreTheMark()
{
    QFETCH(bool, partiallyRetrieved);

    QScopedPointer<DatabaseConnection> databaseConnection(DatabaseTestHelper::getCreatedDatabase());
    QScopedPointer<Patient> patient(insertPatientWithStudy(*databaseConnection, partiallyRetrieved));

    QCOMPARE(isStoredAsPartiallyRetrieved(*databaseConnection), partiallyRetrieved);
}

void test_LocalDatabaseStudyDAL::recordRetrieveOutcome_ShouldUpdateTheMarkAsExpected_data()
{
    QTest::addColumn<bool>("markedBefore");
    QTest::addColumn<bool>("wholeStudyRequested");
    QTest::addColumn<bool>("someImagesMissing");
    QTest::addColumn<bool>("markedAfter");

    QTest::newRow("study retrieve loses images") << false << true << true << true;
    QTest::newRow("series retrieve loses images") << false << false << true << true;
    QTest::newRow("clean study retrieve completes a partial study") << true << true << false << false;
    QTest::newRow("clean series retrieve leaves a partial study partial") << true << false << false << true;
    QTest::newRow("clean series retrieve leaves a complete study complete") << false << false << false << false;
    QTest::newRow("failed retry leaves a partial study partial") << true << true << true << true;
}

void test_LocalDatabaseStudyDAL::recordRetrieveOutcome_ShouldUpdateTheMarkAsExpected()
{
    QFETCH(bool, markedBefore);
    QFETCH(bool, wholeStudyRequested);
    QFETCH(bool, someImagesMissing);
    QFETCH(bool, markedAfter);

    QScopedPointer<DatabaseConnection> databaseConnection(DatabaseTestHelper::getCreatedDatabase());
    QScopedPointer<Patient> patient(insertPatientWithStudy(*databaseConnection, markedBefore));

    QVERIFY(LocalDatabaseStudyDAL(*databaseConnection).recordRetrieveOutcome(StudyUid, wholeStudyRequested, someImagesMissing));

    QCOMPARE(isStoredAsPartiallyRetrieved(*databaseConnection), markedAfter);
}

void test_LocalDatabaseStudyDAL::update_ShouldNotClearTheMark()
{
    QScopedPointer<DatabaseConnection> databaseConnection(DatabaseTestHelper::getCreatedDatabase());
    QScopedPointer<Patient> patient(insertPatientWithStudy(*databaseConnection, true));

    // Saving a retrieved series updates the study from a new Study object, which knows nothing about the mark.
    Study studyFromNewRetrieve(patient.data());
    studyFromNewRetrieve.setInstanceUID(StudyUid);

    QVERIFY(LocalDatabaseStudyDAL(*databaseConnection).update(&studyFromNewRetrieve, QDate::currentDate()));

    QVERIFY(isStoredAsPartiallyRetrieved(*databaseConnection));
}

void test_LocalDatabaseStudyDAL::queryPatientStudy_ShouldReturnTheMark()
{
    QScopedPointer<DatabaseConnection> databaseConnection(DatabaseTestHelper::getCreatedDatabase());
    QScopedPointer<Patient> patient(insertPatientWithStudy(*databaseConnection, true));

    DicomMask mask;
    mask.setStudyInstanceUID(StudyUid);
    QList<Patient*> patients = LocalDatabaseStudyDAL(*databaseConnection).queryPatientStudy(mask);

    QCOMPARE(patients.size(), 1);
    QVERIFY(patients.at(0)->hasPartiallyRetrievedStudy());

    qDeleteAll(patients);
}

DECLARE_TEST(test_LocalDatabaseStudyDAL)

#include "test_localdatabasestudydal.moc"
