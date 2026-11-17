/*@
    "name": "test_DicomDeidentifier",
    "requirements": []
 */

#include "autotest.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <dcmtk/dcmdata/dctk.h>

#include "deidentificationjob.h"
#include "dicomdeidentifier.h"
#include "dicompixelblackout.h"

using namespace udg;

class test_DicomDeidentifier : public QObject {

    Q_OBJECT

private slots:
    void process_ShouldRemoveEveryIdentifyingValue();
    void process_ShouldRemoveThePrivateAttributesEvenInSequences();
    void process_ShouldReplaceUidsConsistently();
    void process_ShouldKeepTheClassesAndTheSyntaxes();
    void process_ShouldMarkTheObjectAsDeidentified();
    void process_ShouldGiveTheSameCodeToTheSamePatientAndAnotherToAnother();
    void process_ShouldReplaceDatesByDefault();
    void process_ShouldKeepDatesWithTheFullOption();
    void process_ShouldMoveDatesKeepingTheIntervals();
    void process_ShouldKeepCharacteristicsWithTheOption();
    void process_ShouldKeepDeviceIdentityWithTheOption();
    void process_ShouldKeepUidsWithTheOption();
    void process_ShouldCleanDescriptors();
    void process_ShouldWarnAboutTextInThePixels();
    void removedAttributes_ShouldHaveTheIdentityAttributes();

    void blackout_ShouldBlankTheRectangleInAllFrames();
    void blackout_ShouldWorkWithColorAndSixteenBitImages();
    void blackout_ShouldUseTheHighValueForMonochrome1();
    void blackout_ShouldClipAndRefuseWhatItCannotDo();

    void job_ShouldDeidentifyAFolderKeepingTheStudiesTogether();
    void job_ShouldRefuseToWriteInsideTheOriginals();
    void job_ShouldStopWhenTheProgressSaysSo();
    void job_ShouldNotWriteTheImagesWhoseTextCannotBeBlanked();

private:
    static void fill(DcmDataset *dataset, const QString &sop = "1.2.3.4.5.100", const QString &date = "20200315");
    static QStringList allStrings(DcmItem *item);
    static QString value(DcmItem *item, const DcmTagKey &key);
    static void addPixels(DcmDataset *dataset, int rows, int columns, int frames, int samples, int bits, const QString &photometric);
};

QString test_DicomDeidentifier::value(DcmItem *item, const DcmTagKey &key)
{
    OFString s;
    item->findAndGetOFString(key, s);
    return QString::fromLatin1(s.c_str());
}

QStringList test_DicomDeidentifier::allStrings(DcmItem *item)
{
    QStringList strings;

    for (unsigned long i = 0; i < item->card(); i++)
    {
        DcmElement *element = item->getElement(i);

        if (!element->isLeaf())
        {
            DcmSequenceOfItems *sequence = dynamic_cast<DcmSequenceOfItems*>(element);

            for (unsigned long k = 0; sequence && k < sequence->card(); k++)
            {
                strings << allStrings(sequence->getItem(k));
            }
        }
        else
        {
            OFString s;

            if (element->getVR() != EVR_OB && element->getVR() != EVR_OW && element->getOFString(s, 0).good())
            {
                strings << QString::fromLatin1(s.c_str());
            }
        }
    }

    return strings;
}

