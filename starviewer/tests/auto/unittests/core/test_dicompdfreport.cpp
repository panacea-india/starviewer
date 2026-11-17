/*@
    "name": "test_DicomPdfReport",
    "requirements": []
 */

#include "autotest.h"

#include <QFile>
#include <QTemporaryDir>

#include <dcmtk/dcmdata/dctk.h>

#include "dicompdfreport.h"

using namespace udg;

class test_DicomPdfReport : public QObject {

    Q_OBJECT

private slots:
    void renderPdf_ShouldGiveAPdf();
    void renderPdf_ShouldBeEmptyWithoutText();
    void renderPdf_ShouldMakeMorePagesForLongText();
    void write_ShouldMakeAnEncapsulatedPdfOfTheSameStudy();
    void write_ShouldKeepTheDocumentIntact();
    void write_ShouldFailForBadInput();

private:
    static QString makeImage(const QTemporaryDir &dir);
    static QString value(DcmItem *item, const DcmTagKey &key);
};

QString test_DicomPdfReport::value(DcmItem *item, const DcmTagKey &key)
{
    OFString s;
    item->findAndGetOFString(key, s);
    return QString::fromLatin1(s.c_str());
}

QString test_DicomPdfReport::makeImage(const QTemporaryDir &dir)
{
    DcmFileFormat file;
    DcmDataset *ds = file.getDataset();
    ds->putAndInsertString(DCM_SOPClassUID, "1.2.840.10008.5.1.4.1.1.2");
    ds->putAndInsertString(DCM_SOPInstanceUID, "1.2.3.4.5.100");
    ds->putAndInsertString(DCM_Modality, "CT");
    ds->putAndInsertString(DCM_PatientName, "DOE^JANE");
    ds->putAndInsertString(DCM_PatientID, "12345");
    ds->putAndInsertString(DCM_PatientBirthDate, "19700101");
    ds->putAndInsertString(DCM_PatientSex, "F");
    ds->putAndInsertString(DCM_StudyInstanceUID, "1.2.3.4.5");
    ds->putAndInsertString(DCM_SeriesInstanceUID, "1.2.3.4.5.6");
    ds->putAndInsertString(DCM_StudyDate, "20200315");
    ds->putAndInsertString(DCM_StudyDescription, "Chest");
    ds->putAndInsertString(DCM_AccessionNumber, "ACC1");
    QString path = dir.filePath("image.dcm");
    file.saveFile(path.toLocal8Bit().constData(), EXS_LittleEndianExplicit);
    return path;
}

void test_DicomPdfReport::renderPdf_ShouldGiveAPdf()
{
    DicomPdfReport::Content content;
    content.title = "CT chest";
    content.findings = "No nodules.\nThe lungs are clear.";
    content.impression = "Normal.";
    content.author = "Dr Test";

    QByteArray pdf = DicomPdfReport::renderPdf(content, "DOE JANE   ID 12345");

    QVERIFY(pdf.startsWith("%PDF-"));
    QVERIFY(pdf.size() > 1500);
    QVERIFY(pdf.contains("%%EOF"));
    QVERIFY(pdf.contains("/Type /Page"));
}

void test_DicomPdfReport::renderPdf_ShouldBeEmptyWithoutText()
{
    DicomPdfReport::Content content;
    content.author = "Dr Test";

    QVERIFY(!DicomPdfReport::hasText(content));
    QVERIFY(DicomPdfReport::renderPdf(content, "x").isEmpty());
    content.title = "   ";
    QVERIFY(!DicomPdfReport::hasText(content));
    content.impression = "Normal";
    QVERIFY(DicomPdfReport::hasText(content));
}

void test_DicomPdfReport::renderPdf_ShouldMakeMorePagesForLongText()
{
    DicomPdfReport::Content shortContent;
    shortContent.findings = "A short text.";
    DicomPdfReport::Content longContent;

    for (int i = 0; i < 200; i++)
    {
        longContent.findings += QString("Line %1 of a long finding that goes on and on.\n").arg(i);
    }

    auto pages = [](const QByteArray &pdf) { return pdf.count("/Type /Page\n") + pdf.count("/Type /Page ") + pdf.count("/Type /Page/"); };
    QVERIFY(pages(DicomPdfReport::renderPdf(longContent, "")) > pages(DicomPdfReport::renderPdf(shortContent, "")));
}

