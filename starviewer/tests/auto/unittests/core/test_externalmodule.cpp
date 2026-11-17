/*@
    "name": "test_ExternalModule",
    "requirements": []
 */

#include "autotest.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "externalmodule.h"

using namespace udg;

class test_ExternalModule : public QObject {

    Q_OBJECT

private slots:
    void parseManifest_ShouldCheckTheFields_data();
    void parseManifest_ShouldCheckTheFields();
    void loadManifest_ShouldCheckTheProgramAndDiscover();
    void parseMessage_ShouldReadTheProtocol_data();
    void parseMessage_ShouldReadTheProtocol();
    void isInside_ShouldResolveThePath();
    void runRequest_ShouldHaveTheFolders();

    void run_ShouldRunAModuleAndCollectTheResult();
    void run_ShouldRefuseOutputsOutsideTheOutputFolder();
    void run_ShouldFailForAModuleThatBreaksTheProtocol_data();
    void run_ShouldFailForAModuleThatBreaksTheProtocol();
    void run_ShouldStopAModuleThatTakesTooLong();
    void run_ShouldCancelAModule();
    void run_ShouldRefuseBadFolders();

private:
    static QJsonObject manifestJson();
    static QString python();
    /// Makes a module in the folder that runs the python script with a mode, and returns its manifest
    static ModuleManifest makeModule(const QTemporaryDir &dir, const QString &mode, int timeout = 60);
    static bool hasPython();
};

static const char *ModuleScript = R"PY(
import json, os, sys, time

mode = sys.argv[1]
def send(o):
    sys.stdout.write(json.dumps(o) + "\n"); sys.stdout.flush()

request = json.loads(sys.stdin.readline())
inp, out = request["inputFolder"], request["outputFolder"]

if mode == "nohello":
    send({"type": "result", "outputs": []}); sys.exit(0)
send({"type": "hello", "protocol": 1, "name": "test", "version": "1"})
if mode == "garbage":
    sys.stdout.write("this is not json\n"); sys.stdout.flush(); time.sleep(1); sys.exit(0)
if mode == "crash":
    sys.stderr.write("boom: something broke\n"); sys.exit(3)
if mode == "errormsg":
    send({"type": "error", "message": "the model could not be loaded"}); sys.exit(1)
if mode == "slow":
    time.sleep(30); sys.exit(0)
if mode == "cancelable":
    for i in range(300):
        send({"type": "progress", "value": min(i / 300.0, 1.0), "message": "working"})
        time.sleep(0.05)
        import select
    sys.exit(0)
if mode == "outside":
    send({"type": "result", "outputs": [{"path": os.path.join(os.path.dirname(out), "escaped.txt"), "kind": "file"}]}); sys.exit(0)
if mode == "relative":
    send({"type": "result", "outputs": [{"path": "..\\..\\x.txt", "kind": "file"}]}); sys.exit(0)

# ok: counts the files of the input, writes a summary in the output
names = sorted(os.listdir(inp))
send({"type": "progress", "value": 0.25, "message": "reading"})
send({"type": "log", "message": "found %d files" % len(names)})
with open(os.path.join(out, "summary.json"), "w") as f:
    json.dump({"files": names, "parameters": request["parameters"], "env": os.environ.get("STARVIEWER_MODULE")}, f)
send({"type": "progress", "value": 1.0, "message": "done"})
send({"type": "result", "message": "ok", "outputs": [{"path": "summary.json", "kind": "file", "description": "the summary"}]})
)PY";

bool test_ExternalModule::hasPython()
{
    return !python().isEmpty();
}

QString test_ExternalModule::python()
{
    QString found = QStandardPaths::findExecutable("python");
    return found.isEmpty() ? QStandardPaths::findExecutable("python3") : found;
}

QJsonObject test_ExternalModule::manifestJson()
{
    QJsonObject o;
    o["name"] = "Example";
    o["version"] = "1.0";
    o["description"] = "An example";
    o["protocol"] = 1;
    o["executable"] = "run.cmd";
    o["researchOnly"] = true;
    o["network"] = "none";
    o["timeoutSeconds"] = 60;
    return o;
}