void test_DicomDeidentifier::fill(DcmDataset *ds, const QString &sop, const QString &date)
{
    ds->putAndInsertString(DCM_SOPClassUID, "1.2.840.10008.5.1.4.1.1.2");
    ds->putAndInsertString(DCM_SOPInstanceUID, sop.toLatin1().constData());
    ds->putAndInsertString(DCM_Modality, "CT");
    ds->putAndInsertString(DCM_PatientName, "DOE^JOHN");
    ds->putAndInsertString(DCM_PatientID, "12345678");
    ds->putAndInsertString(DCM_PatientBirthDate, "19600102");
    ds->putAndInsertString(DCM_PatientSex, "M");
    ds->putAndInsertString(DCM_PatientAge, "060Y");
    ds->putAndInsertString(DCM_PatientWeight, "80");
    ds->putAndInsertString(DCM_InstitutionName, "Girona Hospital");
    ds->putAndInsertString(DCM_ReferringPhysicianName, "SMITH^BOB");
    ds->putAndInsertString(DCM_OperatorsName, "NURSE^ANN");
    ds->putAndInsertString(DCM_AccessionNumber, "ACC998877");
    ds->putAndInsertString(DCM_StudyID, "STUDY55");
    ds->putAndInsertString(DCM_StudyDate, date.toLatin1().constData());
    ds->putAndInsertString(DCM_AcquisitionDate, "20200318");
    ds->putAndInsertString(DCM_StudyTime, "101500");
    ds->putAndInsertString(DCM_StudyDescription, "Chest of John DOE");
    ds->putAndInsertString(DCM_SeriesDescription, "Routine chest");
    ds->putAndInsertString(DCM_DeviceSerialNumber, "SN-4455");
    ds->putAndInsertString(DCM_StationName, "CT-ROOM-3");
    ds->putAndInsertString(DCM_StudyInstanceUID, "1.2.3.4.5");
    ds->putAndInsertString(DCM_SeriesInstanceUID, "1.2.3.4.5.6");
    ds->putAndInsertString(DCM_FrameOfReferenceUID, "1.2.3.4.5.7");
    ds->putAndInsertString(DcmTag(0x0009, 0x0010, EVR_LO), "PRIVATE CREATOR");
    ds->putAndInsertString(DcmTag(0x0009, 0x1001, EVR_LO), "private value with DOE");

    DcmItem *item = nullptr;
    ds->findOrCreateSequenceItem(DCM_ReferencedStudySequence, item, -2);
    item->putAndInsertString(DCM_ReferencedSOPClassUID, "1.2.840.10008.3.1.2.3.1");
    item->putAndInsertString(DCM_ReferencedSOPInstanceUID, "1.2.3.4.5.100");
    item->putAndInsertString(DcmTag(0x0009, 0x0010, EVR_LO), "NESTED PRIVATE");
    item->putAndInsertString(DCM_ReviewerName, "REVIEWER^RAY");
}

void test_DicomDeidentifier::addPixels(DcmDataset *ds, int rows, int columns, int frames, int samples, int bits, const QString &photometric)
{
    ds->putAndInsertUint16(DCM_Rows, rows);
    ds->putAndInsertUint16(DCM_Columns, columns);
    ds->putAndInsertUint16(DCM_BitsAllocated, bits);
    ds->putAndInsertUint16(DCM_BitsStored, bits);
    ds->putAndInsertUint16(DCM_HighBit, bits - 1);
    ds->putAndInsertUint16(DCM_SamplesPerPixel, samples);
    ds->putAndInsertUint16(DCM_PixelRepresentation, 0);
    ds->putAndInsertString(DCM_PhotometricInterpretation, photometric.toLatin1().constData());

    if (samples == 3)
    {
        ds->putAndInsertUint16(DCM_PlanarConfiguration, 0);
    }

    if (frames > 1)
    {
        ds->putAndInsertString(DCM_NumberOfFrames, QString::number(frames).toLatin1().constData());
    }

    QByteArray bytes(rows * columns * frames * samples * (bits / 8), '\x64');
    ds->putAndInsertUint8Array(DCM_PixelData, reinterpret_cast<const Uint8*>(bytes.constData()), bytes.size());
}

void test_DicomDeidentifier::process_ShouldRemoveEveryIdentifyingValue()
{
    DcmDataset ds;
    fill(&ds);
    DicomDeidentifier deidentifier;
    DicomDeidentifier::Report report = deidentifier.process(&ds);

    QStringList strings = allStrings(&ds);
    QStringList identity = {"DOE", "JOHN", "12345678", "19600102", "Girona", "SMITH", "NURSE", "ACC998877", "STUDY55", "CT-ROOM", "SN-4455", "REVIEWER", "Chest of", "Routine chest", "private value"};

    for (const QString &s : strings)
    {
        for (const QString &needle : identity)
        {
            QVERIFY2(!s.contains(needle), qPrintable(QString("'%1' still has '%2'").arg(s, needle)));
        }
    }

    QCOMPARE(value(&ds, DCM_PatientName), QString("ANONYMOUS"));
    QVERIFY(value(&ds, DCM_PatientID).startsWith("ANON-"));
    QVERIFY(report.removed > 5);
    QVERIFY(report.emptied > 3);
    QVERIFY(report.uidsReplaced >= 4);
}

