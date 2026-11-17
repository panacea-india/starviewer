/*@
    "name": "test_ZipArchive",
    "requirements": []
 */

#include "autotest.h"
#include "ziparchive.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "itk_zlib.h"

using namespace udg;

class test_ZipArchive : public QObject {

    Q_OBJECT

private slots:
    void extract_ShouldWriteStoredAndDeflatedFiles();
    void extract_ShouldCreateSubfolders();
    void extract_ShouldRefuseNamesThatGoOutside_data();
    void extract_ShouldRefuseNamesThatGoOutside();
    void extract_ShouldFail_WhenTheCrcIsWrong();
    void extract_ShouldFail_WhenAnEntryHoldsMoreThanItSays();
    void extract_ShouldFail_WhenTheLimitsAreExceeded();
    void extract_ShouldRefuseEncryptedAndDuplicateEntries();
    void extract_ShouldFail_ForFilesThatAreNotZip();
    void safeRelativePath_ShouldAcceptOrdinaryPaths();
    void removeOldExtractions_ShouldRemoveOnlyTheOldFolders();
};

namespace {

struct Item {
    QString name;
    QByteArray data;
    bool deflate = false;
    quint16 flags = 0;
    // To make a lie: the size that the entry declares, and the crc, if different from the real ones
    qint64 declaredSize = -1;
    qint64 crcOverride = -1;
};

void put16(QByteArray &b, quint16 v) { b.append(char(v & 0xFF)); b.append(char(v >> 8)); }
void put32(QByteArray &b, quint32 v) { put16(b, v & 0xFFFF); put16(b, v >> 16); }

QByteArray rawDeflate(const QByteArray &data)
{
    z_stream s;
    memset(&s, 0, sizeof(s));
    deflateInit2(&s, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    QByteArray out(int(deflateBound(&s, data.size())) + 16, 0);
    s.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.constData()));
    s.avail_in = data.size();
    s.next_out = reinterpret_cast<Bytef*>(out.data());
    s.avail_out = out.size();
    deflate(&s, Z_FINISH);
    out.resize(int(s.total_out));
    deflateEnd(&s);
    return out;
}

QString makeZip(const QString &path, const QList<Item> &items)
{
    QByteArray body;
    QByteArray directory;

    for (const Item &item : items)
    {
        quint32 crc = crc32(crc32(0L, Z_NULL, 0), reinterpret_cast<const Bytef*>(item.data.constData()), item.data.size());

        if (item.crcOverride >= 0)
        {
            crc = quint32(item.crcOverride);
        }

        QByteArray stored = item.deflate ? rawDeflate(item.data) : item.data;
        quint32 usize = item.declaredSize >= 0 ? quint32(item.declaredSize) : quint32(item.data.size());
        QByteArray name = item.name.toUtf8();
        quint32 offset = body.size();

        put32(body, 0x04034b50); put16(body, 20); put16(body, item.flags); put16(body, item.deflate ? 8 : 0); put32(body, 0);
        put32(body, crc); put32(body, stored.size()); put32(body, usize); put16(body, name.size()); put16(body, 0);
        body.append(name); body.append(stored);

        put32(directory, 0x02014b50); put16(directory, 20); put16(directory, 20); put16(directory, item.flags); put16(directory, item.deflate ? 8 : 0);
        put32(directory, 0); put32(directory, crc); put32(directory, stored.size()); put32(directory, usize); put16(directory, name.size());
        put16(directory, 0); put16(directory, 0); put16(directory, 0); put16(directory, 0); put32(directory, 0); put32(directory, offset);
        directory.append(name);
    }

    QByteArray all = body;
    quint32 directoryOffset = all.size();
    all.append(directory);
    put32(all, 0x06054b50); put16(all, 0); put16(all, 0); put16(all, items.size()); put16(all, items.size());
    put32(all, directory.size()); put32(all, directoryOffset); put16(all, 0);

    QFile file(path);
    file.open(QIODevice::WriteOnly);
    file.write(all);
    return path;
}

