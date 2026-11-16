/*@
    "name": "test_PacsOAuthTokenProvider",
    "requirements": ["archive.wado"]
 */

#include "autotest.h"
#include "pacsoauthtokenprovider.h"

#include "pacscredentialstore.h"
#include "pacsdevice.h"

#include <QHash>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

using namespace udg;

namespace {

// A token endpoint that runs in the thread of the test: the provider waits with a local event loop, which serves it meanwhile.
class FakeTokenServer : public QObject {
Q_OBJECT

public:
    struct Request
    {
        QByteArray authorization;
        QByteArray body;
    };

    FakeTokenServer()
    {
        connect(&m_server, &QTcpServer::newConnection, this, &FakeTokenServer::onNewConnection);
    }

    bool start()
    {
        return m_server.listen(QHostAddress::LocalHost);
    }

    QUrl getUrl() const
    {
        return QUrl(QString("http://127.0.0.1:%1/oauth/token").arg(m_server.serverPort()));
    }

    int status = 200;
    int expiresIn = 3600;
    bool omitToken = false;
    QList<Request> requests;

private slots:
    void onNewConnection()
    {
        while (QTcpSocket *socket = m_server.nextPendingConnection())
        {
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] { onReadyRead(socket); });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    }

private:
    void onReadyRead(QTcpSocket *socket)
    {
        QByteArray &buffer = m_buffers[socket];
        buffer += socket->readAll();
        int endOfHeaders = buffer.indexOf("\r\n\r\n");

        if (endOfHeaders < 0)
        {
            return;
        }

        QByteArray headers = buffer.left(endOfHeaders);
        int contentLength = 0;
        Request request;

        for (const QByteArray &line : headers.split('\n'))
        {
            QByteArray trimmed = line.trimmed();

            if (trimmed.toLower().startsWith("content-length:"))
            {
                contentLength = trimmed.mid(15).trimmed().toInt();
            }
            else if (trimmed.toLower().startsWith("authorization:"))
            {
                request.authorization = trimmed.mid(14).trimmed();
            }
        }

        if (buffer.size() < endOfHeaders + 4 + contentLength)
        {
            return;
        }

        request.body = buffer.mid(endOfHeaders + 4, contentLength);
        m_buffers.remove(socket);
        requests << request;

        QByteArray body;

        if (status != 200)
        {
            body = "{\"error\":\"invalid_client\"}";
        }
        else if (omitToken)
        {
            body = "{\"token_type\":\"Bearer\"}";
        }
        else
        {
            body = QString("{\"access_token\":\"tok-%1\",\"token_type\":\"Bearer\",\"expires_in\":%2}").arg(requests.size()).arg(expiresIn).toUtf8();
        }

        socket->write(QString("HTTP/1.1 %1 X\r\nContent-Type: application/json\r\nContent-Length: %2\r\nConnection: close\r\n\r\n").arg(status).arg(body.size()).toUtf8()
                      + body);
        socket->disconnectFromHost();
    }

private:
    QTcpServer m_server;
    QHash<QTcpSocket*, QByteArray> m_buffers;
};

PacsDevice createOAuthPacs(const FakeTokenServer &server)
{
    PacsDevice pacs;
    pacs.setType(PacsDevice::Type::Wado);
    pacs.setBaseUri(QUrl("https://pacs.example.org/dicom-web/"));
    pacs.setOAuth(server.getUrl(), "starviewer", "se cret");
    return pacs;
}

}

class test_PacsOAuthTokenProvider : public QObject {
Q_OBJECT

private slots:
    void init();

    void setOAuth_ShouldTakeTheClientFromTheTokenUri_WhenNoClientIsGiven();
    void setOAuth_ShouldTurnOAuthOff_WithAnEmptyTokenUri();