void test_DicomDeidentifier::process_ShouldRemoveThePrivateAttributesEvenInSequences()
{
    DcmDataset ds;
    fill(&ds);
    DicomDeidentifier deidentifier;
    DicomDeidentifier::Report report = deidentifier.process(&ds);

    QVERIFY(!ds.tagExists(DcmTag(0x0009, 0x0010)));
    QVERIFY(!ds.tagExists(DcmTag(0x0009, 0x1001)));
    DcmItem *item = nullptr;
    QVERIFY(ds.findAndGetSequenceItem(DCM_ReferencedStudySequence, item, 0).good());
    QVERIFY(!item->tagExists(DcmTag(0x0009, 0x0010)));
    QCOMPARE(report.privateRemoved, 3);
}

void test_DicomDeidentifier::process_ShouldReplaceUidsConsistently()
{
    DcmDataset a;
    DcmDataset b;
    fill(&a, "1.2.3.4.5.100");
    fill(&b, "1.2.3.4.5.101");
    DicomDeidentifier deidentifier(DicomDeidentifier::Options(), "salt");
    deidentifier.process(&a);
    deidentifier.process(&b);

    QString study = value(&a, DCM_StudyInstanceUID);
    QVERIFY(study.startsWith("2.25."));
    QVERIFY(study != "1.2.3.4.5");
    QCOMPARE(value(&b, DCM_StudyInstanceUID), study);
    QCOMPARE(value(&b, DCM_SeriesInstanceUID), value(&a, DCM_SeriesInstanceUID));
    QCOMPARE(value(&b, DCM_FrameOfReferenceUID), value(&a, DCM_FrameOfReferenceUID));
    QVERIFY(value(&a, DCM_SOPInstanceUID) != value(&b, DCM_SOPInstanceUID));
    QVERIFY(value(&a, DCM_StudyInstanceUID) != value(&a, DCM_SeriesInstanceUID));

    // The reference inside the sequence points to the new SOP instance of the first image
    DcmItem *item = nullptr;
    a.findAndGetSequenceItem(DCM_ReferencedStudySequence, item, 0);
    QCOMPARE(value(item, DCM_ReferencedSOPInstanceUID), value(&a, DCM_SOPInstanceUID));

    // A UID is at most 64 characters
    QVERIFY(study.length() <= 64);
    QCOMPARE(deidentifier.mappedUid("1.2.3.4.5"), study);

    // With the same salt, the same run: the same UIDs
    DicomDeidentifier again(DicomDeidentifier::Options(), "salt");
    QCOMPARE(again.mappedUid("1.2.3.4.5"), study);
    DicomDeidentifier other(DicomDeidentifier::Options(), "another salt");
    QVERIFY(other.mappedUid("1.2.3.4.5") != study);
}

void test_DicomDeidentifier::process_ShouldKeepTheClassesAndTheSyntaxes()
{
    DcmDataset ds;
    fill(&ds);
    DicomDeidentifier deidentifier;
    deidentifier.process(&ds);

    QCOMPARE(value(&ds, DCM_SOPClassUID), QString("1.2.840.10008.5.1.4.1.1.2"));
    DcmItem *item = nullptr;
    ds.findAndGetSequenceItem(DCM_ReferencedStudySequence, item, 0);
    QCOMPARE(value(item, DCM_ReferencedSOPClassUID), QString("1.2.840.10008.3.1.2.3.1"));
}

