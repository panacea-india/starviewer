/*@
    "name": "test_VtkDcmtkImageReader",
    "requirements": ["archive.dicom_format"]
 */

#include "autotest.h"
#include "vtkdcmtkimagereader.h"

#include <QTemporaryDir>
#include <QVector>

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

#include <dcdeftag.h>
#include <dcfilefo.h>
#include <dcpixel.h>
#include <dcpixseq.h>
#include <dcpxitem.h>
#include <dcuid.h>

using namespace udg;

namespace {

const Uint16 Rows = 2;
const Uint16 Columns = 3;

void insertImageAttributes(DcmDataset *dataset)
{
    dataset->putAndInsertString(DCM_SOPClassUID, UID_SecondaryCaptureImageStorage);
    dataset->putAndInsertString(DCM_SOPInstanceUID, "1.2.826.0.1.3680043.8.498.1");
    dataset->putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME2");
    dataset->putAndInsertUint16(DCM_SamplesPerPixel, 1);
    dataset->putAndInsertUint16(DCM_Rows, Rows);
    dataset->putAndInsertUint16(DCM_Columns, Columns);
    dataset->putAndInsertUint16(DCM_BitsAllocated, 16);
    dataset->putAndInsertUint16(DCM_BitsStored, 16);
    dataset->putAndInsertUint16(DCM_HighBit, 15);
    dataset->putAndInsertUint16(DCM_PixelRepresentation, 0);
}

// Writes an uncompressed image with the given number of pixels, whatever its attributes declare.
QString writeUncompressedFile(const QTemporaryDir &directory, int numberOfPixels)
{
    DcmFileFormat file;
    insertImageAttributes(file.getDataset());

    QVector<Uint16> pixels(numberOfPixels);

    for (int i = 0; i < numberOfPixels; i++)
    {
        pixels[i] = static_cast<Uint16>(100 + i);
    }

    file.getDataset()->putAndInsertUint16Array(DCM_PixelData, pixels.constData(), numberOfPixels);

    QString path = directory.filePath("uncompressed.dcm");
    return file.saveFile(qPrintable(path), EXS_LittleEndianExplicit).good() ? path : QString();
}

// Writes a JPEG-LS file. Nothing in the tests registers a JPEG-LS decoder, so it can't be decoded, as with any frame that fails to decode.
QString writeUndecodableFile(const QTemporaryDir &directory)
{
    DcmFileFormat file;
    insertImageAttributes(file.getDataset());

    const Uint8 fragmentData[4] = { 0xFF, 0xD8, 0xFF, 0xD9 };
    DcmPixelItem *fragment = new DcmPixelItem(DCM_PixelItemTag);
    fragment->putUint8Array(fragmentData, 4);

    DcmPixelSequence *sequence = new DcmPixelSequence(DCM_PixelSequenceTag);
    sequence->insert(new DcmPixelItem(DCM_PixelItemTag));
    sequence->insert(fragment);

    DcmPixelData *pixelData = new DcmPixelData(DCM_PixelData);
    pixelData->putOriginalRepresentation(EXS_JPEGLSLossless, nullptr, sequence);
    file.getDataset()->insert(pixelData);

    QString path = directory.filePath("undecodable.dcm");
    return file.saveFile(qPrintable(path), EXS_JPEGLSLossless).good() ? path : QString();
}

}

class test_VtkDcmtkImageReader : public QObject {

    Q_OBJECT

private slots:

    void update_ShouldReadImage_WhenPixelDataIsComplete();
    void update_ShouldThrow_WhenPixelDataIsShorterThanDeclared();
    void update_ShouldThrow_WhenFrameCannotBeDecoded();
};

void test_VtkDcmtkImageReader::update_ShouldReadImage_WhenPixelDataIsComplete()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QString path = writeUncompressedFile(directory, Rows * Columns);
    QVERIFY(!path.isEmpty());

    vtkSmartPointer<VtkDcmtkImageReader> reader = vtkSmartPointer<VtkDcmtkImageReader>::New();
    reader->SetFileName(qPrintable(path));
    reader->Update();

    double range[2];
    reader->GetOutput()->GetScalarRange(range);
    QCOMPARE(range[0], 100.0);
    QCOMPARE(range[1], 105.0);
}

void test_VtkDcmtkImageReader::update_ShouldThrow_WhenPixelDataIsShorterThanDeclared()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QString path = writeUncompressedFile(directory, Columns);
    QVERIFY(!path.isEmpty());

    vtkSmartPointer<VtkDcmtkImageReader> reader = vtkSmartPointer<VtkDcmtkImageReader>::New();
    reader->SetFileName(qPrintable(path));

    QVERIFY_EXCEPTION_THROWN(reader->Update(), VtkDcmtkImageReader::CantReadImageException);
}

void test_VtkDcmtkImageReader::update_ShouldThrow_WhenFrameCannotBeDecoded()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QString path = writeUndecodableFile(directory);
    QVERIFY(!path.isEmpty());

    vtkSmartPointer<VtkDcmtkImageReader> reader = vtkSmartPointer<VtkDcmtkImageReader>::New();
    reader->SetFileName(qPrintable(path));

    QVERIFY_EXCEPTION_THROWN(reader->Update(), VtkDcmtkImageReader::CantReadImageException);
}

DECLARE_TEST(test_VtkDcmtkImageReader)

#include "test_vtkdcmtkimagereader.moc"
