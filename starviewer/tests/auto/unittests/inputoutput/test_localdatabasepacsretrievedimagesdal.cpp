/*@
    "name": "test_LocalDatabasePACSRetrievedImagesDAL",
    "requirements": ["archive.local_storage", "archive.wado"]
 */

#include "autotest.h"
#include "localdatabasepacsretrievedimagesdal.h"

#include "databaseconnection.h"
#include "databasetesthelper.h"
#include "pacsdevice.h"

#include <QSqlQuery>
#include <QUrl>

using namespace udg;
using namespace testing;

namespace {

const QString UriWithoutLogin("http://example.com:8042/dicom-web/");
const QString UriWithLogin("http://user:secret@example.com:8042/dicom-web/");

// Inserts a row directly, like an earlier version that kept the login in the URI, and returns its id.
qlonglong insertRow(DatabaseConnection &databaseConnection, const QString &type, const QString &baseUri)
{
    QSqlQuery query(databaseConnection.getConnection());
    query.prepare("INSERT INTO PACSRetrievedImages (Type, BaseUri) VALUES (:type, :baseUri)");
    query.bindValue(":type", type);
    query.bindValue(":baseUri", baseUri);
    query.exec();
    return query.lastInsertId().toLongLong();
}

void insertImageAndDocument(DatabaseConnection &databaseConnection, qlonglong pacsId)
{
    QSqlQuery image(databaseConnection.getConnection());
    image.prepare("INSERT INTO Image (SOPInstanceUID, FrameNumber, StudyInstanceUID, SeriesInstanceUID, RetrievedPACSID) VALUES ('1', 0, '2', '3', :id)");
    image.bindValue(":id", pacsId);
    image.exec();

    QSqlQuery document(databaseConnection.getConnection());
    document.prepare("INSERT INTO EncapsulatedDocument (SOPInstanceUID, RetrievedPacsID) VALUES ('4', :id)");
    document.bindValue(":id", pacsId);
    document.exec();
}

int countRows(DatabaseConnection &databaseConnection, const QString &select)
{
    QSqlQuery query(databaseConnection.getConnection());
    query.exec(select);
    query.next();
    return query.value(0).toInt();
}

}

class test_LocalDatabasePACSRetrievedImagesDAL : public QObject {
    Q_OBJECT

private slots:
    void removeLoginsFromBaseUris_ShouldRemoveTheLogin();
    void removeLoginsFromBaseUris_ShouldKeepTheLinkFromImages();
    void removeLoginsFromBaseUris_ShouldMergeWithTheRowWithoutLogin();
    void removeLoginsFromBaseUris_ShouldNotChangeOtherRows();

    void insert_ShouldNotStoreALogin();
};

void test_LocalDatabasePACSRetrievedImagesDAL::removeLoginsFromBaseUris_ShouldRemoveTheLogin()
{
    QScopedPointer<DatabaseConnection> databaseConnection(DatabaseTestHelper::getCreatedDatabase());
    qlonglong id = insertRow(*databaseConnection, "WADO", UriWithLogin);

    LocalDatabasePACSRetrievedImagesDAL dal(*databaseConnection);

    QCOMPARE(dal.removeLoginsFromBaseUris(), 1);
    QCOMPARE(countRows(*databaseConnection, "SELECT COUNT(*) FROM PACSRetrievedImages WHERE BaseUri LIKE '%secret%'"), 0);
    QCOMPARE(dal.query(id).getBaseUri(), QUrl(UriWithoutLogin));
    QVERIFY(!dal.query(id).isLoginSpecified());

    // A device from settings, which has the URI without login, must now match the row
    PacsDevice pacs;
    pacs.setType(PacsDevice::Type::Wado);
    pacs.setBaseUri(QUrl(UriWithoutLogin));
    QCOMPARE(dal.queryId(pacs).toLongLong(), id);
}