void test_DicomDeidentifier::process_ShouldMarkTheObjectAsDeidentified()
{
    DcmDataset ds;
    fill(&ds);
    DicomDeidentifier::Options options;
    options.retainDates = true;
    DicomDeidentifier deidentifier(options);
    deidentifier.process(&ds);

    QCOMPARE(value(&ds, DCM_PatientIdentityRemoved), QString("YES"));
    QVERIFY(value(&ds, DCM_DeidentificationMethod).contains("Basic Application Level Confidentiality Profile"));
    QVERIFY(value(&ds, DCM_DeidentificationMethod).length() <= 64);

    // The profile and the option are coded
    DcmSequenceOfItems *sequence = nullptr;
    QVERIFY(ds.findAndGetSequence(DCM_DeidentificationMethodCodeSequence, sequence).good());
    QCOMPARE(static_cast<int>(sequence->card()), 2);
    QCOMPARE(value(sequence->getItem(0), DCM_CodeValue), QString("113100"));
    QCOMPARE(value(sequence->getItem(1), DCM_CodeValue), QString("113106"));
    QCOMPARE(value(sequence->getItem(1), DCM_CodingSchemeDesignator), QString("DCM"));

    // Processing a second object does not pile up the codes
    DcmDataset another;
    fill(&another);
    deidentifier.process(&another);
    another.findAndGetSequence(DCM_DeidentificationMethodCodeSequence, sequence);
    QCOMPARE(static_cast<int>(sequence->card()), 2);

    // The cleaned pixels are said
    DcmDataset cleaned;
    fill(&cleaned);
    DicomDeidentifier::Options cleanedOptions;
    cleanedOptions.pixelsCleaned = true;
    DicomDeidentifier(cleanedOptions).process(&cleaned);
    cleaned.findAndGetSequence(DCM_DeidentificationMethodCodeSequence, sequence);
    QCOMPARE(value(sequence->getItem(1), DCM_CodeValue), QString("113101"));
}

void test_DicomDeidentifier::process_ShouldGiveTheSameCodeToTheSamePatientAndAnotherToAnother()
{
    DcmDataset a, b, c;
    fill(&a);
    fill(&b);
    fill(&c);
    c.putAndInsertString(DCM_PatientID, "99999999");
    DicomDeidentifier deidentifier(DicomDeidentifier::Options(), "salt");
    deidentifier.process(&a);
    deidentifier.process(&b);
    deidentifier.process(&c);

    QCOMPARE(value(&a, DCM_PatientID), value(&b, DCM_PatientID));
    QVERIFY(value(&a, DCM_PatientID) != value(&c, DCM_PatientID));
    QVERIFY(!value(&a, DCM_PatientID).contains("12345678"));

    DcmDataset d;
    fill(&d);
    DicomDeidentifier::Options own;
    own.patientId = "TRIAL-001";
    own.patientName = "TRIAL^ONE";
    DicomDeidentifier named(own);
    named.process(&d);
    QCOMPARE(value(&d, DCM_PatientID), QString("TRIAL-001"));
    QCOMPARE(value(&d, DCM_PatientName), QString("TRIAL^ONE"));
}

void test_DicomDeidentifier::process_ShouldReplaceDatesByDefault()
{
    DcmDataset ds;
    fill(&ds);
    DicomDeidentifier deidentifier;
    deidentifier.process(&ds);

    QCOMPARE(value(&ds, DCM_StudyDate), QString("19000101"));
    QCOMPARE(value(&ds, DCM_AcquisitionDate), QString("19000101"));
    QCOMPARE(value(&ds, DCM_StudyTime), QString("000000"));
    QVERIFY(value(&ds, DCM_PatientBirthDate).isEmpty());
}

void test_DicomDeidentifier::process_ShouldKeepDatesWithTheFullOption()
{
    DcmDataset ds;
    fill(&ds);
    DicomDeidentifier::Options options;
    options.retainDates = true;
    DicomDeidentifier deidentifier(options);
    deidentifier.process(&ds);

    QCOMPARE(value(&ds, DCM_StudyDate), QString("20200315"));
    QCOMPARE(value(&ds, DCM_StudyTime), QString("101500"));
    // The birth date is still emptied: it is not a date of the care
    QVERIFY(value(&ds, DCM_PatientBirthDate).isEmpty());
}

