/*@
    "name": "test_HangingProtocolXMLWriter",
    "requirements": []
 */

#include "autotest.h"

#include <QFile>

#include <memory>

#include <QBuffer>
#include <QDirIterator>
#include <QTemporaryDir>

#include "hangingprotocol.h"
#include "hangingprotocollayout.h"
#include "hangingprotocolmask.h"
#include "patientorientation.h"
#include "hangingprotocoldisplayset.h"
#include "hangingprotocolimageset.h"
#include "hangingprotocolxmlreader.h"
#include "hangingprotocolxmlwriter.h"
#include "starviewerapplication.h"

using namespace udg;

class test_HangingProtocolXMLWriter : public QObject {

    Q_OBJECT

private slots:
    void toXml_ShouldGiveAProtocolThatReadsBackTheSame_Shipped();
    void toXml_ShouldGiveAProtocolThatReadsBackTheSame_AllFields();
    void toXml_ShouldKeepTheExpressionOfEachImageSet();
    void toXml_ShouldFailIfTheSameRestrictionNumberMeansTwoThings();
    void toXml_ShouldNotWriteTheDefaults();
    void writeFile_ShouldWriteAFileThatCanBeRead();
    void writeFile_ShouldFailForAPathThatCannotBeWritten();

private:
    static std::unique_ptr<HangingProtocol> parse(const QByteArray &xml);
    /// The first thing that is different between two protocols, or an empty string if they are the same (it says what to look at when a comparison fails)
    static QString difference(const HangingProtocol &a, const HangingProtocol &b);
};

QString test_HangingProtocolXMLWriter::difference(const HangingProtocol &a, const HangingProtocol &b)
{
    if (a.getName() != b.getName()) return "name";
    if (a.isStrict() != b.isStrict()) return "strictness";
    if (a.getAllDifferent() != b.getAllDifferent()) return "all different";
    if (a.getIconType() != b.getIconType()) return "icon";
    if (a.getNumberOfPriors() != b.getNumberOfPriors()) return "priors";
    if (a.getPriority() != b.getPriority()) return "priority";
    if (a.getHangingProtocolLayout()->getNumberOfScreens() != b.getHangingProtocolLayout()->getNumberOfScreens()) return "screens";
    if (a.getHangingProtocolLayout()->getDisplayEnvironmentSpatialPositionList() != b.getHangingProtocolLayout()->getDisplayEnvironmentSpatialPositionList()) return "spatial positions";
    if (a.getHangingProtocolLayout()->getHorizontalPixelsList() != b.getHangingProtocolLayout()->getHorizontalPixelsList()) return "horizontal pixels";
    if (a.getHangingProtocolLayout()->getVerticalPixelsList() != b.getHangingProtocolLayout()->getVerticalPixelsList()) return "vertical pixels";
    if (a.getHangingProtocolMask()->getProtocolList() != b.getHangingProtocolMask()->getProtocolList()) return "modalities";
    if (!(a.getInstitutionsRegularExpression() == b.getInstitutionsRegularExpression())) return "institutions";
    if (a.getImageSets().size() != b.getImageSets().size()) return "number of image sets";
    if (a.getDisplaySets().size() != b.getDisplaySets().size()) return "number of display sets";

    for (int i = 0; i < a.getImageSets().size(); i++)
    {
        HangingProtocolImageSet *x = a.getImageSets()[i];
        HangingProtocolImageSet *y = b.getImageSets()[i];
        QString where = QString("image set %1: ").arg(x->getIdentifier());
        if (x->getIdentifier() != y->getIdentifier()) return where + "identifier";
        if (!(x->getRestrictionExpressions() == y->getRestrictionExpressions())) return where + "expressions";
        if (x->getType() != y->getType()) return where + "type";
        if (x->getImageToDisplay() != y->getImageToDisplay()) return where + "image to display";
        if (x->isDownloaded() != y->isDownloaded()) return where + "downloaded";
        if (x->getImageNumberInStudyModality() != y->getImageNumberInStudyModality()) return where + "image number in study modality";
    }

    for (int i = 0; i < a.getDisplaySets().size(); i++)
    {
        HangingProtocolDisplaySet *x = a.getDisplaySets()[i];
        HangingProtocolDisplaySet *y = b.getDisplaySets()[i];
        QString where = QString("display set %1: ").arg(x->getIdentifier());
        if (x->getIdentifier() != y->getIdentifier()) return where + "identifier";
        if (x->getDescription() != y->getDescription()) return where + "description";
        if (x->getPosition() != y->getPosition()) return where + "position";
        if (x->getReconstruction() != y->getReconstruction()) return where + "reconstruction";
        if (x->getPhase() != y->getPhase()) return where + "phase";
        if (x->getSlice() != y->getSlice()) return where + "slice";
        if (x->getIconType() != y->getIconType()) return where + "icon";
        if (x->getAlignment() != y->getAlignment()) return where + "alignment";
        if (x->getToolActivation() != y->getToolActivation()) return where + "tool activation";
        if (x->getWindowWidth() != y->getWindowWidth()) return where + "window width";
        if (x->getWindowCenter() != y->getWindowCenter()) return where + "window center";
        if (x->getPatientOrientation().getDICOMFormattedPatientOrientation() != y->getPatientOrientation().getDICOMFormattedPatientOrientation()) return where + "orientation";
        if ((x->getImageSet() ? x->getImageSet()->getIdentifier() : -1) != (y->getImageSet() ? y->getImageSet()->getIdentifier() : -1)) return where + "image set";
    }

    return QString();
}