QByteArray readAll(const QString &path)
{
    QFile file(path);
    file.open(QIODevice::ReadOnly);
    return file.readAll();
}

}

void test_ZipArchive::extract_ShouldWriteStoredAndDeflatedFiles()
{
    QTemporaryDir dir;
    QByteArray big(100000, 'x');
    makeZip(dir.filePath("a.zip"), {{"one.dcm", "hello", false}, {"two.dcm", big, true}});

    ZipArchive::Result result = ZipArchive::extract(dir.filePath("a.zip"), dir.filePath("out"));

    QVERIFY2(result.success, qPrintable(result.message));
    QCOMPARE(result.files.size(), 2);
    QCOMPARE(readAll(dir.filePath("out/one.dcm")), QByteArray("hello"));
    QCOMPARE(readAll(dir.filePath("out/two.dcm")), big);
    QCOMPARE(result.bytes, qint64(100005));
}

void test_ZipArchive::extract_ShouldCreateSubfolders()
{
    QTemporaryDir dir;
    makeZip(dir.filePath("a.zip"), {{"study/", "", false}, {"study/series/img.dcm", "data", true}});

    QVERIFY(ZipArchive::extract(dir.filePath("a.zip"), dir.filePath("out")).success);
    QCOMPARE(readAll(dir.filePath("out/study/series/img.dcm")), QByteArray("data"));
}

void test_ZipArchive::extract_ShouldRefuseNamesThatGoOutside_data()
{
    QTest::addColumn<QString>("name");
    QTest::newRow("parent") << "../evil.txt";
    QTest::newRow("deep parent") << "a/../../evil.txt";
    QTest::newRow("backslash parent") << "..\\evil.txt";
    QTest::newRow("absolute") << "/etc/evil.txt";
    QTest::newRow("drive") << "C:/evil.txt";
    QTest::newRow("stream") << "file.txt:evil";
    QTest::newRow("double slash") << "a//b.txt";
}

void test_ZipArchive::extract_ShouldRefuseNamesThatGoOutside()
{
    QFETCH(QString, name);
    QTemporaryDir dir;
    makeZip(dir.filePath("a.zip"), {{"good.dcm", "ok", false}, {name, "bad", false}});

    ZipArchive::Result result = ZipArchive::extract(dir.filePath("a.zip"), dir.filePath("sub/out"));

    QVERIFY(!result.success);
    // Nothing was written, not even the good file, and nothing outside
    QVERIFY(!QFile::exists(dir.filePath("sub/out/good.dcm")));
    QVERIFY(!QFile::exists(dir.filePath("sub/evil.txt")));
    QVERIFY(!QFile::exists(dir.filePath("evil.txt")));
}

void test_ZipArchive::extract_ShouldFail_WhenTheCrcIsWrong()
{
    QTemporaryDir dir;
    Item item{"a.dcm", "hello", true};
    item.crcOverride = 12345;
    makeZip(dir.filePath("a.zip"), {item});

    ZipArchive::Result result = ZipArchive::extract(dir.filePath("a.zip"), dir.filePath("out"));
    QVERIFY(!result.success);
    QVERIFY(!QFile::exists(dir.filePath("out/a.dcm")));
}

void test_ZipArchive::extract_ShouldFail_WhenAnEntryHoldsMoreThanItSays()
{
    QTemporaryDir dir;
    // Says 10 bytes, holds 100000: a bomb that lies about its size
    Item item{"a.dcm", QByteArray(100000, 'z'), true};
    item.declaredSize = 10;
    makeZip(dir.filePath("a.zip"), {item});

    QVERIFY(!ZipArchive::extract(dir.filePath("a.zip"), dir.filePath("out")).success);
    QVERIFY(!QFile::exists(dir.filePath("out/a.dcm")));
}