void test_DicomDeidentifier::process_ShouldMoveDatesKeepingTheIntervals()
{
    DcmDataset a, b;
    fill(&a, "1.2.3.4.5.100", "20200315");
    fill(&b, "1.2.3.4.5.101", "20210101");
    DicomDeidentifier::Options options;
    options.retainModifiedDates = true;
    DicomDeidentifier deidentifier(options, "salt");
    deidentifier.process(&a);
    deidentifier.process(&b);

    QDate study = QDate::fromString(value(&a, DCM_StudyDate), "yyyyMMdd");
    QDate acquisition = QDate::fromString(value(&a, DCM_AcquisitionDate), "yyyyMMdd");
    QVERIFY(study.isValid() && acquisition.isValid());
    QCOMPARE(study.daysTo(acquisition), 3);
    QVERIFY(study != QDate(2020, 3, 15));
    int shift = QDate(2020, 3, 15).daysTo(study);
    QVERIFY(shift <= -30 && shift >= -365);
    QCOMPARE(shift, deidentifier.dateShiftDaysFor("12345678"));

    // The same patient has the same shift in another study
    QCOMPARE(QDate(2021, 1, 1).daysTo(QDate::fromString(value(&b, DCM_StudyDate), "yyyyMMdd")), shift);
    QCOMPARE(value(&a, DCM_StudyTime), QString("101500"));
}

void test_DicomDeidentifier::process_ShouldKeepCharacteristicsWithTheOption()
{
    DcmDataset ds;
    fill(&ds);
    DicomDeidentifier::Options options;
    options.retainPatientCharacteristics = true;
    DicomDeidentifier deidentifier(options);
    deidentifier.process(&ds);

    QCOMPARE(value(&ds, DCM_PatientSex), QString("M"));
    QCOMPARE(value(&ds, DCM_PatientAge), QString("060Y"));
    QCOMPARE(value(&ds, DCM_PatientWeight), QString("80"));

    DcmDataset plain;
    fill(&plain);
    DicomDeidentifier().process(&plain);
    QVERIFY(value(&plain, DCM_PatientSex).isEmpty());
    QVERIFY(!plain.tagExists(DCM_PatientAge));
}

void test_DicomDeidentifier::process_ShouldKeepDeviceIdentityWithTheOption()
{
    DcmDataset ds;
    fill(&ds);
    DicomDeidentifier::Options options;
    options.retainDeviceIdentity = true;
    DicomDeidentifier deidentifier(options);
    deidentifier.process(&ds);

    QCOMPARE(value(&ds, DCM_DeviceSerialNumber), QString("SN-4455"));
    QCOMPARE(value(&ds, DCM_StationName), QString("CT-ROOM-3"));
}

void test_DicomDeidentifier::process_ShouldKeepUidsWithTheOption()
{
    DcmDataset ds;
    fill(&ds);
    DicomDeidentifier::Options options;
    options.retainUids = true;
    DicomDeidentifier deidentifier(options);
    DicomDeidentifier::Report report = deidentifier.process(&ds);

    QCOMPARE(value(&ds, DCM_StudyInstanceUID), QString("1.2.3.4.5"));
    QCOMPARE(report.uidsReplaced, 0);
}

void test_DicomDeidentifier::process_ShouldCleanDescriptors()
{
    DcmDataset ds;
    fill(&ds);
    DicomDeidentifier::Options options;
    options.cleanDescriptors = true;
    DicomDeidentifier deidentifier(options);
    deidentifier.process(&ds);

    // The description with the name of the patient is emptied, the other one stays
    QVERIFY(value(&ds, DCM_StudyDescription).isEmpty());
    QCOMPARE(value(&ds, DCM_SeriesDescription), QString("Routine chest"));

    // Without the option both are removed
    DcmDataset plain;
    fill(&plain);
    DicomDeidentifier().process(&plain);
    QVERIFY(!plain.tagExists(DCM_SeriesDescription));
}

void test_DicomDeidentifier::process_ShouldWarnAboutTextInThePixels()
{
    DcmDataset burned;
    fill(&burned);
    burned.putAndInsertString(DCM_BurnedInAnnotation, "YES");
    DicomDeidentifier::Report report = DicomDeidentifier().process(&burned);
    QVERIFY(!report.warnings.isEmpty());
    QVERIFY(report.warnings.first().contains("Burned In Annotation"));

    DcmDataset ultrasound;
    fill(&ultrasound);
    ultrasound.putAndInsertString(DCM_Modality, "US");
    QVERIFY(!DicomDeidentifier().process(&ultrasound).warnings.isEmpty());

    DcmDataset ct;
    fill(&ct);
    QVERIFY(DicomDeidentifier().process(&ct).warnings.isEmpty());
}

