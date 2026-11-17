/*@
    "name": "test_StructuredReport",
    "requirements": []
 */

#include "autotest.h"
#include "structuredreport.h"
#include "structuredreportrenderer.h"

#include "dicomtagreader.h"
#include "study.h"

#include <QTemporaryDir>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmsr/dsrdoc.h>

using namespace udg;

Q_DECLARE_METATYPE(DcmDataset*)

namespace {

const char* const BasicTextSrClass = "1.2.840.10008.5.1.4.1.1.88.11";

DcmDataset* createDataset(const char *sopClass, const char *title = "Radiology Report", const char *completion = "COMPLETE",
                          const char *verification = "VERIFIED")
{
    DcmDataset *dataset = new DcmDataset;
    dataset->putAndInsertString(DCM_SOPClassUID, sopClass);
    dataset->putAndInsertString(DCM_SOPInstanceUID, "1.2.3.55");
    dataset->putAndInsertString(DCM_ContentDate, "20260101");
    dataset->putAndInsertString(DCM_CompletionFlag, completion);
    dataset->putAndInsertString(DCM_VerificationFlag, verification);

    if (title)
    {
        DcmItem *concept = nullptr;
        dataset->findOrCreateSequenceItem(DCM_ConceptNameCodeSequence, concept, -2);
        concept->putAndInsertString(DCM_CodeValue, "11528-7");
        concept->putAndInsertString(DCM_CodingSchemeDesignator, "LN");
        concept->putAndInsertString(DCM_CodeMeaning, title);
    }

    return dataset;
}

StructuredReport read(DcmDataset *dataset, QString *error = nullptr)
{
    DICOMTagReader reader;
    reader.setDcmDataset("report.dcm", dataset);
    return StructuredReport::fromDICOM(reader, error);
}

// Writes a Basic Text SR with one finding. Returns the path, or an empty string if it could not be written.
QString writeReport(const QString &directory, const QString &finding)
{
    DSRDocument document(DSRTypes::DT_BasicTextSR);
    document.setPatientName("TEST^PATIENT");
    document.setPatientID("TEST-1");
    document.setSpecificCharacterSetType(DSRTypes::CS_Latin1);

    DSRDocumentTree &tree = document.getTree();
    tree.addContentItem(DSRTypes::RT_isRoot, DSRTypes::VT_Container);
    tree.getCurrentContentItem().setConceptName(DSRCodedEntryValue("11528-7", "LN", "Radiology Report"));
    tree.addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Text, DSRTypes::AM_belowCurrent);
    tree.getCurrentContentItem().setConceptName(DSRCodedEntryValue("121070", "DCM", "Findings"));
    tree.getCurrentContentItem().setStringValue(finding.toLatin1().constData());

    document.completeDocument();

    DcmFileFormat file;
    OFCondition status = document.write(*file.getDataset());

    if (status.bad())
    {
        return QString();
    }

    QString path = directory + "/report.dcm";
    status = file.saveFile(path.toLocal8Bit().constData(), EXS_LittleEndianExplicit);
    return status.good() ? path : QString();
}

}

class test_StructuredReport : public QObject {

    Q_OBJECT

private slots:
    void isStructuredReportClass_ShouldBeTrueForReportsAndFalseForKeyObjectsAndImages_data();
    void isStructuredReportClass_ShouldBeTrueForReportsAndFalseForKeyObjectsAndImages();

    void fromDICOM_ShouldReadTheTitleTheFlagsAndThePath();
    void fromDICOM_ShouldReturnNullAndSayWhy_data();
    void fromDICOM_ShouldReturnNullAndSayWhy();

    void isFinal_ShouldBeTrueOnlyForACompleteAndVerifiedReport_data();
    void isFinal_ShouldBeTrueOnlyForACompleteAndVerifiedReport();

    void addStructuredReport_ShouldNotAddANullOrARepeatedReport();

    void toHtml_ShouldRenderTheContentOfTheReport();
    void toHtml_ShouldEscapeTheTextOfTheReport();
    void toHtml_ShouldReturnAnEmptyStringAndSayWhy_WhenTheFileIsNotAReport();
};

void test_StructuredReport::isStructuredReportClass_ShouldBeTrueForReportsAndFalseForKeyObjectsAndImages_data()
{
    QTest::addColumn<QString>("sopClass");
    QTest::addColumn<bool>("expected");

    QTest::newRow("Basic Text SR") << "1.2.840.10008.5.1.4.1.1.88.11" << true;
    QTest::newRow("Enhanced SR") << "1.2.840.10008.5.1.4.1.1.88.22" << true;
    QTest::newRow("Comprehensive SR") << "1.2.840.10008.5.1.4.1.1.88.33" << true;
    QTest::newRow("X-Ray Radiation Dose SR") << "1.2.840.10008.5.1.4.1.1.88.67" << true;
    QTest::newRow("Key Object Selection") << "1.2.840.10008.5.1.4.1.1.88.59" << false;
    QTest::newRow("CT image") << "1.2.840.10008.5.1.4.1.1.2" << false;
    QTest::newRow("nothing") << "" << false;
}