std::unique_ptr<HangingProtocol> test_HangingProtocolXMLWriter::parse(const QByteArray &xml)
{
    QByteArray copy = xml;
    QBuffer buffer(&copy);
    buffer.open(QIODevice::ReadOnly);
    HangingProtocolXMLReader reader;
    std::unique_ptr<HangingProtocol> protocol(reader.read(&buffer));

    if (protocol)
    {
        protocol->setIdentifier(0);
    }

    return protocol;
}

static const char *AllFields = R"(<?xml version='1.0' encoding='UTF-8'?>
<hangingProtocol name="All" version="2">
    <hangingProtocolName>All the fields</hangingProtocolName>
    <numberScreens>2</numberScreens>
    <protocol>CT</protocol>
    <protocol>MR</protocol>
    <institutions>hospital.*</institutions>
    <priority>0.5</priority>
    <strictness>yes</strictness>
    <allDifferent>yes</allDifferent>
    <iconType>icon-a</iconType>
    <numberOfPriors>1</numberOfPriors>
    <restriction identifier="1"><selectorAttribute>ImageType</selectorAttribute><selectorValue>LOCALIZER</selectorValue></restriction>
    <restriction identifier="2"><selectorAttribute>MinimumNumberOfImages</selectorAttribute><selectorValue>9</selectorValue></restriction>
    <restriction identifier="3"><selectorAttribute>SeriesDescription</selectorAttribute><selectorValue>head</selectorValue></restriction>
    <imageSet identifier="1">
        <type>series</type>
        <restrictionExpression>(not 1 and 2) or 3</restrictionExpression>
        <abstractPriorValue>2</abstractPriorValue>
        <imageNumberInStudyModality>3</imageNumberInStudyModality>
    </imageSet>
    <imageSet identifier="2">
        <type>fusion</type>
        <restrictionExpression>2</restrictionExpression>
        <restrictionExpression>3</restrictionExpression>
    </imageSet>
    <imageSet identifier="3">
        <type>image</type>
    </imageSet>
    <displaySet identifier="1">
        <imageSetNumber>1</imageSetNumber>
        <position>0.0\1.0\0.5\0.0</position>
        <patientOrientation>A\F</patientOrientation>
        <reconstruction>SAGITTAL</reconstruction>
        <phase>2</phase>
        <imageNumber>4</imageNumber>
        <iconType>icon-b</iconType>
        <alignment>right</alignment>
        <toolActivation>synchronization</toolActivation>
        <windowLevel><width>400.5</width><center>40</center></windowLevel>
    </displaySet>
    <displaySet identifier="2">
        <imageSetNumber>2</imageSetNumber>
        <position>0.5\1.0\1.0\0.0</position>
    </displaySet>
</hangingProtocol>
)";

void test_HangingProtocolXMLWriter::toXml_ShouldGiveAProtocolThatReadsBackTheSame_Shipped()
{
#ifdef HANGING_PROTOCOLS_SOURCE_DIR
    QString directory = HANGING_PROTOCOLS_SOURCE_DIR;
#else
    QString directory = sourcePath() + "/hangingprotocols";
#endif
    QDirIterator it(directory, QStringList() << "*.xml", QDir::Files, QDirIterator::Subdirectories);
    int files = 0;

    while (it.hasNext())
    {
        QString path = it.next();
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        std::unique_ptr<HangingProtocol> original = parse(file.readAll());
        QVERIFY2(original, qPrintable(path));

        QString error;
        QString xml = HangingProtocolXMLWriter::toXml(*original, &error);
        QVERIFY2(!xml.isEmpty(), qPrintable(path + ": " + error));

        std::unique_ptr<HangingProtocol> again = parse(xml.toUtf8());
        QVERIFY2(again, qPrintable(path));
        QVERIFY2(difference(*original, *again).isEmpty() && original->compareTo(*again), qPrintable(path + ": " + difference(*original, *again)));
        files++;
    }

    // The protocols that come with the program are there
    QVERIFY2(files > 5, qPrintable(QString("%1 files found in %2").arg(files).arg(directory)));
}