ModuleManifest test_ExternalModule::makeModule(const QTemporaryDir &dir, const QString &mode, int timeout)
{
    QString folder = dir.filePath("module-" + mode);
    QDir().mkpath(folder);

    QFile script(folder + "/module.py");
    script.open(QIODevice::WriteOnly);
    script.write(ModuleScript);
    script.close();

#ifdef Q_OS_WIN
    QString launcher = "run.cmd";
    QFile run(folder + "/" + launcher);
    run.open(QIODevice::WriteOnly);
    run.write(QString("@\"%1\" \"%~dp0module.py\" %*\r\n").arg(QDir::toNativeSeparators(python())).toUtf8());
#else
    QString launcher = "run.sh";
    QFile run(folder + "/" + launcher);
    run.open(QIODevice::WriteOnly);
    run.write(QString("#!/bin/sh\nexec \"%1\" \"$(dirname \"$0\")/module.py\" \"$@\"\n").arg(python()).toUtf8());
    run.setPermissions(run.permissions() | QFileDevice::ExeOwner);
#endif
    run.close();

    QJsonObject json = manifestJson();
    json["executable"] = launcher;
    json["arguments"] = QJsonArray::fromStringList(QStringList() << mode);
    json["timeoutSeconds"] = timeout;
    QFile manifest(folder + "/module.json");
    manifest.open(QIODevice::WriteOnly);
    manifest.write(QJsonDocument(json).toJson());
    manifest.close();

    ModuleManifest loaded;
    QStringList problems;
    bool ok = ExternalModules::loadManifest(folder, &loaded, &problems);
    Q_ASSERT_X(ok, "makeModule", qPrintable(problems.join("; ")));
    Q_UNUSED(ok);
    return loaded;
}

void test_ExternalModule::parseManifest_ShouldCheckTheFields_data()
{
    QTest::addColumn<QString>("field");
    QTest::addColumn<QJsonValue>("value");
    QTest::addColumn<bool>("valid");

    QTest::newRow("as it is") << "name" << QJsonValue("Example") << true;
    QTest::newRow("no name") << "name" << QJsonValue("  ") << false;
    QTest::newRow("no version") << "version" << QJsonValue("") << false;
    QTest::newRow("protocol 2") << "protocol" << QJsonValue(2) << false;
    QTest::newRow("no protocol") << "protocol" << QJsonValue(0) << false;
    QTest::newRow("executable up") << "executable" << QJsonValue("../evil.exe") << false;
    QTest::newRow("executable inside up") << "executable" << QJsonValue("bin/../../evil.exe") << false;
    QTest::newRow("executable absolute") << "executable" << QJsonValue("C:/Windows/system32/cmd.exe") << false;
    QTest::newRow("executable absolute unix") << "executable" << QJsonValue("/usr/bin/python3") << false;
    QTest::newRow("executable in a folder") << "executable" << QJsonValue("bin/tool.exe") << true;
    QTest::newRow("timeout 0") << "timeoutSeconds" << QJsonValue(0) << false;
    QTest::newRow("timeout too long") << "timeoutSeconds" << QJsonValue(90000) << false;
    QTest::newRow("timeout one hour") << "timeoutSeconds" << QJsonValue(3600) << true;
    QTest::newRow("network empty") << "network" << QJsonValue("") << false;
    QTest::newRow("network says") << "network" << QJsonValue("downloads the model from example.org") << true;
}

void test_ExternalModule::parseManifest_ShouldCheckTheFields()
{
    QFETCH(QString, field);
    QFETCH(QJsonValue, value);
    QFETCH(bool, valid);

    QJsonObject json = manifestJson();
    json[field] = value;
    ModuleManifest manifest;
    QStringList problems;

    QCOMPARE(ExternalModules::parseManifest(json, &manifest, &problems), valid);
    QCOMPARE(problems.isEmpty(), valid);

    if (valid)
    {
        QVERIFY(manifest.researchOnly);
    }
}

