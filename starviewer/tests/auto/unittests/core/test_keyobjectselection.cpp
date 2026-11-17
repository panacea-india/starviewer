/*@
    "name": "test_KeyObjectSelection",
    "requirements": []
 */

#include "autotest.h"
#include "keyobjectselection.h"

#include "dicomtagreader.h"
#include "study.h"

#include <dcmtk/dcmdata/dctk.h>

using namespace udg;

Q_DECLARE_METATYPE(DcmDataset*)

namespace {

const char* const KeyObjectSelectionClass = "1.2.840.10008.5.1.4.1.1.88.59";

struct ReferenceData {
    const char *sopInstanceUid;
    const char *frames;
};

// Creates the dataset of a Key Object Selection Document. The caller (the DICOMTagReader) owns it.
DcmDataset* createDataset(const char *sopClass, const char *titleCode, const char *scheme, const char *meaning, const QList<ReferenceData> &references,
                          const char *note = nullptr)
{
    DcmDataset *dataset = new DcmDataset;
    dataset->putAndInsertString(DCM_SOPClassUID, sopClass);
    dataset->putAndInsertString(DCM_SOPInstanceUID, "1.2.3.99");

    if (titleCode)
    {
        DcmItem *title = nullptr;
        dataset->findOrCreateSequenceItem(DCM_ConceptNameCodeSequence, title, -2);
        title->putAndInsertString(DCM_CodeValue, titleCode);
        title->putAndInsertString(DCM_CodingSchemeDesignator, scheme);
        title->putAndInsertString(DCM_CodeMeaning, meaning);
    }

    for (const ReferenceData &reference : references)
    {
        DcmItem *content = nullptr;
        dataset->findOrCreateSequenceItem(DCM_ContentSequence, content, -2);
        content->putAndInsertString(DCM_RelationshipType, "CONTAINS");
        content->putAndInsertString(DCM_ValueType, "IMAGE");

        DcmItem *referenced = nullptr;
        content->findOrCreateSequenceItem(DCM_ReferencedSOPSequence, referenced, -2);
        referenced->putAndInsertString(DCM_ReferencedSOPClassUID, "1.2.840.10008.5.1.4.1.1.2");
        referenced->putAndInsertString(DCM_ReferencedSOPInstanceUID, reference.sopInstanceUid);

        if (reference.frames)
        {
            referenced->putAndInsertString(DCM_ReferencedFrameNumber, reference.frames);
        }
    }

    if (note)
    {
        DcmItem *content = nullptr;
        dataset->findOrCreateSequenceItem(DCM_ContentSequence, content, -2);
        content->putAndInsertString(DCM_RelationshipType, "CONTAINS");
        content->putAndInsertString(DCM_ValueType, "TEXT");
        content->putAndInsertString(DCM_TextValue, note);

        DcmItem *concept = nullptr;
        content->findOrCreateSequenceItem(DCM_ConceptNameCodeSequence, concept, -2);
        concept->putAndInsertString(DCM_CodeValue, "113012");
        concept->putAndInsertString(DCM_CodingSchemeDesignator, "DCM");
        concept->putAndInsertString(DCM_CodeMeaning, "Key Object Description");
    }

    return dataset;
}

KeyObjectSelection read(DcmDataset *dataset, QString *error = nullptr)
{
    DICOMTagReader reader;
    reader.setDcmDataset("", dataset);
    return KeyObjectSelection::fromDICOM(reader, error);
}

}

class test_KeyObjectSelection : public QObject {

    Q_OBJECT

private slots:
    void fromDICOM_ShouldReadTheTitleTheReferencesAndTheNote();
    void fromDICOM_ShouldReadTheFramesOfAReference();
    void fromDICOM_ShouldReturnNullAndSayWhy_data();
    void fromDICOM_ShouldReturnNullAndSayWhy();

    void getKind_ShouldFollowTheTitle_data();
    void getKind_ShouldFollowTheTitle();

    void getDisplayText_ShouldNameTheKindAndTheTitle_data();
    void getDisplayText_ShouldNameTheKindAndTheTitle();

    void references_ShouldBeTrueOnlyForTheReferencedInstances();
    void getUnresolvedReferences_ShouldReturnTheReferencesThatAreNotAvailable();