void test_HangingProtocolXMLWriter::toXml_ShouldGiveAProtocolThatReadsBackTheSame_AllFields()
{
    std::unique_ptr<HangingProtocol> original = parse(AllFields);
    QVERIFY(original);

    QString xml = HangingProtocolXMLWriter::toXml(*original);
    std::unique_ptr<HangingProtocol> again = parse(xml.toUtf8());

    QVERIFY(again);
    QVERIFY2(difference(*original, *again).isEmpty(), qPrintable(difference(*original, *again)));
    QVERIFY(original->compareTo(*again));
    QCOMPARE(again->getName(), QString("All the fields"));
    QCOMPARE(again->getPriority(), 0.5);
    QVERIFY(again->isStrict());
    QVERIFY(again->getAllDifferent());
    QCOMPARE(again->getNumberOfPriors(), 1);
    QCOMPARE(again->getImageSets().size(), 3);
    QCOMPARE(again->getDisplaySets().size(), 2);
    QCOMPARE(again->getDisplaySet(1)->getPhase(), 2);
    QCOMPARE(again->getDisplaySet(1)->getSlice(), 4);
    QCOMPARE(again->getDisplaySet(1)->getWindowWidth(), 400.5);
    QCOMPARE(again->getDisplaySet(1)->getPatientOrientation().getDICOMFormattedPatientOrientation(), QString("A\\F"));
    QCOMPARE(again->getImageSet(2)->getType(), HangingProtocolImageSet::Type::Fusion);
    QCOMPARE(again->getImageSet(2)->getNumberOfRestrictionExpressions(), 2);
    QCOMPARE(again->getImageSet(1)->getAbstractPriorValue(), 2);
    QCOMPARE(again->getImageSet(1)->getImageNumberInStudyModality(), 3);
}

void test_HangingProtocolXMLWriter::toXml_ShouldKeepTheExpressionOfEachImageSet()
{
    std::unique_ptr<HangingProtocol> original = parse(AllFields);
    QVERIFY(original);
    std::unique_ptr<HangingProtocol> again = parse(HangingProtocolXMLWriter::toXml(*original).toUtf8());
    QVERIFY(again);

    QVERIFY(original->getImageSet(1)->getRestrictionExpressions() == again->getImageSet(1)->getRestrictionExpressions());
    QVERIFY(original->getImageSet(2)->getRestrictionExpressions() == again->getImageSet(2)->getRestrictionExpressions());
    // An image set without restrictions stays without them
    QVERIFY(original->getImageSet(3)->getRestrictionExpressions() == again->getImageSet(3)->getRestrictionExpressions());
}

void test_HangingProtocolXMLWriter::toXml_ShouldFailIfTheSameRestrictionNumberMeansTwoThings()
{
    // Built by hand: two image sets that use the number 1 for different restrictions (a file cannot say that)
    std::unique_ptr<HangingProtocol> protocol(new HangingProtocol());
    protocol->setName("Conflict");

    for (int i = 1; i <= 2; i++)
    {
        QMap<int, HangingProtocolImageSetRestriction> restrictions;
        restrictions[1] = HangingProtocolImageSetRestriction(1, "SeriesDescription", i == 1 ? "a" : "b", 0);
        HangingProtocolImageSet *imageSet = new HangingProtocolImageSet();
        imageSet->setIdentifier(i);
        imageSet->addRestrictionExpression(HangingProtocolImageSetRestrictionExpression("1", restrictions));
        protocol->addImageSet(imageSet);
    }

    QString error;
    QVERIFY(HangingProtocolXMLWriter::toXml(*protocol, &error).isEmpty());
    QVERIFY(!error.isEmpty());
}

void test_HangingProtocolXMLWriter::toXml_ShouldNotWriteTheDefaults()
{
    std::unique_ptr<HangingProtocol> protocol(new HangingProtocol());
    protocol->setName("Small");
    protocol->setNumberOfScreens(1);
    QString xml = HangingProtocolXMLWriter::toXml(*protocol);

    QVERIFY(xml.contains("<hangingProtocolName>Small</hangingProtocolName>"));
    QVERIFY(!xml.contains("strictness"));
    QVERIFY(!xml.contains("allDifferent"));
    QVERIFY(!xml.contains("numberOfPriors"));
    QVERIFY(!xml.contains("institutions"));
    QVERIFY(!xml.contains("iconType"));
}

void test_HangingProtocolXMLWriter::writeFile_ShouldWriteAFileThatCanBeRead()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    std::unique_ptr<HangingProtocol> original = parse(AllFields);
    QVERIFY(original);

    QString path = directory.filePath("written.xml");
    QVERIFY(HangingProtocolXMLWriter::writeFile(*original, path));

    HangingProtocolXMLReader reader;
    std::unique_ptr<HangingProtocol> read(reader.readFile(path));
    QVERIFY(read);
    read->setIdentifier(0);
    QVERIFY2(difference(*original, *read).isEmpty(), qPrintable(difference(*original, *read)));
    QVERIFY(original->compareTo(*read));
}

void test_HangingProtocolXMLWriter::writeFile_ShouldFailForAPathThatCannotBeWritten()
{
    QTemporaryDir directory;
    HangingProtocol protocol;
    protocol.setName("X");
    QString error;

    QVERIFY(!HangingProtocolXMLWriter::writeFile(protocol, directory.filePath("missing-folder/file.xml"), &error));
    QVERIFY(!error.isEmpty());
}

DECLARE_TEST(test_HangingProtocolXMLWriter)

#include "test_hangingprotocolxmlwriter.moc"
