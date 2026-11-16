/*@
    "name": "test_EchoToWadoPacsOAuth",
    "requirements": ["archive.wado"]
 */

#include "autotest.h"
#include "echotowadopacs.h"

#include "pacsdevice.h"
#include "pacsoauthtokenprovider.h"

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

using namespace udg;

// These tests talk to the OAuth mock of quality/testlab (oauth_mock.py) in front of the test PACS, so they only run when these variables are set.
// See quality/testlab/README.md.
//   STARVIEWER_TESTLAB_OAUTH_URI: DICOMweb base URI of the mock, for example http://127.0.0.1:18044/dicom-web/.
//   STARVIEWER_TESTLAB_OAUTH_TOKEN_URI: its token endpoint, for example http://127.0.0.1:18044/oauth/token.
class test_EchoToWadoPacsOAuth : public QObject {
Q_OBJECT

private slots:
    void init();

    void echo_ShouldSucceed_WithTheRightClient();
    void echo_ShouldSayNoTokenCouldBeGot_WithAWrongSecret();
    void echo_ShouldSayTheTokenWasRefused_AfterItWasRevoked_AndSucceedAfterwards();

private:
    PacsDevice createTestLabPacs(const QString &secret) const;
    void revokeTokens() const;
};

void test_EchoToWadoPacsOAuth::init()
{
    if (!qEnvironmentVariableIsSet("STARVIEWER_TESTLAB_OAUTH_URI") || !qEnvironmentVariableIsSet("STARVIEWER_TESTLAB_OAUTH_TOKEN_URI"))
    {
        QSKIP("The OAuth mock of the test lab is not configured.");
    }

    PacsOAuthTokenProvider::clear();
}

PacsDevice test_EchoToWadoPacsOAuth::createTestLabPacs(const QString &secret) const
{
    PacsDevice pacs;
    pacs.setType(PacsDevice::Type::Wado);
    pacs.setBaseUri(QUrl(qEnvironmentVariable("STARVIEWER_TESTLAB_OAUTH_URI")));
    pacs.setOAuth(QUrl(qEnvironmentVariable("STARVIEWER_TESTLAB_OAUTH_TOKEN_URI")), "starviewer-qa", secret);
    return pacs;
}

void test_EchoToWadoPacsOAuth::revokeTokens() const
{
    QUrl url(qEnvironmentVariable("STARVIEWER_TESTLAB_OAUTH_URI"));
    url.setPath("/mock/revoke");
    QNetworkAccessManager networkAccessManager;
    QNetworkReply *reply = networkAccessManager.post(QNetworkRequest(url), QByteArray());
    QEventLoop eventLoop;
    connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
    eventLoop.exec();
    QCOMPARE(reply->error(), QNetworkReply::NoError);
    reply->deleteLater();
}

void test_EchoToWadoPacsOAuth::echo_ShouldSucceed_WithTheRightClient()
{
    EchoToWadoPacs echo;

    QVERIFY2(echo.echo(createTestLabPacs("qa-only-secret")), qPrintable(echo.getErrors()));
}

void test_EchoToWadoPacsOAuth::echo_ShouldSayNoTokenCouldBeGot_WithAWrongSecret()
{
    EchoToWadoPacs echo;

    QVERIFY(!echo.echo(createTestLabPacs("wrong")));
    QVERIFY2(echo.getErrors().contains("No access token could be got"), qPrintable(echo.getErrors()));
}

void test_EchoToWadoPacsOAuth::echo_ShouldSayTheTokenWasRefused_AfterItWasRevoked_AndSucceedAfterwards()
{
    PacsDevice pacs = createTestLabPacs("qa-only-secret");
    EchoToWadoPacs first;
    QVERIFY2(first.echo(pacs), qPrintable(first.getErrors()));
    revokeTokens();

    EchoToWadoPacs refused;
    QVERIFY(!refused.echo(pacs));
    QVERIFY2(refused.getErrors().contains("refused the access token"), qPrintable(refused.getErrors()));

    EchoToWadoPacs again;
    QVERIFY2(again.echo(pacs), qPrintable(again.getErrors()));
}

DECLARE_TEST(test_EchoToWadoPacsOAuth)

#include "test_echotowadopacsoauth.moc"