void test_DicomPdfReport::write_ShouldMakeAnEncapsulatedPdfOfTheSameStudy()
{
    QTemporaryDir dir;
    QString image = makeImage(dir);
    DicomPdfReport::Content content;
    content.title = "CT chest report";
    content.findings = "Clear.";
    content.impression = "Normal.";
    QString error;

    QVERIFY2(DicomPdfReport::write(image, content, dir.filePath("report.dcm"), &error), qPrintable(error));

    DcmFileFormat report;
    QVERIFY(report.loadFile(dir.filePath("report.dcm").toLocal8Bit().constData()).good());
    DcmDataset *ds = report.getDataset();

    QCOMPARE(value(ds, DCM_SOPClassUID), QString(UID_EncapsulatedPDFStorage));
    QCOMPARE(value(ds, DCM_Modality), QString("DOC"));
    QCOMPARE(value(ds, DCM_PatientName), QString("DOE^JANE"));
    QCOMPARE(value(ds, DCM_PatientID), QString("12345"));
    QCOMPARE(value(ds, DCM_StudyInstanceUID), QString("1.2.3.4.5"));
    QCOMPARE(value(ds, DCM_StudyDate), QString("20200315"));
    QCOMPARE(value(ds, DCM_AccessionNumber), QString("ACC1"));
    QVERIFY(value(ds, DCM_SeriesInstanceUID) != "1.2.3.4.5.6");
    QVERIFY(value(ds, DCM_SOPInstanceUID) != "1.2.3.4.5.100");
    QCOMPARE(value(ds, DCM_MIMETypeOfEncapsulatedDocument), QString("application/pdf"));
    QCOMPARE(value(ds, DCM_DocumentTitle), QString("CT chest report"));
    QCOMPARE(value(ds, DCM_BurnedInAnnotation), QString("YES"));
    QCOMPARE(value(report.getMetaInfo(), DCM_MediaStorageSOPInstanceUID), value(ds, DCM_SOPInstanceUID));
    QCOMPARE(value(report.getMetaInfo(), DCM_MediaStorageSOPClassUID), QString(UID_EncapsulatedPDFStorage));
}

void test_DicomPdfReport::write_ShouldKeepTheDocumentIntact()
{
    QTemporaryDir dir;
    QString image = makeImage(dir);
    DicomPdfReport::Content content;
    content.findings = "Clear.";
    QVERIFY(DicomPdfReport::write(image, content, dir.filePath("report.dcm")));

    DcmFileFormat report;
    QVERIFY(report.loadFile(dir.filePath("report.dcm").toLocal8Bit().constData()).good());
    const Uint8 *bytes = nullptr;
    unsigned long length = 0;
    QVERIFY(report.getDataset()->findAndGetUint8Array(DCM_EncapsulatedDocument, bytes, &length).good());

    QVERIFY(length % 2 == 0);
    QVERIFY(length > 1500);
    QCOMPARE(QByteArray(reinterpret_cast<const char*>(bytes), 5), QByteArray("%PDF-"));
    // The end of the PDF (before the padding, if there is one) is the end marker
    QByteArray tail(reinterpret_cast<const char*>(bytes) + length - 16, 16);
    QVERIFY(tail.contains("%%EOF"));
}

void test_DicomPdfReport::write_ShouldFailForBadInput()
{
    QTemporaryDir dir;
    QString image = makeImage(dir);
    DicomPdfReport::Content content;
    content.findings = "Clear.";
    QString error;

    QVERIFY(!DicomPdfReport::write(image, DicomPdfReport::Content(), dir.filePath("r.dcm"), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!DicomPdfReport::write(dir.filePath("missing.dcm"), content, dir.filePath("r.dcm"), &error));
    QVERIFY(!DicomPdfReport::write(image, content, image, &error));
    QVERIFY(!DicomPdfReport::write(image, content, "", &error));
    QVERIFY(!DicomPdfReport::write(image, content, dir.filePath("no-folder/r.dcm"), &error));
    QVERIFY(!QFile::exists(dir.filePath("r.dcm")));

    // A file that is not DICOM
    QFile notDicom(dir.filePath("x.txt"));
    QVERIFY(notDicom.open(QIODevice::WriteOnly));
    notDicom.write("x");
    notDicom.close();
    QVERIFY(!DicomPdfReport::write(dir.filePath("x.txt"), content, dir.filePath("r.dcm"), &error));
}

DECLARE_TEST(test_DicomPdfReport)

#include "test_dicompdfreport.moc"
