/*@
    "name": "test_HangingProtocolFiles",
    "requirements": []
 */

#include "autotest.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "hangingprotocolfiles.h"

using namespace udg;

class test_HangingProtocolFiles : public QObject {

    Q_OBJECT

private slots:
    void fileNameFor_ShouldKeepSafeCharacters_data();
    void fileNameFor_ShouldKeepSafeCharacters();

    void isValidPosition_data();
    void isValidPosition();

    void list_ShouldGiveBuiltInFirstThenUserOrderedByName();
    void list_ShouldIgnoreFilesThatAreNotProtocols();
    void list_ShouldHandleMissingFolders();

    void copyToUser_ShouldWriteACopyWithTheNewName();
    void copyToUser_ShouldNotReplaceAFile();
    void copyToUser_ShouldFailForBadInput();

    void removeUserFile_ShouldOnlyDeleteFilesOfTheUserFolder();

private:
    static void writeProtocol(const QString &path, const QString &name, const QString &modality, const QString &priority = "0");
};

void test_HangingProtocolFiles::writeProtocol(const QString &path, const QString &name, const QString &modality, const QString &priority)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QString("<?xml version='1.0' encoding='UTF-8'?>\n<hangingProtocol name=\"%1\" version=\"2\">\n"
                       "<hangingProtocolName>%1</hangingProtocolName><numberScreens>1</numberScreens><protocol>%2</protocol><priority>%3</priority>\n"
                       "<imageSet identifier=\"1\"><type>series</type></imageSet>\n"
                       "<displaySet identifier=\"1\"><imageSetNumber>1</imageSetNumber><position>0.0\\1.0\\1.0\\0.0</position></displaySet>\n"
                       "</hangingProtocol>\n").arg(name, modality, priority).toUtf8());
}

void test_HangingProtocolFiles::fileNameFor_ShouldKeepSafeCharacters_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QString>("expected");

    QTest::newRow("plain") << "Explore CT" << "Explore_CT";
    QTest::newRow("kept") << "my-protocol_2" << "my-protocol_2";
    QTest::newRow("unsafe") << "a/b\\c:d*e" << "a_b_c_d_e";
    QTest::newRow("trimmed") << "  X  " << "X";
    QTest::newRow("accent") << QString::fromUtf8("Cr\xC3\xA2nio") << "Cr_nio";
    QTest::newRow("empty") << "" << "protocol";
    QTest::newRow("only symbols") << "???" << "protocol";
}

void test_HangingProtocolFiles::fileNameFor_ShouldKeepSafeCharacters()
{
    QFETCH(QString, name);
    QFETCH(QString, expected);

    QCOMPARE(HangingProtocolFiles::fileNameFor(name), expected);
}

void test_HangingProtocolFiles::isValidPosition_data()
{
    QTest::addColumn<QString>("position");
    QTest::addColumn<bool>("valid");

    QTest::newRow("whole") << "0.0\\1.0\\1.0\\0.0" << true;
    QTest::newRow("quarter") << "0.25\\1.0\\0.5\\0.66" << true;
    QTest::newRow("three numbers") << "0.0\\1.0\\1.0" << false;
    QTest::newRow("five numbers") << "0\\1\\1\\0\\0" << false;
    QTest::newRow("text") << "a\\b\\c\\d" << false;
    QTest::newRow("out of range") << "0.0\\1.0\\1.5\\0.0" << false;
    QTest::newRow("negative") << "-0.1\\1.0\\1.0\\0.0" << false;
    QTest::newRow("x reversed") << "0.5\\1.0\\0.2\\0.0" << false;
    QTest::newRow("y reversed") << "0.0\\0.2\\1.0\\0.8" << false;
    QTest::newRow("empty") << "" << false;
    QTest::newRow("slash instead of backslash") << "0.0/1.0/1.0/0.0" << false;
}

void test_HangingProtocolFiles::isValidPosition()
{
    QFETCH(QString, position);
    QFETCH(bool, valid);

    QCOMPARE(HangingProtocolFiles::isValidPosition(position), valid);
}

void test_HangingProtocolFiles::list_ShouldGiveBuiltInFirstThenUserOrderedByName()
{
    QTemporaryDir root;
    writeProtocol(root.filePath("builtin/CT/b.xml"), "Beta", "CT", "2");
    writeProtocol(root.filePath("builtin/MR/a.xml"), "alpha", "MR");
    writeProtocol(root.filePath("user/z.xml"), "Aaa mine", "CT");

    QList<HangingProtocolFiles::Entry> entries = HangingProtocolFiles::list({root.filePath("builtin")}, root.filePath("user"));

    QCOMPARE(entries.size(), 3);
    QCOMPARE(entries[0].name, QString("alpha"));
    QVERIFY(!entries[0].isUser);
    QCOMPARE(entries[0].modalities, QStringList() << "MR");
    QCOMPARE(entries[1].name, QString("Beta"));
    QCOMPARE(entries[1].priority, 2.0);
    QCOMPARE(entries[2].name, QString("Aaa mine"));
    QVERIFY(entries[2].isUser);
}

