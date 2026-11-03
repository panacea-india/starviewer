/*@
    "name": "test_PixelDataLengthCheck",
    "requirements": ["archive.dicom_format"]
 */

#include "autotest.h"
#include "pixeldatalengthcheck.h"

#include <QFile>
#include <QTemporaryDir>
#include <QVector>

#include <dcdeftag.h>
#include <dcfilefo.h>
#include <dcuid.h>

using namespace udg;

namespace {

// Large enough that the pixel data is longer than what checkFile() loads, so that only its length is read.
const Uint16 Rows = 64;
const Uint16 Columns = 64;

// Writes an uncompressed 16-bit image of Rows x Columns with the given number of pixels, whatever its attributes declare.
QString writeUncompressedFile(const QTemporaryDir &directory, int numberOfPixels, int numberOfFrames = 1)
{
    DcmFileFormat file;
    DcmDataset *dataset = file.getDataset();
    dataset->putAndInsertString(DCM_SOPClassUID, UID_SecondaryCaptureImageStorage);
    dataset->putAndInsertString(DCM_SOPInstanceUID, "1.2.826.0.1.3680043.8.498.2");
    dataset->putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME2");
    dataset->putAndInsertUint16(DCM_SamplesPerPixel, 1);
    dataset->putAndInsertUint16(DCM_Rows, Rows);
    dataset->putAndInsertUint16(DCM_Columns, Columns);
    dataset->putAndInsertUint16(DCM_BitsAllocated, 16);
    dataset->putAndInsertUint16(DCM_BitsStored, 16);
    dataset->putAndInsertUint16(DCM_HighBit, 15);
    dataset->putAndInsertUint16(DCM_PixelRepresentation, 0);

    if (numberOfFrames > 1)
    {
        dataset->putAndInsertString(DCM_NumberOfFrames, qPrintable(QString::number(numberOfFrames)));
    }

    QVector<Uint16> pixels(numberOfPixels, 100);
    dataset->putAndInsertUint16Array(DCM_PixelData, pixels.constData(), numberOfPixels);

    QString path = directory.filePath("uncompressed.dcm");
    return file.saveFile(qPrintable(path), EXS_LittleEndianExplicit).good() ? path : QString();
}

}

class test_PixelDataLengthCheck : public QObject {

    Q_OBJECT

private slots:

    void checkFile_ShouldAccept_WhenPixelDataIsComplete();
    void checkFile_ShouldReject_WhenPixelDataIsShorterThanDeclared();
    void checkFile_ShouldReject_WhenFramesAreMissing();
    void checkFile_ShouldAccept_WhenFileIsNotDicom();
};

void test_PixelDataLengthCheck::checkFile_ShouldAccept_WhenPixelDataIsComplete()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QString path = writeUncompressedFile(directory, Rows * Columns);
    QVERIFY(!path.isEmpty());

    QCOMPARE(PixelDataLengthCheck::checkFile(path), QString());
}

void test_PixelDataLengthCheck::checkFile_ShouldReject_WhenPixelDataIsShorterThanDeclared()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QString path = writeUncompressedFile(directory, Rows * Columns / 2);
    QVERIFY(!path.isEmpty());

    QString problem = PixelDataLengthCheck::checkFile(path);
    QVERIFY2(problem.contains("4096 bytes") && problem.contains("8192"), qPrintable(problem));
}

void test_PixelDataLengthCheck::checkFile_ShouldReject_WhenFramesAreMissing()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QString path = writeUncompressedFile(directory, Rows * Columns * 2, 3);
    QVERIFY(!path.isEmpty());

    QVERIFY(!PixelDataLengthCheck::checkFile(path).isEmpty());
}

void test_PixelDataLengthCheck::checkFile_ShouldAccept_WhenFileIsNotDicom()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QString path = directory.filePath("image.png");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("not DICOM");
    file.close();

    QCOMPARE(PixelDataLengthCheck::checkFile(path), QString());
}

DECLARE_TEST(test_PixelDataLengthCheck)

#include "test_pixeldatalengthcheck.moc"