void test_ExternalModule::loadManifest_ShouldCheckTheProgramAndDiscover()
{
    QTemporaryDir dir;
    QDir().mkpath(dir.filePath("modules/good"));
    QDir().mkpath(dir.filePath("modules/missing-program"));
    QDir().mkpath(dir.filePath("modules/broken-json"));
    QDir().mkpath(dir.filePath("modules/no-manifest"));

    auto write = [&](const QString &path, const QByteArray &bytes) {
        QFile f(dir.filePath(path));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(bytes);
    };

    QJsonObject good = manifestJson();
    good["name"] = "Zeta";
    write("modules/good/module.json", QJsonDocument(good).toJson());
    write("modules/good/run.cmd", "x");
    QJsonObject missing = manifestJson();
    missing["name"] = "Alpha";
    write("modules/missing-program/module.json", QJsonDocument(missing).toJson());
    write("modules/broken-json/module.json", "{ nope");

    ModuleManifest manifest;
    QStringList problems;
    QVERIFY2(ExternalModules::loadManifest(dir.filePath("modules/good"), &manifest, &problems), qPrintable(problems.join("; ")));
    QCOMPARE(manifest.name, QString("Zeta"));
    QVERIFY(manifest.executablePath().endsWith("modules/good/run.cmd"));
    QVERIFY(!ExternalModules::loadManifest(dir.filePath("modules/missing-program"), &manifest, &problems));
    QVERIFY(problems.first().contains("does not exist"));
    QVERIFY(!ExternalModules::loadManifest(dir.filePath("modules/broken-json"), &manifest, &problems));
    QVERIFY(!ExternalModules::loadManifest(dir.filePath("modules/no-manifest"), &manifest, &problems));

    QStringList discoveryProblems;
    QList<ModuleManifest> found = ExternalModules::discover({dir.filePath("modules"), dir.filePath("nowhere")}, &discoveryProblems);
    QCOMPARE(found.size(), 1);
    QCOMPARE(found[0].name, QString("Zeta"));
    QCOMPARE(discoveryProblems.size(), 2);
}

void test_ExternalModule::parseMessage_ShouldReadTheProtocol_data()
{
    QTest::addColumn<QByteArray>("line");
    QTest::addColumn<int>("type");

    typedef ExternalModules::Message M;
    QTest::newRow("hello") << QByteArray("{\"type\":\"hello\",\"protocol\":1,\"name\":\"x\",\"version\":\"1\"}") << int(M::Hello);
    QTest::newRow("hello protocol 2") << QByteArray("{\"type\":\"hello\",\"protocol\":2}") << int(M::Invalid);
    QTest::newRow("hello no protocol") << QByteArray("{\"type\":\"hello\"}") << int(M::Invalid);
    QTest::newRow("progress") << QByteArray("{\"type\":\"progress\",\"value\":0.5,\"message\":\"half\"}") << int(M::Progress);
    QTest::newRow("progress one") << QByteArray("{\"type\":\"progress\",\"value\":1}") << int(M::Progress);
    QTest::newRow("progress over") << QByteArray("{\"type\":\"progress\",\"value\":1.5}") << int(M::Invalid);
    QTest::newRow("progress negative") << QByteArray("{\"type\":\"progress\",\"value\":-0.1}") << int(M::Invalid);
    QTest::newRow("progress text") << QByteArray("{\"type\":\"progress\",\"value\":\"half\"}") << int(M::Invalid);
    QTest::newRow("log") << QByteArray("{\"type\":\"log\",\"message\":\"hi\"}") << int(M::Log);
    QTest::newRow("error") << QByteArray("{\"type\":\"error\",\"message\":\"bad\"}") << int(M::Error);
    QTest::newRow("result") << QByteArray("{\"type\":\"result\",\"outputs\":[{\"path\":\"a.txt\",\"kind\":\"file\"}]}") << int(M::Result);
    QTest::newRow("result empty") << QByteArray("{\"type\":\"result\"}") << int(M::Result);
    QTest::newRow("result bad kind") << QByteArray("{\"type\":\"result\",\"outputs\":[{\"path\":\"a\",\"kind\":\"socket\"}]}") << int(M::Invalid);
    QTest::newRow("result no path") << QByteArray("{\"type\":\"result\",\"outputs\":[{\"kind\":\"file\"}]}") << int(M::Invalid);
    QTest::newRow("unknown type") << QByteArray("{\"type\":\"launch\"}") << int(M::Invalid);
    QTest::newRow("not json") << QByteArray("hello there") << int(M::Invalid);
    QTest::newRow("json array") << QByteArray("[1,2]") << int(M::Invalid);
    QTest::newRow("empty") << QByteArray("") << int(M::Invalid);
    QTest::newRow("too long") << (QByteArray("{\"type\":\"log\",\"message\":\"") + QByteArray(1100000, 'x') + "\"}") << int(M::Invalid);
}

