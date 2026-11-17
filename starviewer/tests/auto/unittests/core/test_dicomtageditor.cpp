/*@
    "name": "test_DicomTagEditor",
    "requirements": []
 */

#include "autotest.h"

#include <QDir>
#include <QFileInfo>

#include <QFile>
#include <QTemporaryDir>

#include <dcmtk/dcmdata/dctk.h>

#include "dicomtageditor.h"

using namespace udg;

class test_DicomTagEditor : public QObject {

    Q_OBJECT

private slots:
    void validate_ShouldCheckTheValueForItsType_data();
    void validate_ShouldCheckTheValueForItsType();

    void load_ShouldListTheAttributesWithTheSequences();
    void load_ShouldFailForAFileThatIsNotDicom();
    void setValue_ShouldChangeAnAttribute();
    void setValue_ShouldRefuseWrongValuesAndBinaryAttributes();
    void setValue_ShouldWorkInsideSequences();
    void remove_ShouldRemoveAnAttributeAndWhatIsInIt();
    void add_ShouldAddAttributesOfTheDictionary();
    void add_ShouldRefuseWhatItCannotAdd();
    void save_ShouldWriteAnotherFileAndNeverTheOriginal();
    void paths_ShouldBeChecked();

private:
    static QString makeFile(const QTemporaryDir &dir);
};

QString test_DicomTagEditor::makeFile(const QTemporaryDir &dir)
{
    DcmFileFormat file;
    DcmDataset *ds = file.getDataset();
    ds->putAndInsertString(DCM_SOPClassUID, "1.2.840.10008.5.1.4.1.1.7");
    ds->putAndInsertString(DCM_SOPInstanceUID, "1.2.3.4.5.100");
    ds->putAndInsertString(DCM_Modality, "OT");
    ds->putAndInsertString(DCM_PatientName, "DOE^JOHN");
    ds->putAndInsertString(DCM_PatientID, "12345");
    ds->putAndInsertString(DCM_StudyDate, "20200315");
    ds->putAndInsertString(DCM_StudyInstanceUID, "1.2.3.4.5");
    ds->putAndInsertString(DCM_PatientAge, "060Y");
    ds->putAndInsertString(DcmTag(0x0009, 0x0010, EVR_LO), "PRIVATE");
    DcmItem *item = nullptr;
    ds->findOrCreateSequenceItem(DCM_ReferencedStudySequence, item, -2);
    item->putAndInsertString(DCM_ReferencedSOPInstanceUID, "1.2.3.4.5.9");
    ds->putAndInsertUint16(DCM_Rows, 2);
    ds->putAndInsertUint16(DCM_Columns, 2);
    ds->putAndInsertUint16(DCM_BitsAllocated, 8);
    ds->putAndInsertUint16(DCM_SamplesPerPixel, 1);
    QByteArray bytes(4, '\x64');
    ds->putAndInsertUint8Array(DCM_PixelData, reinterpret_cast<const Uint8*>(bytes.constData()), bytes.size());
    QString path = dir.filePath("a.dcm");
    file.saveFile(path.toLocal8Bit().constData(), EXS_LittleEndianExplicit);
    return path;
}

