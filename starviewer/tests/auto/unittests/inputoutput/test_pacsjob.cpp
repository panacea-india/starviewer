/*@
    "name": "test_PACSJob",
    "requirements": ["archive.dimse"]
 */

#include "autotest.h"
#include "pacsjob.h"

using namespace udg;

namespace {

// Counts how many times the job was told to stop
class TestPACSJob : public PACSJob {
public:
    TestPACSJob() : PACSJob(PacsDevice()) {}

    PACSJob::PACSJobType getPACSJobType() override
    {
        return PACSJob::RetrieveDICOMFilesFromPACSJobType;
    }

    void run(ThreadWeaver::JobPointer, ThreadWeaver::Thread *) override
    {
    }

    int cancelRequests = 0;

private:
    void requestCancelJob() override
    {
        cancelRequests++;
    }
};

}

class test_PACSJob : public QObject {
Q_OBJECT

private slots:
    void requestAbort_ShouldStopTheJob_BeforeTheUninterruptiblePhase();
    void beginUninterruptiblePhase_ShouldFail_WhenAnAbortWasRequested();
    void requestAbort_ShouldNotStopTheJob_OnceTheUninterruptiblePhaseBegan();
};

void test_PACSJob::requestAbort_ShouldStopTheJob_BeforeTheUninterruptiblePhase()
{
    TestPACSJob job;

    job.requestAbort();

    QVERIFY(job.isAbortRequested());
    QCOMPARE(job.cancelRequests, 1);
}

void test_PACSJob::beginUninterruptiblePhase_ShouldFail_WhenAnAbortWasRequested()
{
    TestPACSJob job;
    job.requestAbort();

    QVERIFY(!job.beginUninterruptiblePhase());
}

void test_PACSJob::requestAbort_ShouldNotStopTheJob_OnceTheUninterruptiblePhaseBegan()
{
    TestPACSJob job;

    QVERIFY(job.beginUninterruptiblePhase());
    job.requestAbort();

    QVERIFY(!job.isAbortRequested());
    QCOMPARE(job.cancelRequests, 0);
}

DECLARE_TEST(test_PACSJob)

#include "test_pacsjob.moc"