void test_DicomDeidentifier::removedAttributes_ShouldHaveTheIdentityAttributes()
{
    QList<QPair<quint16, quint16>> list = DicomDeidentifier::removedAttributes();

    QVERIFY(list.contains(qMakePair<quint16, quint16>(0x0008, 0x0080)));
    QVERIFY(list.contains(qMakePair<quint16, quint16>(0x0010, 0x0030)));
    QVERIFY(list.contains(qMakePair<quint16, quint16>(0x0008, 0x0050)));
    QVERIFY(list.size() > 100);
}

void test_DicomDeidentifier::blackout_ShouldBlankTheRectangleInAllFrames()
{
    DcmDataset ds;
    addPixels(&ds, 6, 8, 2, 1, 8, "MONOCHROME2");
    QString error;
    QVERIFY(DicomPixelBlackout::apply(&ds, {QRect(1, 2, 3, 2)}, &error));

    const Uint8 *bytes = nullptr;
    ds.findAndGetUint8Array(DCM_PixelData, bytes);

    for (int frame = 0; frame < 2; frame++)
    {
        for (int y = 0; y < 6; y++)
        {
            for (int x = 0; x < 8; x++)
            {
                bool inside = x >= 1 && x <= 3 && y >= 2 && y <= 3;
                QCOMPARE(static_cast<int>(bytes[frame * 48 + y * 8 + x]), inside ? 0 : 0x64);
            }
        }
    }
}

void test_DicomDeidentifier::blackout_ShouldWorkWithColorAndSixteenBitImages()
{
    DcmDataset color;
    addPixels(&color, 4, 4, 1, 3, 8, "RGB");
    QVERIFY(DicomPixelBlackout::apply(&color, {QRect(0, 0, 2, 1)}));
    const Uint8 *bytes = nullptr;
    color.findAndGetUint8Array(DCM_PixelData, bytes);
    QCOMPARE(static_cast<int>(bytes[0]), 0);
    QCOMPARE(static_cast<int>(bytes[5]), 0);
    QCOMPARE(static_cast<int>(bytes[6]), 0x64);

    DcmDataset wide;
    addPixels(&wide, 4, 4, 1, 1, 16, "MONOCHROME2");
    QVERIFY(DicomPixelBlackout::apply(&wide, {QRect(1, 1, 1, 1)}));
    wide.findAndGetUint8Array(DCM_PixelData, bytes);
    QCOMPARE(static_cast<int>(bytes[(1 * 4 + 1) * 2]), 0);
    QCOMPARE(static_cast<int>(bytes[(1 * 4 + 1) * 2 + 1]), 0);
    QCOMPARE(static_cast<int>(bytes[(1 * 4 + 2) * 2]), 0x64);
}

void test_DicomDeidentifier::blackout_ShouldUseTheHighValueForMonochrome1()
{
    DcmDataset ds;
    addPixels(&ds, 2, 2, 1, 1, 8, "MONOCHROME1");
    QVERIFY(DicomPixelBlackout::apply(&ds, {QRect(0, 0, 1, 1)}));
    const Uint8 *bytes = nullptr;
    ds.findAndGetUint8Array(DCM_PixelData, bytes);
    QCOMPARE(static_cast<int>(bytes[0]), 255);
}

void test_DicomDeidentifier::blackout_ShouldClipAndRefuseWhatItCannotDo()
{
    DcmDataset ds;
    addPixels(&ds, 4, 4, 1, 1, 8, "MONOCHROME2");
    // A rectangle that goes out of the image is clipped, and one that is entirely outside does nothing
    QVERIFY(DicomPixelBlackout::apply(&ds, {QRect(2, 2, 100, 100), QRect(50, 50, 5, 5)}));
    const Uint8 *bytes = nullptr;
    ds.findAndGetUint8Array(DCM_PixelData, bytes);
    QCOMPARE(static_cast<int>(bytes[3 * 4 + 3]), 0);
    QCOMPARE(static_cast<int>(bytes[0]), 0x64);

    QString error;
    DcmDataset none;
    QVERIFY(!DicomPixelBlackout::apply(&none, {QRect(0, 0, 1, 1)}, &error));
    QVERIFY(!error.isEmpty());

    DcmDataset odd;
    addPixels(&odd, 4, 4, 1, 1, 16, "MONOCHROME2");
    odd.putAndInsertUint16(DCM_BitsAllocated, 12);
    QVERIFY(!DicomPixelBlackout::apply(&odd, {QRect(0, 0, 1, 1)}, &error));

    DcmDataset shortPixels;
    addPixels(&shortPixels, 4, 4, 1, 1, 8, "MONOCHROME2");
    shortPixels.putAndInsertUint16(DCM_Rows, 40);
    QVERIFY(!DicomPixelBlackout::apply(&shortPixels, {QRect(0, 0, 1, 1)}, &error));

    // No rectangles is not an error and changes nothing
    QVERIFY(DicomPixelBlackout::apply(&none, QList<QRect>()));
}