    void getAuthorizationHeader_ShouldGetABearerToken_WithTheClientCredentials();
    void getAuthorizationHeader_ShouldReuseTheToken_UntilItExpires();
    void getAuthorizationHeader_ShouldGetANewToken_AfterForgettingIt();
    void getAuthorizationHeader_ShouldGetANewToken_WhenTheTokenExpired();
    void getAuthorizationHeader_ShouldFail_WhenTheClientIsRefused();
    void getAuthorizationHeader_ShouldFail_WhenTheAnswerHasNoToken();
    void getAuthorizationHeader_ShouldFail_WhenTheTokenServerIsNotThere();

    void forget_ShouldIgnoreHeadersThatAreNotBearer();

    void credentialStore_ShouldGiveABearerToken_ForAnOAuthPacs();
    void credentialStore_ShouldNotGiveTheToken_ToAnotherHost();
};

void test_PacsOAuthTokenProvider::init()
{
    PacsOAuthTokenProvider::clear();
}

void test_PacsOAuthTokenProvider::setOAuth_ShouldTakeTheClientFromTheTokenUri_WhenNoClientIsGiven()
{
    PacsDevice pacs;

    pacs.setOAuth(QUrl("https://id:p%40ss@auth.example.org/token"), QString(), QString());

    QVERIFY(pacs.isOAuthEnabled());
    QCOMPARE(pacs.getOAuthTokenUri(), QUrl("https://auth.example.org/token"));
    QCOMPARE(pacs.getOAuthClientId(), QString("id"));
    QCOMPARE(pacs.getOAuthClientSecret(), QString("p@ss"));
}

void test_PacsOAuthTokenProvider::setOAuth_ShouldTurnOAuthOff_WithAnEmptyTokenUri()
{
    PacsDevice pacs;
    pacs.setOAuth(QUrl("https://auth.example.org/token"), "id", "secret");

    pacs.setOAuth(QUrl(), "id", "secret");

    QVERIFY(!pacs.isOAuthEnabled());
    QVERIFY(pacs.getOAuthClientId().isEmpty());
    QVERIFY(pacs.getOAuthClientSecret().isEmpty());
}

void test_PacsOAuthTokenProvider::getAuthorizationHeader_ShouldGetABearerToken_WithTheClientCredentials()
{
    FakeTokenServer server;
    QVERIFY(server.start());

    QString error;
    QByteArray header = PacsOAuthTokenProvider::getAuthorizationHeader(createOAuthPacs(server), &error);

    QCOMPARE(header, QByteArray("Bearer tok-1"));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(server.requests.size(), 1);
    QCOMPARE(server.requests.first().body, QByteArray("grant_type=client_credentials"));
    // The ID and the secret are form-encoded before they go in the Basic header
    QCOMPARE(server.requests.first().authorization, QByteArray("Basic ") + QByteArray("starviewer:se%20cret").toBase64());
}

void test_PacsOAuthTokenProvider::getAuthorizationHeader_ShouldReuseTheToken_UntilItExpires()
{
    FakeTokenServer server;
    QVERIFY(server.start());
    PacsDevice pacs = createOAuthPacs(server);

    QByteArray first = PacsOAuthTokenProvider::getAuthorizationHeader(pacs);
    QByteArray second = PacsOAuthTokenProvider::getAuthorizationHeader(pacs);

    QCOMPARE(first, second);
    QCOMPARE(server.requests.size(), 1);
}

void test_PacsOAuthTokenProvider::getAuthorizationHeader_ShouldGetANewToken_AfterForgettingIt()
{
    FakeTokenServer server;
    QVERIFY(server.start());
    PacsDevice pacs = createOAuthPacs(server);
    QByteArray first = PacsOAuthTokenProvider::getAuthorizationHeader(pacs);

    PacsOAuthTokenProvider::forget(first);
    QByteArray second = PacsOAuthTokenProvider::getAuthorizationHeader(pacs);

    QCOMPARE(first, QByteArray("Bearer tok-1"));
    QCOMPARE(second, QByteArray("Bearer tok-2"));
    QCOMPARE(server.requests.size(), 2);
}