void test_StructuredReport::isStructuredReportClass_ShouldBeTrueForReportsAndFalseForKeyObjectsAndImages()
{
    QFETCH(QString, sopClass);
    QFETCH(bool, expected);

    QCOMPARE(StructuredReport::isStructuredReportClass(sopClass), expected);
}

void test_StructuredReport::fromDICOM_ShouldReadTheTitleTheFlagsAndThePath()
{
    StructuredReport report = read(createDataset(BasicTextSrClass, "Radiology Report", "PARTIAL", "UNVERIFIED"));

    QVERIFY(!report.isNull());
    QCOMPARE(report.getSopInstanceUid(), QString("1.2.3.55"));
    QCOMPARE(report.getSopClassUid(), QString(BasicTextSrClass));
    QCOMPARE(report.getTitle(), QString("Radiology Report"));
    QCOMPARE(report.getCompletionFlag(), QString("PARTIAL"));
    QCOMPARE(report.getVerificationFlag(), QString("UNVERIFIED"));
    QCOMPARE(report.getContentDate(), QString("20260101"));
    QCOMPARE(report.getPath(), QString("report.dcm"));
}

void test_StructuredReport::fromDICOM_ShouldReturnNullAndSayWhy_data()
{
    QTest::addColumn<DcmDataset*>("dataset");

    QTest::newRow("a key object selection") << createDataset("1.2.840.10008.5.1.4.1.1.88.59");
    QTest::newRow("an image") << createDataset("1.2.840.10008.5.1.4.1.1.2");
}

void test_StructuredReport::fromDICOM_ShouldReturnNullAndSayWhy()
{
    QFETCH(DcmDataset*, dataset);

    QString error;
    StructuredReport report = read(dataset, &error);

    QVERIFY(report.isNull());
    QCOMPARE(error, QString("It is not a Structured Report."));
}

void test_StructuredReport::isFinal_ShouldBeTrueOnlyForACompleteAndVerifiedReport_data()
{
    QTest::addColumn<QString>("completion");
    QTest::addColumn<QString>("verification");
    QTest::addColumn<bool>("expected");

    QTest::newRow("complete and verified") << "COMPLETE" << "VERIFIED" << true;
    QTest::newRow("complete and unverified") << "COMPLETE" << "UNVERIFIED" << false;
    QTest::newRow("partial and verified") << "PARTIAL" << "VERIFIED" << false;
    QTest::newRow("no flags") << "" << "" << false;
}

void test_StructuredReport::isFinal_ShouldBeTrueOnlyForACompleteAndVerifiedReport()
{
    QFETCH(QString, completion);
    QFETCH(QString, verification);
    QFETCH(bool, expected);

    DcmDataset *dataset = createDataset(BasicTextSrClass, "Radiology Report", qPrintable(completion), qPrintable(verification));

    QCOMPARE(read(dataset).isFinal(), expected);
}

void test_StructuredReport::addStructuredReport_ShouldNotAddANullOrARepeatedReport()
{
    Study study;
    StructuredReport report = read(createDataset(BasicTextSrClass));

    QVERIFY(!study.addStructuredReport(StructuredReport()));
    QVERIFY(study.addStructuredReport(report));
    QVERIFY(!study.addStructuredReport(report));
    QCOMPARE(study.getStructuredReports().size(), 1);
}

void test_StructuredReport::toHtml_ShouldRenderTheContentOfTheReport()
{
    QTemporaryDir directory;
    QString path = writeReport(directory.path(), "No acute findings.");
    QVERIFY(!path.isEmpty());

    QString error;
    QString html = StructuredReportRenderer::toHtml(path, &error);

    QVERIFY2(!html.isEmpty(), qPrintable(error));
    QVERIFY(html.contains("Radiology Report"));
    QVERIFY(html.contains("Findings"));
    QVERIFY(html.contains("No acute findings."));
    QVERIFY(html.contains("TEST"));
}

void test_StructuredReport::toHtml_ShouldEscapeTheTextOfTheReport()
{
    QTemporaryDir directory;
    QString path = writeReport(directory.path(), "<script>alert(1)</script> & <b>bold</b>");
    QVERIFY(!path.isEmpty());

    QString html = StructuredReportRenderer::toHtml(path);

    QVERIFY(!html.isEmpty());
    QVERIFY(!html.contains("<script>"));
    QVERIFY(!html.contains("<b>bold</b>"));
    QVERIFY(html.contains("&lt;script&gt;"));
}

void test_StructuredReport::toHtml_ShouldReturnAnEmptyStringAndSayWhy_WhenTheFileIsNotAReport()
{
    QTemporaryDir directory;
    QString path = directory.path() + "/not-a-report.dcm";
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("this is not DICOM");
    file.close();

    QString error;

    QVERIFY(StructuredReportRenderer::toHtml(path, &error).isEmpty());
    QVERIFY(!error.isEmpty());
}

DECLARE_TEST(test_StructuredReport)

#include "test_structuredreport.moc"