void test_ZipArchive::extract_ShouldFail_WhenTheLimitsAreExceeded()
{
    QTemporaryDir dir;
    makeZip(dir.filePath("a.zip"), {{"a.dcm", QByteArray(1000, 'a'), true}, {"b.dcm", QByteArray(1000, 'b'), true}});

    ZipArchive::Limits total;
    total.maxTotalBytes = 1500;
    QVERIFY(!ZipArchive::extract(dir.filePath("a.zip"), dir.filePath("o1"), total).success);

    ZipArchive::Limits file;
    file.maxFileBytes = 500;
    QVERIFY(!ZipArchive::extract(dir.filePath("a.zip"), dir.filePath("o2"), file).success);

    ZipArchive::Limits entries;
    entries.maxEntries = 1;
    QVERIFY(!ZipArchive::extract(dir.filePath("a.zip"), dir.filePath("o3"), entries).success);
}

void test_ZipArchive::extract_ShouldRefuseEncryptedAndDuplicateEntries()
{
    QTemporaryDir dir;
    Item encrypted{"a.dcm", "x", false};
    encrypted.flags = 1;
    makeZip(dir.filePath("e.zip"), {encrypted});
    QVERIFY(!ZipArchive::extract(dir.filePath("e.zip"), dir.filePath("o1")).success);

    makeZip(dir.filePath("d.zip"), {{"a.dcm", "x", false}, {"A.DCM", "y", false}});
    QVERIFY(!ZipArchive::extract(dir.filePath("d.zip"), dir.filePath("o2")).success);
}

void test_ZipArchive::extract_ShouldFail_ForFilesThatAreNotZip()
{
    QTemporaryDir dir;
    QFile text(dir.filePath("t.zip"));
    text.open(QIODevice::WriteOnly);
    text.write(QByteArray(500, 'q'));
    text.close();

    QVERIFY(!ZipArchive::extract(dir.filePath("t.zip"), dir.filePath("o")).success);
    QVERIFY(!ZipArchive::extract(dir.filePath("missing.zip"), dir.filePath("o")).success);

    // A zip cut in the middle
    makeZip(dir.filePath("c.zip"), {{"a.dcm", QByteArray(5000, 'a'), false}});
    QByteArray bytes = readAll(dir.filePath("c.zip"));
    QFile cut(dir.filePath("cut.zip"));
    cut.open(QIODevice::WriteOnly);
    cut.write(bytes.left(bytes.size() / 2));
    cut.close();
    QVERIFY(!ZipArchive::extract(dir.filePath("cut.zip"), dir.filePath("o")).success);
}

void test_ZipArchive::safeRelativePath_ShouldAcceptOrdinaryPaths()
{
    QCOMPARE(ZipArchive::safeRelativePath("a/b/c.dcm"), QString("a/b/c.dcm"));
    QCOMPARE(ZipArchive::safeRelativePath("a\\b.dcm"), QString("a/b.dcm"));
    QCOMPARE(ZipArchive::safeRelativePath("folder/"), QString("folder"));
    QVERIFY(ZipArchive::safeRelativePath("").isEmpty());
    QVERIFY(ZipArchive::safeRelativePath("a/b ").isEmpty());
    QVERIFY(ZipArchive::safeRelativePath("a/./b").isEmpty());
}

void test_ZipArchive::removeOldExtractions_ShouldRemoveOnlyTheOldFolders()
{
    QTemporaryDir dir;
    QDir(dir.path()).mkpath("one/inner");
    QDir(dir.path()).mkpath("two");
    QFile file(dir.filePath("one/inner/a.dcm"));
    file.open(QIODevice::WriteOnly);
    file.close();

    // Folders that were just made are not old
    QCOMPARE(ZipArchive::removeOldExtractions(dir.path(), 24), 0);
    QVERIFY(QFile::exists(dir.filePath("one/inner/a.dcm")));
    // With a negative age everything is older than the limit (which is in the future): the folders go, with what they hold
    QCOMPARE(ZipArchive::removeOldExtractions(dir.path(), -1), 2);
    QVERIFY(!QFile::exists(dir.filePath("one")));
    QVERIFY(!QFile::exists(dir.filePath("two")));
    QCOMPARE(ZipArchive::removeOldExtractions(dir.path() + "/missing", 24), 0);
}

DECLARE_TEST(test_ZipArchive)

#include "test_ziparchive.moc"
