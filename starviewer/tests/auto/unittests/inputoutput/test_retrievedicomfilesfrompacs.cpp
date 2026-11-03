/*@
    "name": "test_RetrieveDICOMFilesFromPACS",
    "requirements": ["archive.dimse"]
 */

#include "autotest.h"
#include "retrievedicomfilesfrompacs.h"

#include "inputoutputsettings.h"
#include "pacsdevice.h"
#include "settings.h"
#include "testingsettings.h"

#include <QDirIterator>
#include <QTemporaryDir>

using namespace udg;
using namespace testing;

// These tests talk to the test PACS of quality/testlab, so they only run when these variables are set. See quality/testlab/README.md.
//   STARVIEWER_TESTLAB_PORT: DIMSE port.
//   STARVIEWER_TESTLAB_ADDRESS: address, by default localhost.
//   STARVIEWER_TESTLAB_STUDY: Study Instance UID to retrieve.
//   STARVIEWER_TESTLAB_FILES: number of files that the study has.
//   STARVIEWER_TESTLAB_CA, _CLIENT_CERTIFICATE, _CLIENT_KEY: certificate files, to use DICOM TLS.
//   STARVIEWER_TESTLAB_CACHE: directory to leave the files in, to look at them. By default, they go to a temporary one.
//   STARVIEWER_TESTLAB_EXPECT_FAILURE: if set, the connection must be refused, for example because of a certificate.
class test_RetrieveDICOMFilesFromPACS : public QObject {
Q_OBJECT

private slots:
    void retrieve_ShouldReceiveTheStudyWithCGet();

private:
    PacsDevice createTestLabPacs() const;
};

PacsDevice test_RetrieveDICOMFilesFromPACS::createTestLabPacs() const
{
    PacsDevice pacs;
    pacs.setType(PacsDevice::Type::Dimse);
    pacs.setAETitle("ORTHANC");
    pacs.setAddress(qEnvironmentVariable("STARVIEWER_TESTLAB_ADDRESS", "localhost"));
    pacs.setQueryRetrieveServiceEnabled(true);
    pacs.setQueryRetrieveServicePort(qEnvironmentVariableIntValue("STARVIEWER_TESTLAB_PORT"));
    pacs.setCGetEnabled(true);

    if (qEnvironmentVariableIsSet("STARVIEWER_TESTLAB_CA"))
    {
        pacs.setDicomTlsEnabled(true);
        pacs.setCaCertificateFile(qEnvironmentVariable("STARVIEWER_TESTLAB_CA"));
        pacs.setClientCertificate(qEnvironmentVariable("STARVIEWER_TESTLAB_CLIENT_CERTIFICATE"), qEnvironmentVariable("STARVIEWER_TESTLAB_CLIENT_KEY"));
    }

    return pacs;
}

void test_RetrieveDICOMFilesFromPACS::retrieve_ShouldReceiveTheStudyWithCGet()
{
    if (!qEnvironmentVariableIsSet("STARVIEWER_TESTLAB_PORT"))
    {
        QSKIP("The test PACS is not configured.");
    }

    QTemporaryDir temporaryCache;
    QVERIFY(temporaryCache.isValid());
    QString cachePath = qEnvironmentVariable("STARVIEWER_TESTLAB_CACHE", temporaryCache.path());

    TestingSettings testingSettings;
    Settings::setStaticTestingSettings(&testingSettings);
    Settings().setValue(InputOutputSettings::CachePath, cachePath + "/");
    Settings().setValue(InputOutputSettings::LocalAETitle, "STARVIEWER");
    Settings().setValue(InputOutputSettings::PACSConnectionTimeout, 10);

    RetrieveDICOMFilesFromPACS retrieve(createTestLabPacs());
    PACSRequestStatus::RetrieveRequestStatus status = retrieve.retrieve(qEnvironmentVariable("STARVIEWER_TESTLAB_STUDY"), QString(), QString());

    Settings::setStaticTestingSettings(nullptr);

    if (qEnvironmentVariableIsSet("STARVIEWER_TESTLAB_EXPECT_FAILURE"))
    {
        QCOMPARE(status, PACSRequestStatus::RetrieveCanNotConnectToPACS);
        return;
    }

    QCOMPARE(status, PACSRequestStatus::RetrieveOk);
    QCOMPARE(retrieve.getNumberOfDICOMFilesRetrieved(), qEnvironmentVariableIntValue("STARVIEWER_TESTLAB_FILES"));

    int filesOnDisk = 0;
    QDirIterator iterator(cachePath, QDir::Files, QDirIterator::Subdirectories);

    while (iterator.hasNext())
    {
        iterator.next();
        filesOnDisk++;
    }

    QCOMPARE(filesOnDisk, qEnvironmentVariableIntValue("STARVIEWER_TESTLAB_FILES"));
}

DECLARE_TEST(test_RetrieveDICOMFilesFromPACS)

#include "test_retrievedicomfilesfrompacs.moc"