    void addKeyObjectSelection_ShouldNotAddANullOrARepeatedDocument();
    void getKeyObjectSelectionsReferencing_ShouldReturnTheDocumentsThatReferTheInstance();
};

void test_KeyObjectSelection::fromDICOM_ShouldReadTheTitleTheReferencesAndTheNote()
{
    KeyObjectSelection selection = read(createDataset(KeyObjectSelectionClass, "113000", "DCM", "Of Interest", {{"1.1", nullptr}, {"1.2", nullptr}}, "Look here"));

    QVERIFY(!selection.isNull());
    QCOMPARE(selection.getSopInstanceUid(), QString("1.2.3.99"));
    QCOMPARE(selection.getTitleCodeValue(), QString("113000"));
    QCOMPARE(selection.getTitleMeaning(), QString("Of Interest"));
    QCOMPARE(selection.getNote(), QString("Look here"));
    QCOMPARE(selection.getReferences().size(), 2);
    QCOMPARE(selection.getReferences().at(0).sopInstanceUid, QString("1.1"));
    QCOMPARE(selection.getReferences().at(0).sopClassUid, QString("1.2.840.10008.5.1.4.1.1.2"));
    QCOMPARE(selection.getReferences().at(1).sopInstanceUid, QString("1.2"));
}

void test_KeyObjectSelection::fromDICOM_ShouldReadTheFramesOfAReference()
{
    KeyObjectSelection selection = read(createDataset(KeyObjectSelectionClass, "113000", "DCM", "Of Interest", {{"1.1", "2\\5"}, {"1.2", nullptr}}));

    QCOMPARE(selection.getReferences().at(0).frames, QList<int>({2, 5}));
    QVERIFY(selection.getReferences().at(1).frames.isEmpty());
}

void test_KeyObjectSelection::fromDICOM_ShouldReturnNullAndSayWhy_data()
{
    QTest::addColumn<DcmDataset*>("dataset");
    QTest::addColumn<QString>("expectedError");

    QTest::newRow("another kind of document") << createDataset("1.2.840.10008.5.1.4.1.1.2", "113000", "DCM", "Of Interest", {{"1.1", nullptr}})
                                              << QString("It is not a Key Object Selection Document.");
    QTest::newRow("no images") << createDataset(KeyObjectSelectionClass, "113000", "DCM", "Of Interest", {})
                               << QString("The Key Object Selection Document does not refer to any image.");
}

void test_KeyObjectSelection::fromDICOM_ShouldReturnNullAndSayWhy()
{
    QFETCH(DcmDataset*, dataset);
    QFETCH(QString, expectedError);

    QString error;
    KeyObjectSelection selection = read(dataset, &error);

    QVERIFY(selection.isNull());
    QCOMPARE(error, expectedError);
}

void test_KeyObjectSelection::getKind_ShouldFollowTheTitle_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("scheme");
    QTest::addColumn<int>("expectedKind");

    int selection = static_cast<int>(KeyObjectSelection::Kind::Selection);
    int rejection = static_cast<int>(KeyObjectSelection::Kind::Rejection);
    int manifest = static_cast<int>(KeyObjectSelection::Kind::Manifest);

    QTest::newRow("Of Interest") << "113000" << "DCM" << selection;
    QTest::newRow("For Teaching") << "113004" << "DCM" << selection;
    QTest::newRow("Rejected for Quality Reasons") << "113001" << "DCM" << rejection;
    QTest::newRow("Rejected for Patient Safety Reasons") << "113037" << "DCM" << rejection;
    QTest::newRow("Incorrect Modality Worklist Entry") << "113038" << "DCM" << rejection;
    QTest::newRow("Data Retention Policy Expired") << "113039" << "DCM" << rejection;
    QTest::newRow("Manifest") << "113030" << "DCM" << manifest;
    QTest::newRow("Complete Study Content") << "113032" << "DCM" << manifest;
    QTest::newRow("a title of another scheme with a rejection code") << "113001" << "99LOCAL" << selection;
    QTest::newRow("an unknown title") << "999999" << "DCM" << selection;
}

void test_KeyObjectSelection::getKind_ShouldFollowTheTitle()
{
    QFETCH(QString, code);
    QFETCH(QString, scheme);
    QFETCH(int, expectedKind);

    QCOMPARE(static_cast<int>(KeyObjectSelection::getKindOfTitle(code, scheme)), expectedKind);

    KeyObjectSelection selection = read(createDataset(KeyObjectSelectionClass, qPrintable(code), qPrintable(scheme), "Title", {{"1.1", nullptr}}));
    QCOMPARE(static_cast<int>(selection.getKind()), expectedKind);
}