static void saveImage(const QString &path, const QString &sop, bool withPixels = true, const QString &date = "20200315")
{
    DcmFileFormat file;
    DcmDataset *ds = file.getDataset();
    ds->putAndInsertString(DCM_SOPClassUID, "1.2.840.10008.5.1.4.1.1.7");
    ds->putAndInsertString(DCM_SOPInstanceUID, sop.toLatin1().constData());
    ds->putAndInsertString(DCM_Modality, "OT");
    ds->putAndInsertString(DCM_PatientName, "DOE^JOHN");
    ds->putAndInsertString(DCM_PatientID, "12345678");
    ds->putAndInsertString(DCM_InstitutionName, "Girona Hospital");
    ds->putAndInsertString(DCM_StudyDate, date.toLatin1().constData());
    ds->putAndInsertString(DCM_StudyInstanceUID, "1.2.3.4.5");
    ds->putAndInsertString(DCM_SeriesInstanceUID, "1.2.3.4.5.6");

    if (withPixels)
    {
        ds->putAndInsertUint16(DCM_Rows, 4);
        ds->putAndInsertUint16(DCM_Columns, 4);
        ds->putAndInsertUint16(DCM_BitsAllocated, 8);
        ds->putAndInsertUint16(DCM_BitsStored, 8);
        ds->putAndInsertUint16(DCM_HighBit, 7);
        ds->putAndInsertUint16(DCM_SamplesPerPixel, 1);
        ds->putAndInsertUint16(DCM_PixelRepresentation, 0);
        ds->putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME2");
        QByteArray bytes(16, '\x64');
        ds->putAndInsertUint8Array(DCM_PixelData, reinterpret_cast<const Uint8*>(bytes.constData()), bytes.size());
    }

    file.saveFile(path.toLocal8Bit().constData(), EXS_LittleEndianExplicit);
}

void test_DicomDeidentifier::job_ShouldDeidentifyAFolderKeepingTheStudiesTogether()
{
    QTemporaryDir root;
    QDir().mkpath(root.filePath("in/series1"));
    QDir().mkpath(root.filePath("in/DOE_JOHN_series2"));
    saveImage(root.filePath("in/series1/a.dcm"), "1.2.3.4.5.100");
    saveImage(root.filePath("in/DOE_JOHN_series2/b.dcm"), "1.2.3.4.5.101");
    QFile notDicom(root.filePath("in/readme.txt"));
    QVERIFY(notDicom.open(QIODevice::WriteOnly));
    notDicom.write("not dicom at all");
    notDicom.close();

    DicomDeidentifier::Options options;
    DeidentificationJob::Result result = DeidentificationJob::run(root.filePath("in"), root.filePath("out"), options, QList<QRect>(), nullptr, "salt");

    QVERIFY2(result.success, qPrintable(result.message));
    QCOMPARE(result.processed, 2);
    QCOMPARE(result.notDicom, 1);
    QVERIFY(result.totals.uidsReplaced >= 6);

    QStringList names = QDir(root.filePath("out")).entryList(QDir::Files, QDir::Name);
    QCOMPARE(names, QStringList() << "IM000001.dcm" << "IM000002.dcm");

    DcmFileFormat first, second;
    QVERIFY(first.loadFile(root.filePath("out/IM000001.dcm").toLocal8Bit().constData()).good());
    QVERIFY(second.loadFile(root.filePath("out/IM000002.dcm").toLocal8Bit().constData()).good());
    QCOMPARE(value(first.getDataset(), DCM_StudyInstanceUID), value(second.getDataset(), DCM_StudyInstanceUID));
    QVERIFY(value(first.getDataset(), DCM_StudyInstanceUID) != "1.2.3.4.5");
    QVERIFY(value(first.getDataset(), DCM_SOPInstanceUID) != value(second.getDataset(), DCM_SOPInstanceUID));
    QCOMPARE(value(first.getMetaInfo(), DCM_MediaStorageSOPInstanceUID), value(first.getDataset(), DCM_SOPInstanceUID));

    // None of the identity is in the bytes of the new files (the meta information too)
    for (const QString &name : names)
    {
        QFile file(root.filePath("out/" + name));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QByteArray bytes = file.readAll();
        QVERIFY(!bytes.contains("DOE"));
        QVERIFY(!bytes.contains("12345678"));
        QVERIFY(!bytes.contains("Girona"));
    }

    // The originals are not changed
    DcmFileFormat original;
    QVERIFY(original.loadFile(root.filePath("in/series1/a.dcm").toLocal8Bit().constData()).good());
    QCOMPARE(value(original.getDataset(), DCM_PatientName), QString("DOE^JOHN"));
}