void test_ExternalModule::parseMessage_ShouldReadTheProtocol()
{
    QFETCH(QByteArray, line);
    QFETCH(int, type);

    ExternalModules::Message message = ExternalModules::parseMessage(line);
    QCOMPARE(int(message.type), type);

    if (type == int(ExternalModules::Message::Invalid))
    {
        QVERIFY(!message.text.isEmpty());
    }
}

void test_ExternalModule::isInside_ShouldResolveThePath()
{
    QTemporaryDir dir;
    QDir().mkpath(dir.filePath("out"));
    QDir().mkpath(dir.filePath("out2"));
    QString out = dir.filePath("out");

    QVERIFY(ExternalModules::isInside(out + "/a.txt", out));
    QVERIFY(ExternalModules::isInside(out + "/new/folder/a.txt", out));
    QVERIFY(ExternalModules::isInside(out, out));
    QVERIFY(!ExternalModules::isInside(out + "/../out2/a.txt", out));
    QVERIFY(!ExternalModules::isInside(dir.filePath("out2/a.txt"), out));
    QVERIFY(!ExternalModules::isInside(dir.filePath("outside.txt"), out));
    QVERIFY(!ExternalModules::isInside(dir.path(), out));
    QVERIFY(!ExternalModules::isInside(out + "/sub/../../escaped.txt", out));
}

void test_ExternalModule::runRequest_ShouldHaveTheFolders()
{
    QJsonObject parameters;
    parameters["threshold"] = 0.5;
    QByteArray request = ExternalModules::runRequest("in", "out", parameters);

    QVERIFY(request.endsWith("\n"));
    QCOMPARE(request.count('\n'), 1);
    QJsonObject o = QJsonDocument::fromJson(request).object();
    QCOMPARE(o["type"].toString(), QString("run"));
    QCOMPARE(o["protocol"].toInt(), 1);
    QVERIFY(QDir::isAbsolutePath(o["inputFolder"].toString()));
    QVERIFY(o["outputFolder"].toString().endsWith("out"));
    QCOMPARE(o["parameters"].toObject()["threshold"].toDouble(), 0.5);
}

void test_ExternalModule::run_ShouldRunAModuleAndCollectTheResult()
{
    if (!hasPython())
    {
        QSKIP("python is not on the PATH");
    }

    QTemporaryDir dir;
    ModuleManifest manifest = makeModule(dir, "ok");
    QDir().mkpath(dir.filePath("in"));
    QFile(dir.filePath("in/a.dcm")).open(QIODevice::WriteOnly);
    QFile(dir.filePath("in/b.dcm")).open(QIODevice::WriteOnly);

    QList<double> progress;
    QJsonObject parameters;
    parameters["threshold"] = 0.3;
    ModuleRunner::Result result = ModuleRunner::run(manifest, dir.filePath("in"), dir.filePath("out"), parameters, [&](double value, const QString &) {
        progress << value;
        return true;
    });

    QVERIFY2(result.success, qPrintable(result.message));
    QCOMPARE(result.message, QString("ok"));
    QCOMPARE(progress, QList<double>() << 0.25 << 1.0);
    QCOMPARE(result.log, QStringList() << "found 2 files");
    QCOMPARE(result.outputs.size(), 1);
    QVERIFY(QFile::exists(result.outputs[0].path));
    QVERIFY(ExternalModules::isInside(result.outputs[0].path, dir.filePath("out")));
    QCOMPARE(result.outputs[0].description, QString("the summary"));

    QFile summary(result.outputs[0].path);
    QVERIFY(summary.open(QIODevice::ReadOnly));
    QJsonObject o = QJsonDocument::fromJson(summary.readAll()).object();
    QCOMPARE(o["files"].toArray().size(), 2);
    QCOMPARE(o["parameters"].toObject()["threshold"].toDouble(), 0.3);
    QCOMPARE(o["env"].toString(), QString("1"));
}