void test_LocalDatabasePACSRetrievedImagesDAL::removeLoginsFromBaseUris_ShouldKeepTheLinkFromImages()
{
    QScopedPointer<DatabaseConnection> databaseConnection(DatabaseTestHelper::getCreatedDatabase());
    qlonglong id = insertRow(*databaseConnection, "WADO", UriWithLogin);
    insertImageAndDocument(*databaseConnection, id);

    LocalDatabasePACSRetrievedImagesDAL(*databaseConnection).removeLoginsFromBaseUris();

    QCOMPARE(countRows(*databaseConnection, QString("SELECT COUNT(*) FROM Image WHERE RetrievedPACSID = %1").arg(id)), 1);
    QCOMPARE(countRows(*databaseConnection, QString("SELECT COUNT(*) FROM EncapsulatedDocument WHERE RetrievedPacsID = %1").arg(id)), 1);
}

void test_LocalDatabasePACSRetrievedImagesDAL::removeLoginsFromBaseUris_ShouldMergeWithTheRowWithoutLogin()
{
    QScopedPointer<DatabaseConnection> databaseConnection(DatabaseTestHelper::getCreatedDatabase());
    qlonglong keptId = insertRow(*databaseConnection, "WADO", UriWithoutLogin);
    qlonglong oldId = insertRow(*databaseConnection, "WADO", UriWithLogin);
    insertImageAndDocument(*databaseConnection, oldId);

    QCOMPARE(LocalDatabasePACSRetrievedImagesDAL(*databaseConnection).removeLoginsFromBaseUris(), 1);

    QCOMPARE(countRows(*databaseConnection, "SELECT COUNT(*) FROM PACSRetrievedImages"), 1);
    QCOMPARE(countRows(*databaseConnection, QString("SELECT COUNT(*) FROM PACSRetrievedImages WHERE ID = %1").arg(keptId)), 1);
    QCOMPARE(countRows(*databaseConnection, QString("SELECT COUNT(*) FROM Image WHERE RetrievedPACSID = %1").arg(keptId)), 1);
    QCOMPARE(countRows(*databaseConnection, QString("SELECT COUNT(*) FROM EncapsulatedDocument WHERE RetrievedPacsID = %1").arg(keptId)), 1);
}

void test_LocalDatabasePACSRetrievedImagesDAL::removeLoginsFromBaseUris_ShouldNotChangeOtherRows()
{
    QScopedPointer<DatabaseConnection> databaseConnection(DatabaseTestHelper::getCreatedDatabase());
    insertRow(*databaseConnection, "WADO", UriWithoutLogin);
    insertRow(*databaseConnection, "WADO", "http://example.com/path@withat/");

    QCOMPARE(LocalDatabasePACSRetrievedImagesDAL(*databaseConnection).removeLoginsFromBaseUris(), 0);
    QCOMPARE(countRows(*databaseConnection, "SELECT COUNT(*) FROM PACSRetrievedImages WHERE BaseUri = 'http://example.com/path@withat/'"), 1);
}

void test_LocalDatabasePACSRetrievedImagesDAL::insert_ShouldNotStoreALogin()
{
    QScopedPointer<DatabaseConnection> databaseConnection(DatabaseTestHelper::getCreatedDatabase());
    PacsDevice pacs;
    pacs.setType(PacsDevice::Type::Wado);
    pacs.setBaseUri(QUrl(UriWithLogin));

    LocalDatabasePACSRetrievedImagesDAL(*databaseConnection).insert(pacs);

    QCOMPARE(countRows(*databaseConnection, "SELECT COUNT(*) FROM PACSRetrievedImages WHERE BaseUri LIKE '%secret%'"), 0);
    QCOMPARE(countRows(*databaseConnection, "SELECT COUNT(*) FROM PACSRetrievedImages"), 1);
}

DECLARE_TEST(test_LocalDatabasePACSRetrievedImagesDAL)

#include "test_localdatabasepacsretrievedimagesdal.moc"
