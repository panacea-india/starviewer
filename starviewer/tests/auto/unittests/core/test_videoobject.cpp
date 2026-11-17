/*@
    "name": "test_VideoObject",
    "requirements": []
 */

#include "autotest.h"
#include "videoobject.h"

#include "dicomtagreader.h"
#include "study.h"

#include <QFile>
#include <QTemporaryDir>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmdata/dcpixseq.h>
#include <dcmtk/dcmdata/dcpxitem.h>

using namespace udg;

namespace {

// Not a real video, but what the extraction has to give back, byte by byte
const QByteArray FakeBitstream = QByteArray::fromHex("000001B3140010FFFFE0182000000100000102030405060708090A0B0C0D0E0F");

// Writes a DICOM file with the given transfer syntax, whose pixel data is encapsulated in a basic offset table and the given fragments.
QString writeVideo(const QString &directory, E_TransferSyntax syntax, const QList<QByteArray> &fragments, const char *frameTime = "40")
{
    DcmFileFormat file;
    DcmDataset *dataset = file.getDataset();
    dataset->putAndInsertString(DCM_SOPClassUID, UID_VideoPhotographicImageStorage);
    dataset->putAndInsertString(DCM_SOPInstanceUID, "1.2.3.88");
    dataset->putAndInsertString(DCM_SeriesDescription, "Skin video");
    dataset->putAndInsertUint16(DCM_Rows, 240);
    dataset->putAndInsertUint16(DCM_Columns, 320);
    dataset->putAndInsertString(DCM_NumberOfFrames, "50");

    if (frameTime)
    {
        dataset->putAndInsertString(DCM_FrameTime, frameTime);
    }

    DcmPixelSequence *sequence = new DcmPixelSequence(DcmTag(DCM_PixelData, EVR_OB));
    sequence->insert(new DcmPixelItem(DcmTag(DCM_Item, EVR_OB)));

    for (const QByteArray &fragment : fragments)
    {
        DcmPixelItem *item = new DcmPixelItem(DcmTag(DCM_Item, EVR_OB));
        item->putUint8Array(reinterpret_cast<const Uint8*>(fragment.constData()), fragment.size());
        sequence->insert(item);
    }

    DcmPixelData *pixelData = new DcmPixelData(DcmTag(DCM_PixelData, EVR_OB));
    pixelData->putOriginalRepresentation(syntax, nullptr, sequence);
    dataset->insert(pixelData);

    QString path = directory + "/video.dcm";
    OFCondition status = file.saveFile(path.toLocal8Bit().constData(), syntax);
    return status.good() ? path : QString();
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

}

class test_VideoObject : public QObject {

    Q_OBJECT

private slots:
    void isVideoTransferSyntax_ShouldBeTrueOnlyForTheVideoSyntaxes_data();
    void isVideoTransferSyntax_ShouldBeTrueOnlyForTheVideoSyntaxes();

    void getFileExtensionAndFormatName_ShouldFollowTheSyntax_data();
    void getFileExtensionAndFormatName_ShouldFollowTheSyntax();

    void fromDICOM_ShouldReadWhatDescribesTheVideo();
    void fromDICOM_ShouldUseTheCineRate_WhenThereIsNoFrameTime();
    void fromDICOM_ShouldReturnNullAndSayWhy_ForAFileThatIsNotAVideo();

    void extractTo_ShouldWriteTheBitstreamOfTheFragments();
    void extractTo_ShouldFailAndSayWhy_ForANullVideo();
    void extractTo_ShouldFailAndSayWhy_WhenThereIsNoData();