void test_HangingProtocolFiles::list_ShouldIgnoreFilesThatAreNotProtocols()
{
    QTemporaryDir root;
    writeProtocol(root.filePath("user/good.xml"), "Good", "CT");

    QFile bad(root.filePath("user/bad.xml"));
    QVERIFY(bad.open(QIODevice::WriteOnly));
    bad.write("this is not xml");
    bad.close();

    QFile other(root.filePath("user/readme.txt"));
    QVERIFY(other.open(QIODevice::WriteOnly));
    other.write("x");
    other.close();

    QList<HangingProtocolFiles::Entry> entries = HangingProtocolFiles::list(QStringList(), root.filePath("user"));

    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].name, QString("Good"));
}

void test_HangingProtocolFiles::list_ShouldHandleMissingFolders()
{
    QVERIFY(HangingProtocolFiles::list({"C:/this/does/not/exist"}, "C:/neither/does/this").isEmpty());
    QVERIFY(HangingProtocolFiles::list(QStringList(), QString()).isEmpty());
}

void test_HangingProtocolFiles::copyToUser_ShouldWriteACopyWithTheNewName()
{
    QTemporaryDir root;
    writeProtocol(root.filePath("builtin/a.xml"), "Original", "CT", "3");

    QString error;
    QString path = HangingProtocolFiles::copyToUser(root.filePath("builtin/a.xml"), root.filePath("user/deeper"), "My copy", &error);

    QVERIFY2(!path.isEmpty(), qPrintable(error));
    QVERIFY(path.endsWith("My_copy.xml"));

    QList<HangingProtocolFiles::Entry> entries = HangingProtocolFiles::list(QStringList(), root.filePath("user"));
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].name, QString("My copy"));
    QCOMPARE(entries[0].priority, 3.0);
    QVERIFY(entries[0].isUser);

    // The original is not changed
    QCOMPARE(HangingProtocolFiles::list({root.filePath("builtin")}, QString())[0].name, QString("Original"));
}

void test_HangingProtocolFiles::copyToUser_ShouldNotReplaceAFile()
{
    QTemporaryDir root;
    writeProtocol(root.filePath("builtin/a.xml"), "Original", "CT");

    QVERIFY(!HangingProtocolFiles::copyToUser(root.filePath("builtin/a.xml"), root.filePath("user"), "Same", nullptr).isEmpty());

    QString error;
    QVERIFY(HangingProtocolFiles::copyToUser(root.filePath("builtin/a.xml"), root.filePath("user"), "Same", &error).isEmpty());
    QVERIFY(!error.isEmpty());
}

void test_HangingProtocolFiles::copyToUser_ShouldFailForBadInput()
{
    QTemporaryDir root;
    writeProtocol(root.filePath("builtin/a.xml"), "Original", "CT");
    QString error;

    QVERIFY(HangingProtocolFiles::copyToUser(root.filePath("builtin/a.xml"), root.filePath("user"), "   ", &error).isEmpty());
    QVERIFY(HangingProtocolFiles::copyToUser(root.filePath("builtin/a.xml"), QString(), "X", &error).isEmpty());
    QVERIFY(HangingProtocolFiles::copyToUser(root.filePath("builtin/missing.xml"), root.filePath("user"), "X", &error).isEmpty());
    QVERIFY(!QDir(root.filePath("user")).exists());
}

void test_HangingProtocolFiles::removeUserFile_ShouldOnlyDeleteFilesOfTheUserFolder()
{
    QTemporaryDir root;
    writeProtocol(root.filePath("builtin/a.xml"), "Original", "CT");
    writeProtocol(root.filePath("user/mine.xml"), "Mine", "CT");
    QString error;

    QVERIFY(!HangingProtocolFiles::removeUserFile(root.filePath("builtin/a.xml"), root.filePath("user"), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(QFile::exists(root.filePath("builtin/a.xml")));

    // A path that goes out of the folder is not inside it
    QVERIFY(!HangingProtocolFiles::removeUserFile(root.filePath("user/../builtin/a.xml"), root.filePath("user"), nullptr));
    QVERIFY(QFile::exists(root.filePath("builtin/a.xml")));

    // Only XML files
    QFile other(root.filePath("user/notes.txt"));
    QVERIFY(other.open(QIODevice::WriteOnly));
    other.close();
    QVERIFY(!HangingProtocolFiles::removeUserFile(root.filePath("user/notes.txt"), root.filePath("user"), nullptr));

    QVERIFY(HangingProtocolFiles::removeUserFile(root.filePath("user/mine.xml"), root.filePath("user"), &error));
    QVERIFY(!QFile::exists(root.filePath("user/mine.xml")));
}

DECLARE_TEST(test_HangingProtocolFiles)

#include "test_hangingprotocolfiles.moc"