void test_ExternalModule::run_ShouldRefuseOutputsOutsideTheOutputFolder()
{
    if (!hasPython())
    {
        QSKIP("python is not on the PATH");
    }

    QTemporaryDir dir;
    QDir().mkpath(dir.filePath("in"));

    for (const QString &mode : {QString("outside"), QString("relative")})
    {
        ModuleRunner::Result result = ModuleRunner::run(makeModule(dir, mode), dir.filePath("in"), dir.filePath("out"));
        QVERIFY(!result.success);
        QVERIFY2(result.message.contains("not inside the output folder"), qPrintable(result.message));
        QVERIFY(result.outputs.isEmpty());
    }
}

void test_ExternalModule::run_ShouldFailForAModuleThatBreaksTheProtocol_data()
{
    QTest::addColumn<QString>("mode");
    QTest::addColumn<QString>("expected");

    QTest::newRow("no hello") << "nohello" << "hello first";
    QTest::newRow("garbage") << "garbage" << "not in the protocol";
    QTest::newRow("crash") << "crash" << "boom";
    QTest::newRow("error message") << "errormsg" << "the model could not be loaded";
}

void test_ExternalModule::run_ShouldFailForAModuleThatBreaksTheProtocol()
{
    if (!hasPython())
    {
        QSKIP("python is not on the PATH");
    }

    QFETCH(QString, mode);
    QFETCH(QString, expected);

    QTemporaryDir dir;
    QDir().mkpath(dir.filePath("in"));
    ModuleRunner::Result result = ModuleRunner::run(makeModule(dir, mode), dir.filePath("in"), dir.filePath("out"));

    QVERIFY(!result.success);
    QVERIFY2(result.message.contains(expected), qPrintable(result.message));
}

void test_ExternalModule::run_ShouldStopAModuleThatTakesTooLong()
{
    if (!hasPython())
    {
        QSKIP("python is not on the PATH");
    }

    QTemporaryDir dir;
    QDir().mkpath(dir.filePath("in"));
    QElapsedTimer timer;
    timer.start();
    ModuleRunner::Result result = ModuleRunner::run(makeModule(dir, "slow", 1), dir.filePath("in"), dir.filePath("out"));

    QVERIFY(!result.success);
    QVERIFY2(result.message.contains("took more than 1 seconds"), qPrintable(result.message));
    QVERIFY2(timer.elapsed() < 15000, qPrintable(QString::number(timer.elapsed())));
}

void test_ExternalModule::run_ShouldCancelAModule()
{
    if (!hasPython())
    {
        QSKIP("python is not on the PATH");
    }

    QTemporaryDir dir;
    QDir().mkpath(dir.filePath("in"));
    int calls = 0;
    QElapsedTimer timer;
    timer.start();
    ModuleRunner::Result result = ModuleRunner::run(makeModule(dir, "cancelable"), dir.filePath("in"), dir.filePath("out"), QJsonObject(), [&](double, const QString &) {
        calls++;
        return calls < 3;
    });

    QVERIFY(!result.success);
    QCOMPARE(result.message, QString("Cancelled."));
    QVERIFY(calls >= 3 && calls < 100);
    // The module does not read the cancel message, so it is killed after the wait
    QVERIFY2(timer.elapsed() < 20000, qPrintable(QString::number(timer.elapsed())));
}

void test_ExternalModule::run_ShouldRefuseBadFolders()
{
    ModuleManifest manifest;
    manifest.name = "x";
    manifest.executable = "run.cmd";
    manifest.directory = QDir::tempPath();
    QTemporaryDir dir;
    QDir().mkpath(dir.filePath("in"));

    QVERIFY(!ModuleRunner::run(manifest, dir.filePath("missing"), dir.filePath("out")).success);
    QVERIFY(!ModuleRunner::run(manifest, dir.filePath("in"), "").success);
    QVERIFY(!ModuleRunner::run(manifest, dir.filePath("in"), dir.filePath("in/out")).success);
    QVERIFY(!ModuleRunner::run(manifest, dir.filePath("in"), dir.filePath("in")).success);
    QVERIFY(!ModuleRunner::run(manifest, dir.filePath("in/sub"), dir.path()).success);

    // A program that does not exist is a failure to start
    ModuleRunner::Result result = ModuleRunner::run(manifest, dir.filePath("in"), dir.filePath("out"));
    QVERIFY(!result.success);
    QVERIFY(result.message.contains("could not be started"));
}

DECLARE_TEST(test_ExternalModule)

#include "test_externalmodule.moc"