void test_DicomTagEditor::validate_ShouldCheckTheValueForItsType_data()
{
    QTest::addColumn<QString>("vr");
    QTest::addColumn<QString>("value");
    QTest::addColumn<bool>("valid");

    QTest::newRow("empty") << "DA" << "" << true;
    QTest::newRow("date") << "DA" << "20200315" << true;
    QTest::newRow("date two") << "DA" << "20200315\\20200316" << true;
    QTest::newRow("date format") << "DA" << "2020-03-15" << false;
    QTest::newRow("date not real") << "DA" << "20200230" << false;
    QTest::newRow("time") << "TM" << "101500" << true;
    QTest::newRow("time fraction") << "TM" << "101500.25" << true;
    QTest::newRow("time hour only") << "TM" << "10" << true;
    QTest::newRow("time hour 25") << "TM" << "251500" << false;
    QTest::newRow("date time") << "DT" << "20200315101500" << true;
    QTest::newRow("date time bad") << "DT" << "2020031" << false;
    QTest::newRow("uid") << "UI" << "1.2.840.10008.5.1.4.1.1.7" << true;
    QTest::newRow("uid letters") << "UI" << "1.2.a" << false;
    QTest::newRow("uid leading zero") << "UI" << "1.02.3" << false;
    QTest::newRow("uid long") << "UI" << ("1." + QString(70, '2')) << false;
    QTest::newRow("code string") << "CS" << "ORIGINAL\\PRIMARY" << true;
    QTest::newRow("code string lower") << "CS" << "original" << false;
    QTest::newRow("code string long") << "CS" << "ABCDEFGHIJKLMNOPQ" << false;
    QTest::newRow("age") << "AS" << "045Y" << true;
    QTest::newRow("age bad") << "AS" << "45Y" << false;
    QTest::newRow("integer string") << "IS" << "-12" << true;
    QTest::newRow("integer string bad") << "IS" << "1.5" << false;
    QTest::newRow("decimal string") << "DS" << "0.8025" << true;
    QTest::newRow("decimal string exponent") << "DS" << "1.5e3" << true;
    QTest::newRow("decimal string bad") << "DS" << "abc" << false;
    QTest::newRow("unsigned short") << "US" << "65535" << true;
    QTest::newRow("unsigned short big") << "US" << "65536" << false;
    QTest::newRow("unsigned short negative") << "US" << "-1" << false;
    QTest::newRow("signed short") << "SS" << "-32768" << true;
    QTest::newRow("signed short big") << "SS" << "32768" << false;
    QTest::newRow("unsigned long") << "UL" << "4294967295" << true;
    QTest::newRow("unsigned long big") << "UL" << "4294967296" << false;
    QTest::newRow("float") << "FD" << "3.14" << true;
    QTest::newRow("float bad") << "FL" << "pi" << false;
    QTest::newRow("person name") << "PN" << "DOE^JOHN^^^Dr" << true;
    QTest::newRow("person name six components") << "PN" << "A^B^C^D^E^F" << false;
    QTest::newRow("person name four groups") << "PN" << "A=B=C=D" << false;
    QTest::newRow("long text") << "LO" << QString(64, 'x') << true;
    QTest::newRow("long text too long") << "LO" << QString(65, 'x') << false;
    QTest::newRow("short text too long") << "SH" << QString(17, 'x') << false;
    QTest::newRow("binary") << "OB" << "12" << false;
    QTest::newRow("sequence") << "SQ" << "12" << false;
}

void test_DicomTagEditor::validate_ShouldCheckTheValueForItsType()
{
    QFETCH(QString, vr);
    QFETCH(QString, value);
    QFETCH(bool, valid);

    QString error;
    QCOMPARE(DicomTagEditor::validate(vr, value, &error), valid);
    QCOMPARE(error.isEmpty(), valid);
}

void test_DicomTagEditor::load_ShouldListTheAttributesWithTheSequences()
{
    QTemporaryDir dir;
    DicomTagEditor editor;
    QString error;
    QVERIFY2(editor.load(makeFile(dir), &error), qPrintable(error));

    QList<DicomTagEditor::Entry> entries = editor.entries();
    QVERIFY(entries.size() >= 12);

    DicomTagEditor::Entry name, sequence, nested, pixels, privateEntry;

    for (const DicomTagEditor::Entry &e : entries)
    {
        if (e.path == "0010,0010")
        {
            name = e;
        }
        else if (e.path == "0008,1110")
        {
            sequence = e;
        }
        else if (e.path == "0008,1110[0]/0008,1155")
        {
            nested = e;
        }
        else if (e.path == "7fe0,0010")
        {
            pixels = e;
        }
        else if (e.isPrivate && e.path == "0009,0010")
        {
            privateEntry = e;
        }
    }

    QCOMPARE(name.value, QString("DOE^JOHN"));
    QCOMPARE(name.vr, QString("PN"));
    QCOMPARE(name.name, QString("PatientName"));
    QVERIFY(name.editable);
    QVERIFY(sequence.isSequence);
    QVERIFY(!sequence.editable);
    QCOMPARE(nested.value, QString("1.2.3.4.5.9"));
    QCOMPARE(nested.depth, 1);
    QVERIFY(!pixels.editable);
    QVERIFY(pixels.value.contains("bytes"));
    QVERIFY(privateEntry.isPrivate);
    QVERIFY(!editor.isModified());
}

