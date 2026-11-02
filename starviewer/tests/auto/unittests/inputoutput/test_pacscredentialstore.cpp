/*@
    "name": "test_PacsCredentialStore",
    "requirements": ["archive.wado"]
 */

#include "autotest.h"
#include "pacscredentialstore.h"

#include "pacsdevice.h"

#include <QHash>
#include <QUuid>

using namespace udg;

namespace {

// Keeps logins in memory, so that tests don't touch the secure store of the machine. It can refuse them, like a machine where the store is not available.
class FakeCredentialBackend : public PacsCredentialBackend {
public:
    bool refuse = false;
    QHash<QString, PacsLogin> logins;

    bool save(const QString &key, const PacsLogin &login) override
    {
        if (!refuse)
        {
            logins.insert(key, login);
        }
        return !refuse;
    }

    bool load(const QString &key, PacsLogin &login) override
    {
        login = logins.value(key);
        return logins.contains(key);
    }

    void remove(const QString &key) override
    {
        logins.remove(key);
    }
};

const QUrl BaseUri("http://example.com:8042/dicom-web/");

PacsDevice createWadoPacs()
{
    PacsDevice pacs;
    pacs.setType(PacsDevice::Type::Wado);
    pacs.setBaseUri(BaseUri);
    return pacs;
}

}

class test_PacsCredentialStore : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void getKey_ShouldReturnExpectedKey_data();
    void getKey_ShouldReturnExpectedKey();

    void save_ShouldKeepTheLoginInTheBackend();
    void save_ShouldKeepTheLoginOnlyForThisRun_WhenTheBackendRefusesIt();
    void save_ShouldForgetTheLoginOfThisRun_WhenTheBackendAcceptsIt();

    void remove_ShouldRemoveTheLoginEverywhere();

    void getLoginFor_ShouldPreferTheLoginOfThePacs();
    void getLoginFor_ShouldUseTheStoredLogin_WhenThePacsSpecifiesNone();
    void getLoginFor_ShouldReturnEmpty_WhenThePacsSpecifiesNoLoginExplicitly();

    void getAuthorizationHeader_ShouldEncodeTheLogin_data();
    void getAuthorizationHeader_ShouldEncodeTheLogin();

    void getAuthorizationHeader_ShouldBeEmpty_WhenThereIsNoLogin();

    void getAuthorizationHeader_ShouldBeEmpty_WhenTheUrlIsOnAnotherServer_data();
    void getAuthorizationHeader_ShouldBeEmpty_WhenTheUrlIsOnAnotherServer();

    void platformBackend_ShouldKeepAndRemoveALogin();

private:
    FakeCredentialBackend m_backend;
};

void test_PacsCredentialStore::init()
{
    m_backend.logins.clear();
    m_backend.refuse = false;
    PacsCredentialStore::setStaticTestingBackend(&m_backend);
}

void test_PacsCredentialStore::cleanup()
{
    PacsCredentialStore::setStaticTestingBackend(nullptr);
}

void test_PacsCredentialStore::getKey_ShouldReturnExpectedKey_data()
{
    QTest::addColumn<QUrl>("baseUri");
    QTest::addColumn<QString>("expectedKey");

    QTest::newRow("trailing slash") << QUrl("http://example.com:8042/dicom-web/") << "http://example.com:8042/dicom-web";
    QTest::newRow("no trailing slash") << QUrl("http://example.com:8042/dicom-web") << "http://example.com:8042/dicom-web";
    QTest::newRow("login") << QUrl("http://user:secret@example.com:8042/dicom-web/") << "http://example.com:8042/dicom-web";
    QTest::newRow("host case") << QUrl("http://EXAMPLE.com/dicom-web") << "http://example.com/dicom-web";
    QTest::newRow("scheme kept") << QUrl("https://example.com/dicom-web") << "https://example.com/dicom-web";
}

void test_PacsCredentialStore::getKey_ShouldReturnExpectedKey()
{
    QFETCH(QUrl, baseUri);
    QFETCH(QString, expectedKey);

    QCOMPARE(PacsCredentialStore::getKey(baseUri), expectedKey);
}

void test_PacsCredentialStore::save_ShouldKeepTheLoginInTheBackend()
{
    QVERIFY(PacsCredentialStore::save(BaseUri, PacsLogin{"user", "secret"}));

    QVERIFY(m_backend.logins.contains(PacsCredentialStore::getKey(BaseUri)));
    QVERIFY(!PacsCredentialStore::isKeptOnlyForThisRun(BaseUri));
    QCOMPARE(PacsCredentialStore::load(BaseUri).username, QString("user"));
    QCOMPARE(PacsCredentialStore::load(BaseUri).password, QString("secret"));
}

void test_PacsCredentialStore::save_ShouldKeepTheLoginOnlyForThisRun_WhenTheBackendRefusesIt()
{
    m_backend.refuse = true;

    QVERIFY(!PacsCredentialStore::save(BaseUri, PacsLogin{"user", "secret"}));

    QVERIFY(m_backend.logins.isEmpty());
    QVERIFY(PacsCredentialStore::isKeptOnlyForThisRun(BaseUri));
    QCOMPARE(PacsCredentialStore::load(BaseUri).password, QString("secret"));
}

void test_PacsCredentialStore::save_ShouldForgetTheLoginOfThisRun_WhenTheBackendAcceptsIt()
{
    m_backend.refuse = true;
    PacsCredentialStore::save(BaseUri, PacsLogin{"user", "old"});
    m_backend.refuse = false;

    QVERIFY(PacsCredentialStore::save(BaseUri, PacsLogin{"user", "new"}));

    QVERIFY(!PacsCredentialStore::isKeptOnlyForThisRun(BaseUri));
    QCOMPARE(PacsCredentialStore::load(BaseUri).password, QString("new"));
}

