/*@
    "name": "test_EchoToWadoPacs",
    "requirements": ["archive.wado"]
 */

#include "autotest.h"
#include "echotowadopacs.h"

#include "pacsdevice.h"

using namespace udg;

// These tests talk to the test PACS with TLS of quality/testlab, so they only run when these variables are set. See quality/testlab/README.md.
//   STARVIEWER_TESTLAB_HTTPS_URI: DICOMweb base URI, for example https://localhost:18043/dicom-web/.
//   STARVIEWER_TESTLAB_CA: file of the CA that signed the PACS's certificate.
// The slots run in this order, in one process, so the last one shows whether a connection made with the CA is reused without it.
class test_EchoToWadoPacs : public QObject {
Q_OBJECT

private slots:
    void init();

    void echo_ShouldFail_WithoutTheCaCertificate();
    void echo_ShouldSucceed_WithTheCaCertificate();
    void echo_ShouldFail_WithoutTheCaCertificate_AfterSucceedingWithIt();
    void echo_ShouldSayTheLoginWasRefused_WithAWrongPassword();
    void echo_ShouldAskForALogin_WithoutOne();

private:
    PacsDevice createTestLabPacs(const QString &caCertificateFile, const QString &password = "qa-only") const;
};

void test_EchoToWadoPacs::init()
{
    if (!qEnvironmentVariableIsSet("STARVIEWER_TESTLAB_HTTPS_URI"))
    {
        QSKIP("The test PACS with TLS is not configured.");
    }
}

PacsDevice test_EchoToWadoPacs::createTestLabPacs(const QString &caCertificateFile, const QString &password) const
{
    PacsDevice pacs;
    pacs.setType(PacsDevice::Type::Wado);
    pacs.setBaseUri(QUrl(qEnvironmentVariable("STARVIEWER_TESTLAB_HTTPS_URI")));
    pacs.setLogin(password.isEmpty() ? QString() : "qa", password);
    pacs.setCaCertificateFile(caCertificateFile);
    return pacs;
}

void test_EchoToWadoPacs::echo_ShouldFail_WithoutTheCaCertificate()
{
    EchoToWadoPacs echo;
    QVERIFY(!echo.echo(createTestLabPacs(QString())));
    QVERIFY2(echo.getErrors().contains("certificate", Qt::CaseInsensitive), qPrintable(echo.getErrors()));
}

void test_EchoToWadoPacs::echo_ShouldSucceed_WithTheCaCertificate()
{
    EchoToWadoPacs echo;
    QVERIFY2(echo.echo(createTestLabPacs(qEnvironmentVariable("STARVIEWER_TESTLAB_CA"))), qPrintable(echo.getErrors()));
}

void test_EchoToWadoPacs::echo_ShouldFail_WithoutTheCaCertificate_AfterSucceedingWithIt()
{
    EchoToWadoPacs echo;
    QVERIFY(!echo.echo(createTestLabPacs(QString())));
    QVERIFY2(echo.getErrors().contains("certificate", Qt::CaseInsensitive), qPrintable(echo.getErrors()));
}

void test_EchoToWadoPacs::echo_ShouldSayTheLoginWasRefused_WithAWrongPassword()
{
    EchoToWadoPacs echo;
    QVERIFY(!echo.echo(createTestLabPacs(qEnvironmentVariable("STARVIEWER_TESTLAB_CA"), "wrong")));
    QVERIFY2(echo.getErrors().contains("refused the login"), qPrintable(echo.getErrors()));
}

void test_EchoToWadoPacs::echo_ShouldAskForALogin_WithoutOne()
{
    EchoToWadoPacs echo;
    QVERIFY(!echo.echo(createTestLabPacs(qEnvironmentVariable("STARVIEWER_TESTLAB_CA"), QString())));
    QVERIFY2(echo.getErrors().contains("requires a login"), qPrintable(echo.getErrors()));
}

DECLARE_TEST(test_EchoToWadoPacs)

#include "test_echotowadopacs.moc"