void test_DicomDeidentifier::job_ShouldRefuseToWriteInsideTheOriginals()
{
    QTemporaryDir root;
    QDir().mkpath(root.filePath("in"));
    saveImage(root.filePath("in/a.dcm"), "1.2.3.4.5.100");

    QVERIFY(!DeidentificationJob::run(root.filePath("in"), root.filePath("in"), DicomDeidentifier::Options()).success);
    QVERIFY(!DeidentificationJob::run(root.filePath("in"), root.filePath("in/out"), DicomDeidentifier::Options()).success);
    QVERIFY(!DeidentificationJob::run(root.filePath("missing"), root.filePath("out"), DicomDeidentifier::Options()).success);
    QVERIFY(!DeidentificationJob::run(root.filePath("in"), "", DicomDeidentifier::Options()).success);
    QCOMPARE(QDir(root.filePath("in")).entryList(QDir::Files).size(), 1);
}

void test_DicomDeidentifier::job_ShouldStopWhenTheProgressSaysSo()
{
    QTemporaryDir root;
    QDir().mkpath(root.filePath("in"));

    for (int i = 0; i < 5; i++)
    {
        saveImage(root.filePath(QString("in/%1.dcm").arg(i)), QString("1.2.3.4.5.%1").arg(100 + i));
    }

    DeidentificationJob::Result result = DeidentificationJob::run(root.filePath("in"), root.filePath("out"), DicomDeidentifier::Options(), QList<QRect>(), [](int done, int) { return done < 2; });

    QVERIFY(!result.success);
    QCOMPARE(result.processed, 2);
    QCOMPARE(QDir(root.filePath("out")).entryList(QDir::Files).size(), 2);
}

void test_DicomDeidentifier::job_ShouldNotWriteTheImagesWhoseTextCannotBeBlanked()
{
    QTemporaryDir root;
    QDir().mkpath(root.filePath("in"));
    saveImage(root.filePath("in/with.dcm"), "1.2.3.4.5.100");
    saveImage(root.filePath("in/without.dcm"), "1.2.3.4.5.101", false);

    DeidentificationJob::Result result = DeidentificationJob::run(root.filePath("in"), root.filePath("out"), DicomDeidentifier::Options(), {QRect(0, 0, 2, 2)});

    QVERIFY(!result.success);
    QCOMPARE(result.processed, 1);
    QCOMPARE(result.blanked, 1);
    QCOMPARE(result.failed, 1);
    QCOMPARE(result.errors.size(), 1);
    QCOMPARE(QDir(root.filePath("out")).entryList(QDir::Files).size(), 1);

    DcmFileFormat file;
    QVERIFY(file.loadFile(root.filePath("out/IM000001.dcm").toLocal8Bit().constData()).good());
    const Uint8 *bytes = nullptr;
    file.getDataset()->findAndGetUint8Array(DCM_PixelData, bytes);
    QCOMPARE(static_cast<int>(bytes[0]), 0);
    QCOMPARE(static_cast<int>(bytes[15]), 0x64);
}

DECLARE_TEST(test_DicomDeidentifier)

#include "test_dicomdeidentifier.moc"