void test_KeyObjectSelection::getDisplayText_ShouldNameTheKindAndTheTitle_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("meaning");
    QTest::addColumn<QString>("expectedText");

    QTest::newRow("selection") << "113000" << "Of Interest" << "KEY IMAGE: Of Interest";
    QTest::newRow("rejection") << "113001" << "Rejected for Quality Reasons" << "REJECTED: Rejected for Quality Reasons";
    QTest::newRow("manifest shows nothing") << "113030" << "Manifest" << "";
    QTest::newRow("selection without a meaning shows the code") << "113013" << "" << "KEY IMAGE: 113013";
}

void test_KeyObjectSelection::getDisplayText_ShouldNameTheKindAndTheTitle()
{
    QFETCH(QString, code);
    QFETCH(QString, meaning);
    QFETCH(QString, expectedText);

    KeyObjectSelection selection = read(createDataset(KeyObjectSelectionClass, qPrintable(code), "DCM", qPrintable(meaning), {{"1.1", nullptr}}));

    QCOMPARE(selection.getDisplayText(), expectedText);
}

void test_KeyObjectSelection::references_ShouldBeTrueOnlyForTheReferencedInstances()
{
    KeyObjectSelection selection = read(createDataset(KeyObjectSelectionClass, "113000", "DCM", "Of Interest", {{"1.1", nullptr}, {"1.2", nullptr}}));

    QVERIFY(selection.references("1.1"));
    QVERIFY(selection.references("1.2"));
    QVERIFY(!selection.references("1.3"));
}

void test_KeyObjectSelection::getUnresolvedReferences_ShouldReturnTheReferencesThatAreNotAvailable()
{
    KeyObjectSelection selection = read(createDataset(KeyObjectSelectionClass, "113000", "DCM", "Of Interest", {{"1.1", nullptr}, {"1.2", nullptr}, {"1.3", nullptr}}));

    QList<KeyObjectSelection::Reference> unresolved = selection.getUnresolvedReferences({"1.1", "1.3", "9.9"});

    QCOMPARE(unresolved.size(), 1);
    QCOMPARE(unresolved.first().sopInstanceUid, QString("1.2"));
    QVERIFY(selection.getUnresolvedReferences({"1.1", "1.2", "1.3"}).isEmpty());
}

void test_KeyObjectSelection::addKeyObjectSelection_ShouldNotAddANullOrARepeatedDocument()
{
    Study study;
    KeyObjectSelection selection = read(createDataset(KeyObjectSelectionClass, "113000", "DCM", "Of Interest", {{"1.1", nullptr}}));

    QVERIFY(!study.addKeyObjectSelection(KeyObjectSelection()));
    QVERIFY(study.addKeyObjectSelection(selection));
    QVERIFY(!study.addKeyObjectSelection(selection));
    QCOMPARE(study.getKeyObjectSelections().size(), 1);
}

void test_KeyObjectSelection::getKeyObjectSelectionsReferencing_ShouldReturnTheDocumentsThatReferTheInstance()
{
    Study study;
    DcmDataset *first = createDataset(KeyObjectSelectionClass, "113000", "DCM", "Of Interest", {{"1.1", nullptr}, {"1.2", nullptr}});
    first->putAndInsertString(DCM_SOPInstanceUID, "5.1");
    DcmDataset *second = createDataset(KeyObjectSelectionClass, "113001", "DCM", "Rejected for Quality Reasons", {{"1.2", nullptr}});
    second->putAndInsertString(DCM_SOPInstanceUID, "5.2");
    study.addKeyObjectSelection(read(first));
    study.addKeyObjectSelection(read(second));

    QCOMPARE(study.getKeyObjectSelectionsReferencing("1.1").size(), 1);
    QCOMPARE(study.getKeyObjectSelectionsReferencing("1.2").size(), 2);
    QVERIFY(study.getKeyObjectSelectionsReferencing("1.9").isEmpty());
}

DECLARE_TEST(test_KeyObjectSelection)

#include "test_keyobjectselection.moc"