    void addVideo_ShouldNotAddANullOrARepeatedVideo();
};

void test_VideoObject::isVideoTransferSyntax_ShouldBeTrueOnlyForTheVideoSyntaxes_data()
{
    QTest::addColumn<QString>("syntax");
    QTest::addColumn<bool>("expected");

    QTest::newRow("MPEG-2 MP@ML") << "1.2.840.10008.1.2.4.100" << true;
    QTest::newRow("MPEG-2 MP@HL") << "1.2.840.10008.1.2.4.101" << true;
    QTest::newRow("H.264 4.1") << "1.2.840.10008.1.2.4.102" << true;
    QTest::newRow("H.264 BD") << "1.2.840.10008.1.2.4.103" << true;
    QTest::newRow("HEVC Main") << "1.2.840.10008.1.2.4.107" << true;
    QTest::newRow("HEVC Main 10") << "1.2.840.10008.1.2.4.108" << true;
    QTest::newRow("JPEG 2000 lossless") << "1.2.840.10008.1.2.4.90" << false;
    QTest::newRow("JPEG baseline") << "1.2.840.10008.1.2.4.50" << false;
    QTest::newRow("RLE") << "1.2.840.10008.1.2.5" << false;
    QTest::newRow("one after the last") << "1.2.840.10008.1.2.4.109" << false;
    QTest::newRow("one before the first") << "1.2.840.10008.1.2.4.99" << false;
    QTest::newRow("explicit little endian") << "1.2.840.10008.1.2.1" << false;
    QTest::newRow("nothing") << "" << false;
}

void test_VideoObject::isVideoTransferSyntax_ShouldBeTrueOnlyForTheVideoSyntaxes()
{
    QFETCH(QString, syntax);
    QFETCH(bool, expected);

    QCOMPARE(VideoObject::isVideoTransferSyntax(syntax), expected);
}

void test_VideoObject::getFileExtensionAndFormatName_ShouldFollowTheSyntax_data()
{
    QTest::addColumn<QString>("syntax");
    QTest::addColumn<QString>("extension");
    QTest::addColumn<QString>("format");

    QTest::newRow("MPEG-2 MP@ML") << "1.2.840.10008.1.2.4.100" << ".mpg" << "MPEG-2";
    QTest::newRow("MPEG-2 MP@HL") << "1.2.840.10008.1.2.4.101" << ".mpg" << "MPEG-2";
    QTest::newRow("H.264 4.1") << "1.2.840.10008.1.2.4.102" << ".mp4" << "MPEG-4 AVC/H.264";
    QTest::newRow("H.264 stereo") << "1.2.840.10008.1.2.4.106" << ".mp4" << "MPEG-4 AVC/H.264";
    QTest::newRow("HEVC") << "1.2.840.10008.1.2.4.107" << ".mp4" << "HEVC/H.265";
    QTest::newRow("not a video") << "1.2.840.10008.1.2.4.50" << "" << "";
}

void test_VideoObject::getFileExtensionAndFormatName_ShouldFollowTheSyntax()
{
    QFETCH(QString, syntax);
    QFETCH(QString, extension);
    QFETCH(QString, format);

    QCOMPARE(VideoObject::getFileExtension(syntax), extension);
    QCOMPARE(VideoObject::getFormatName(syntax), format);
}

void test_VideoObject::fromDICOM_ShouldReadWhatDescribesTheVideo()
{
    QTemporaryDir directory;
    QString path = writeVideo(directory.path(), EXS_MPEG4HighProfileLevel4_1, {FakeBitstream});
    QVERIFY(!path.isEmpty());

    DICOMTagReader reader(path);
    VideoObject video = VideoObject::fromDICOM(reader);

    QVERIFY(!video.isNull());
    QCOMPARE(video.getSopInstanceUid(), QString("1.2.3.88"));
    QCOMPARE(video.getTransferSyntaxUid(), QString("1.2.840.10008.1.2.4.102"));
    QCOMPARE(video.getFormatName(), QString("MPEG-4 AVC/H.264"));
    QCOMPARE(video.getDescription(), QString("Skin video"));
    QCOMPARE(video.getRows(), 240);
    QCOMPARE(video.getColumns(), 320);
    QCOMPARE(video.getNumberOfFrames(), 50);
    // 50 frames of 40 ms
    QVERIFY(qAbs(video.getDurationInSeconds() - 2.0) < 1e-9);
    QCOMPARE(video.getPath(), path);
}

void test_VideoObject::fromDICOM_ShouldUseTheCineRate_WhenThereIsNoFrameTime()
{
    QTemporaryDir directory;
    // Written without a frame time, and with a cine rate added afterwards
    QString path = writeVideo(directory.path(), EXS_MPEG2MainProfileAtMainLevel, {FakeBitstream}, nullptr);
    QVERIFY(!path.isEmpty());

    DcmFileFormat file;
    QVERIFY(file.loadFile(path.toLocal8Bit().constData()).good());
    file.getDataset()->putAndInsertString(DCM_CineRate, "25");
    QVERIFY(file.saveFile(path.toLocal8Bit().constData(), EXS_MPEG2MainProfileAtMainLevel).good());

    DICOMTagReader reader(path);
    VideoObject video = VideoObject::fromDICOM(reader);

    QVERIFY(!video.isNull());
    // 50 frames at 25 frames per second
    QVERIFY(qAbs(video.getDurationInSeconds() - 2.0) < 1e-9);
}

void test_VideoObject::fromDICOM_ShouldReturnNullAndSayWhy_ForAFileThatIsNotAVideo()
{
    DcmDataset *dataset = new DcmDataset;
    dataset->putAndInsertString(DCM_SOPClassUID, UID_CTImageStorage);
    dataset->putAndInsertString(DCM_SOPInstanceUID, "1.2.3.4");
    DICOMTagReader reader;
    reader.setDcmDataset("image.dcm", dataset);

    QString error;
    VideoObject video = VideoObject::fromDICOM(reader, &error);

    QVERIFY(video.isNull());
    QCOMPARE(error, QString("It is not a DICOM video."));
}

void test_VideoObject::extractTo_ShouldWriteTheBitstreamOfTheFragments()
{
    QTemporaryDir directory;
    QByteArray first = FakeBitstream;
    QByteArray second = QByteArray::fromHex("AABBCCDD");
    QString path = writeVideo(directory.path(), EXS_MPEG4HighProfileLevel4_1, {first, second});
    QVERIFY(!path.isEmpty());

    DICOMTagReader reader(path);
    VideoObject video = VideoObject::fromDICOM(reader);
    QString output = directory.path() + "/stream.mp4";
    QString error;

    QVERIFY2(video.extractTo(output, &error), qPrintable(error));
    // All the fragments, in order, and none of the basic offset table
    QCOMPARE(readFile(output), first + second);
}

void test_VideoObject::extractTo_ShouldFailAndSayWhy_ForANullVideo()
{
    QTemporaryDir directory;
    QString error;

    QVERIFY(!VideoObject().extractTo(directory.path() + "/stream.mp4", &error));
    QCOMPARE(error, QString("It is not a DICOM video."));
    QVERIFY(!QFile::exists(directory.path() + "/stream.mp4"));
}

void test_VideoObject::extractTo_ShouldFailAndSayWhy_WhenThereIsNoData()
{
    QTemporaryDir directory;
    QString path = writeVideo(directory.path(), EXS_MPEG4HighProfileLevel4_1, {});
    QVERIFY(!path.isEmpty());

    DICOMTagReader reader(path);
    VideoObject video = VideoObject::fromDICOM(reader);
    QString error;

    QVERIFY(!video.extractTo(directory.path() + "/stream.mp4", &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!QFile::exists(directory.path() + "/stream.mp4"));
}

void test_VideoObject::addVideo_ShouldNotAddANullOrARepeatedVideo()
{
    QTemporaryDir directory;
    QString path = writeVideo(directory.path(), EXS_MPEG4HighProfileLevel4_1, {FakeBitstream});
    DICOMTagReader reader(path);
    VideoObject video = VideoObject::fromDICOM(reader);
    Study study;

    QVERIFY(!study.addVideo(VideoObject()));
    QVERIFY(study.addVideo(video));
    QVERIFY(!study.addVideo(video));
    QCOMPARE(study.getVideos().size(), 1);
}

DECLARE_TEST(test_VideoObject)

#include "test_videoobject.moc"