void test_PacsCredentialStore::remove_ShouldRemoveTheLoginEverywhere()
{
    PacsCredentialStore::save(BaseUri, PacsLogin{"user", "secret"});
    PacsCredentialStore::remove(BaseUri);
    QVERIFY(PacsCredentialStore::load(BaseUri).isEmpty());

    m_backend.refuse = true;
    PacsCredentialStore::save(BaseUri, PacsLogin{"user", "secret"});
    PacsCredentialStore::remove(BaseUri);
    QVERIFY(PacsCredentialStore::load(BaseUri).isEmpty());
    QVERIFY(!PacsCredentialStore::isKeptOnlyForThisRun(BaseUri));
}

void test_PacsCredentialStore::getLoginFor_ShouldPreferTheLoginOfThePacs()
{
    PacsCredentialStore::save(BaseUri, PacsLogin{"stored", "storedSecret"});
    PacsDevice pacs = createWadoPacs();
    pacs.setLogin("typed", "typedSecret");

    QCOMPARE(PacsCredentialStore::getLoginFor(pacs).username, QString("typed"));
    QCOMPARE(PacsCredentialStore::getLoginFor(pacs).password, QString("typedSecret"));
}

void test_PacsCredentialStore::getLoginFor_ShouldUseTheStoredLogin_WhenThePacsSpecifiesNone()
{
    PacsCredentialStore::save(BaseUri, PacsLogin{"stored", "storedSecret"});

    QCOMPARE(PacsCredentialStore::getLoginFor(createWadoPacs()).username, QString("stored"));
}

void test_PacsCredentialStore::getLoginFor_ShouldReturnEmpty_WhenThePacsSpecifiesNoLoginExplicitly()
{
    PacsCredentialStore::save(BaseUri, PacsLogin{"stored", "storedSecret"});
    PacsDevice pacs = createWadoPacs();
    pacs.setLogin(QString(), QString());

    QVERIFY(PacsCredentialStore::getLoginFor(pacs).isEmpty());
}

void test_PacsCredentialStore::getAuthorizationHeader_ShouldEncodeTheLogin_data()
{
    QTest::addColumn<QString>("username");
    QTest::addColumn<QString>("password");
    QTest::addColumn<QByteArray>("expectedHeader");

    QTest::newRow("ascii") << "Aladdin" << "open sesame" << QByteArray("Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==");     // example in RFC 7617
    QTest::newRow("colon in password") << "a" << "b:c" << QByteArray("Basic YTpiOmM=");
    QTest::newRow("non ascii") << "test" << QString::fromUtf8("\xC2\xA3") << QByteArray("Basic dGVzdDrCow==");   // RFC 7617 asks for UTF-8 here
}

void test_PacsCredentialStore::getAuthorizationHeader_ShouldEncodeTheLogin()
{
    QFETCH(QString, username);
    QFETCH(QString, password);
    QFETCH(QByteArray, expectedHeader);

    PacsCredentialStore::save(BaseUri, PacsLogin{username, password});

    QCOMPARE(PacsCredentialStore::getAuthorizationHeader(createWadoPacs(), QUrl("http://example.com:8042/dicom-web/studies")), expectedHeader);
}

void test_PacsCredentialStore::getAuthorizationHeader_ShouldBeEmpty_WhenThereIsNoLogin()
{
    QVERIFY(PacsCredentialStore::getAuthorizationHeader(createWadoPacs(), QUrl("http://example.com:8042/dicom-web/studies")).isEmpty());
}

void test_PacsCredentialStore::getAuthorizationHeader_ShouldBeEmpty_WhenTheUrlIsOnAnotherServer_data()
{
    QTest::addColumn<QUrl>("url");

    QTest::newRow("other host") << QUrl("http://other.example.com:8042/dicom-web/studies");
    QTest::newRow("other port") << QUrl("http://example.com:8043/dicom-web/studies");
    QTest::newRow("other scheme") << QUrl("https://example.com:8042/dicom-web/studies");
}

void test_PacsCredentialStore::getAuthorizationHeader_ShouldBeEmpty_WhenTheUrlIsOnAnotherServer()
{
    QFETCH(QUrl, url);

    PacsCredentialStore::save(BaseUri, PacsLogin{"user", "secret"});

    QVERIFY(PacsCredentialStore::getAuthorizationHeader(createWadoPacs(), url).isEmpty());
}

void test_PacsCredentialStore::platformBackend_ShouldKeepAndRemoveALogin()
{
    QScopedPointer<PacsCredentialBackend> backend(PacsCredentialStore::createPlatformBackend());

    if (!backend)
    {
        QSKIP("This platform has no secure store implemented.");
    }

    const QString key = "autotest/" + QUuid::createUuid().toString();

    if (!backend->save(key, PacsLogin{"user", QString::fromUtf8("s\xC3\xA9" "cret")}))
    {
        QSKIP("The secure store is not available in this session.");
    }

    PacsLogin login;
    bool found = backend->load(key, login);
    backend->remove(key);

    QVERIFY(found);
    QCOMPARE(login.username, QString("user"));
    QCOMPARE(login.password, QString::fromUtf8("s\xC3\xA9" "cret"));

    PacsLogin loginAfterRemoval;
    QVERIFY(!backend->load(key, loginAfterRemoval));
}

DECLARE_TEST(test_PacsCredentialStore)

#include "test_pacscredentialstore.moc"