void test_DicomTagEditor::load_ShouldFailForAFileThatIsNotDicom()
{
    QTemporaryDir dir;
    QFile file(dir.filePath("x.txt"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("hello");
    file.close();

    DicomTagEditor editor;
    QString error;
    QVERIFY(!editor.load(dir.filePath("x.txt"), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!editor.isLoaded());
    QVERIFY(!editor.load(dir.filePath("missing.dcm")));
    QVERIFY(editor.entries().isEmpty());
}

void test_DicomTagEditor::setValue_ShouldChangeAnAttribute()
{
    QTemporaryDir dir;
    DicomTagEditor editor;
    QVERIFY(editor.load(makeFile(dir)));
    QString error;

    QVERIFY2(editor.setValue("0010,0010", "ROE^JANE", &error), qPrintable(error));
    QCOMPARE(editor.value("0010,0010"), QString("ROE^JANE"));
    QVERIFY(editor.setValue("0008,0020", "20210101"));
    QVERIFY(editor.setValue("0010,0020", ""));
    QVERIFY(editor.value("0010,0020").isEmpty());
    QVERIFY(editor.exists("0010,0020"));
    QVERIFY(editor.isModified());
}

void test_DicomTagEditor::setValue_ShouldRefuseWrongValuesAndBinaryAttributes()
{
    QTemporaryDir dir;
    DicomTagEditor editor;
    QVERIFY(editor.load(makeFile(dir)));
    QString error;

    QVERIFY(!editor.setValue("0008,0020", "not a date", &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(editor.value("0008,0020"), QString("20200315"));
    QVERIFY(!editor.setValue("0010,1010", "60", &error));       // age needs 060Y
    QVERIFY(!editor.setValue("7fe0,0010", "12", &error));       // pixels
    QVERIFY(!editor.setValue("0008,1110", "12", &error));       // a sequence
    QVERIFY(!editor.setValue("0020,0010", "1", &error));        // does not exist
    QVERIFY(!editor.isModified());
}

void test_DicomTagEditor::setValue_ShouldWorkInsideSequences()
{
    QTemporaryDir dir;
    DicomTagEditor editor;
    QVERIFY(editor.load(makeFile(dir)));
    QString error;

    QVERIFY2(editor.setValue("0008,1110[0]/0008,1155", "1.2.3.99", &error), qPrintable(error));
    QCOMPARE(editor.value("0008,1110[0]/0008,1155"), QString("1.2.3.99"));
    QVERIFY(!editor.setValue("0008,1110[0]/0008,1155", "1.2.x", &error));
    QVERIFY(!editor.setValue("0008,1110[3]/0008,1155", "1.2.3", &error));
    QVERIFY(!editor.setValue("0008,1111[0]/0008,1155", "1.2.3", &error));
}

void test_DicomTagEditor::remove_ShouldRemoveAnAttributeAndWhatIsInIt()
{
    QTemporaryDir dir;
    DicomTagEditor editor;
    QVERIFY(editor.load(makeFile(dir)));

    QVERIFY(editor.remove("0010,1010"));
    QVERIFY(!editor.exists("0010,1010"));
    QVERIFY(editor.remove("0008,1110"));
    QVERIFY(!editor.exists("0008,1110[0]/0008,1155") || editor.value("0008,1110[0]/0008,1155").isEmpty());
    QVERIFY(editor.remove("0009,0010"));
    QVERIFY(!editor.remove("0010,1010"));
    QVERIFY(editor.isModified());
}

void test_DicomTagEditor::add_ShouldAddAttributesOfTheDictionary()
{
    QTemporaryDir dir;
    DicomTagEditor editor;
    QVERIFY(editor.load(makeFile(dir)));
    QString error;

    QVERIFY2(editor.add(QString(), 0x0010, 0x0040, "F", &error), qPrintable(error));
    QCOMPARE(editor.value("0010,0040"), QString("F"));
    QVERIFY2(editor.add("0008,1110[0]", 0x0008, 0x1150, "1.2.840.10008.5.1.4.1.1.7", &error), qPrintable(error));
    QCOMPARE(editor.value("0008,1110[0]/0008,1150"), QString("1.2.840.10008.5.1.4.1.1.7"));
}

void test_DicomTagEditor::add_ShouldRefuseWhatItCannotAdd()
{
    QTemporaryDir dir;
    DicomTagEditor editor;
    QVERIFY(editor.load(makeFile(dir)));
    QString error;

    QVERIFY(!editor.add(QString(), 0x0010, 0x0010, "X", &error));      // exists
    QVERIFY(!editor.add(QString(), 0x0009, 0x1000, "X", &error));      // private
    QVERIFY(!editor.add(QString(), 0x0011, 0x1111, "X", &error));      // not in the dictionary
    QVERIFY(!editor.add(QString(), 0x0010, 0x0040, "zz", &error));
    QVERIFY(!editor.exists("0010,0040"));
    QVERIFY(!editor.add("0008,1110[5]", 0x0008, 0x1150, "1.2", &error));
    QVERIFY(!editor.add(QString(), 0x7FE0, 0x0010, "1", &error));      // binary
}

void test_DicomTagEditor::save_ShouldWriteAnotherFileAndNeverTheOriginal()
{
    QTemporaryDir dir;
    QString original = makeFile(dir);
    DicomTagEditor editor;
    QVERIFY(editor.load(original));
    QVERIFY(editor.setValue("0010,0010", "ROE^JANE"));
    QVERIFY(editor.remove("0010,1010"));
    QVERIFY(editor.setValue("0020,000D", "1.2.3.4.77"));

    QString error;
    QVERIFY(!editor.save(original, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!editor.save(QDir::cleanPath(dir.path() + "/../" + QFileInfo(dir.path()).fileName() + "/a.dcm"), &error));

    QVERIFY2(editor.save(dir.filePath("b.dcm"), &error), qPrintable(error));

    DicomTagEditor saved;
    QVERIFY(saved.load(dir.filePath("b.dcm")));
    QCOMPARE(saved.value("0010,0010"), QString("ROE^JANE"));
    QVERIFY(!saved.exists("0010,1010"));
    QCOMPARE(saved.value("0020,000D"), QString("1.2.3.4.77"));
    // The pixels are still there
    QVERIFY(saved.exists("7fe0,0010"));

    // The original is as it was
    DicomTagEditor untouched;
    QVERIFY(untouched.load(original));
    QCOMPARE(untouched.value("0010,0010"), QString("DOE^JOHN"));
    QVERIFY(untouched.exists("0010,1010"));
}

void test_DicomTagEditor::paths_ShouldBeChecked()
{
    QTemporaryDir dir;
    DicomTagEditor editor;
    QVERIFY(editor.load(makeFile(dir)));

    QVERIFY(!editor.exists(""));
    QVERIFY(!editor.exists("nonsense"));
    QVERIFY(!editor.exists("0010,00"));
    QVERIFY(editor.exists("0010,0010"));
    QVERIFY(editor.exists(QString("0010,0010").toUpper()));
    QCOMPARE(DicomTagEditor::pathFor(0x0010, 0x0010), QString("0010,0010"));
    QCOMPARE(DicomTagEditor::pathFor(0x7FE0, 0x0010), QString("7fe0,0010"));
    QVERIFY(editor.value("zzzz,zzzz").isEmpty());
}

DECLARE_TEST(test_DicomTagEditor)

#include "test_dicomtageditor.moc"