void test_PacsOAuthTokenProvider::getAuthorizationHeader_ShouldGetANewToken_WhenTheTokenExpired()
{
    FakeTokenServer server;
    server.expiresIn = 1;   // it stops being used half a second after it is got
    QVERIFY(server.start());
    PacsDevice pacs = createOAuthPacs(server);
    QByteArray first = PacsOAuthTokenProvider::getAuthorizationHeader(pacs);

    QThread::msleep(700);
    QByteArray second = PacsOAuthTokenProvider::getAuthorizationHeader(pacs);

    QCOMPARE(first, QByteArray("Bearer tok-1"));
    QCOMPARE(second, QByteArray("Bearer tok-2"));
}

void test_PacsOAuthTokenProvider::getAuthorizationHeader_ShouldFail_WhenTheClientIsRefused()
{
    FakeTokenServer server;
    server.status = 401;
    QVERIFY(server.start());

    QString error;
    QByteArray header = PacsOAuthTokenProvider::getAuthorizationHeader(createOAuthPacs(server), &error);

    QVERIFY(header.isEmpty());
    QVERIFY2(error.contains("refused"), qPrintable(error));
}

void test_PacsOAuthTokenProvider::getAuthorizationHeader_ShouldFail_WhenTheAnswerHasNoToken()
{
    FakeTokenServer server;
    server.omitToken = true;
    QVERIFY(server.start());

    QString error;
    QByteArray header = PacsOAuthTokenProvider::getAuthorizationHeader(createOAuthPacs(server), &error);

    QVERIFY(header.isEmpty());
    QVERIFY2(error.contains("bearer token"), qPrintable(error));
}

void test_PacsOAuthTokenProvider::getAuthorizationHeader_ShouldFail_WhenTheTokenServerIsNotThere()
{
    FakeTokenServer server;
    QVERIFY(server.start());
    PacsDevice pacs = createOAuthPacs(server);
    QTcpServer closed;
    // A port that was just free and has nobody listening on it
    QVERIFY(closed.listen(QHostAddress::LocalHost));
    QUrl deadUrl(QString("http://127.0.0.1:%1/oauth/token").arg(closed.serverPort()));
    closed.close();
    pacs.setOAuth(deadUrl, "starviewer", "se cret");

    QString error;
    QByteArray header = PacsOAuthTokenProvider::getAuthorizationHeader(pacs, &error);

    QVERIFY(header.isEmpty());
    QVERIFY2(error.contains("could not be reached"), qPrintable(error));
}

void test_PacsOAuthTokenProvider::forget_ShouldIgnoreHeadersThatAreNotBearer()
{
    FakeTokenServer server;
    QVERIFY(server.start());
    PacsDevice pacs = createOAuthPacs(server);
    PacsOAuthTokenProvider::getAuthorizationHeader(pacs);

    PacsOAuthTokenProvider::forget("Basic dG9rLTE=");
    PacsOAuthTokenProvider::forget(QByteArray());
    PacsOAuthTokenProvider::getAuthorizationHeader(pacs);

    QCOMPARE(server.requests.size(), 1);
}

void test_PacsOAuthTokenProvider::credentialStore_ShouldGiveABearerToken_ForAnOAuthPacs()
{
    FakeTokenServer server;
    QVERIFY(server.start());
    PacsDevice pacs = createOAuthPacs(server);
    // The login of the PACS is ignored when it uses OAuth
    pacs.setLogin("qa", "qa-only");

    QByteArray header = PacsCredentialStore::getAuthorizationHeader(pacs, QUrl("https://pacs.example.org/dicom-web/studies"));

    QCOMPARE(header, QByteArray("Bearer tok-1"));
}

void test_PacsOAuthTokenProvider::credentialStore_ShouldNotGiveTheToken_ToAnotherHost()
{
    FakeTokenServer server;
    QVERIFY(server.start());
    PacsDevice pacs = createOAuthPacs(server);

    QByteArray header = PacsCredentialStore::getAuthorizationHeader(pacs, QUrl("https://elsewhere.example.net/dicom-web/studies"));

    QVERIFY(header.isEmpty());
    QVERIFY(server.requests.isEmpty());
}

DECLARE_TEST(test_PacsOAuthTokenProvider)

#include "test_pacsoauthtokenprovider.moc"
