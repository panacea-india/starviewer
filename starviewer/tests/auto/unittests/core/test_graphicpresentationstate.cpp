/*@
    "name": "test_GraphicPresentationState",
    "requirements": []
 */

#include "autotest.h"
#include "graphicpresentationstate.h"

#include "dicomtagreader.h"
#include "study.h"

#include <dcmtk/dcmdata/dctk.h>

using namespace udg;

Q_DECLARE_METATYPE(DcmDataset*)

namespace {

const char* const GrayscaleClass = "1.2.840.10008.5.1.4.1.1.11.1";
const char* const ColorClass = "1.2.840.10008.5.1.4.1.1.11.2";

// Creates the dataset of a presentation state that refers to the given images. The caller (the DICOMTagReader) owns it.
DcmDataset* createDataset(const char *sopClass, const QStringList &images, const char *label = "Annotations")
{
    DcmDataset *dataset = new DcmDataset;
    dataset->putAndInsertString(DCM_SOPClassUID, sopClass);
    dataset->putAndInsertString(DCM_SOPInstanceUID, "1.2.3.77");
    dataset->putAndInsertString(DCM_ContentLabel, label);
    dataset->putAndInsertString(DCM_ContentDescription, "A description");

    for (const QString &image : images)
    {
        DcmItem *series = nullptr;
        dataset->findOrCreateSequenceItem(DCM_ReferencedSeriesSequence, series, -2);
        DcmItem *referenced = nullptr;
        series->findOrCreateSequenceItem(DCM_ReferencedImageSequence, referenced, -2);
        referenced->putAndInsertString(DCM_ReferencedSOPClassUID, "1.2.840.10008.5.1.4.1.1.2");
        referenced->putAndInsertString(DCM_ReferencedSOPInstanceUID, qPrintable(image));
    }

    return dataset;
}

// Adds an annotation to the dataset. images are the ones it applies to. graphics and texts are added to it by the callers.
DcmItem* addAnnotation(DcmDataset *dataset, const QStringList &images)
{
    DcmItem *annotation = nullptr;
    dataset->findOrCreateSequenceItem(DCM_GraphicAnnotationSequence, annotation, -2);
    annotation->putAndInsertString(DCM_GraphicLayer, "LAYER1");

    for (const QString &image : images)
    {
        DcmItem *referenced = nullptr;
        annotation->findOrCreateSequenceItem(DCM_ReferencedImageSequence, referenced, -2);
        referenced->putAndInsertString(DCM_ReferencedSOPClassUID, "1.2.840.10008.5.1.4.1.1.2");
        referenced->putAndInsertString(DCM_ReferencedSOPInstanceUID, qPrintable(image));
    }

    return annotation;
}

void addGraphic(DcmItem *annotation, const char *type, const char *units, const char *data, const char *filled = "N")
{
    DcmItem *graphic = nullptr;
    annotation->findOrCreateSequenceItem(DCM_GraphicObjectSequence, graphic, -2);
    graphic->putAndInsertString(DCM_GraphicAnnotationUnits, units);
    graphic->putAndInsertString(DCM_GraphicDimensions, "2");
    graphic->putAndInsertString(DCM_GraphicType, type);
    graphic->putAndInsertString(DCM_GraphicFilled, filled);
    graphic->putAndInsertString(DCM_GraphicData, data);
}

GraphicPresentationState read(DcmDataset *dataset, QString *error = nullptr)
{
    DICOMTagReader reader;
    reader.setDcmDataset("", dataset);
    return GraphicPresentationState::fromDICOM(reader, error);
}

}

class test_GraphicPresentationState : public QObject {

    Q_OBJECT

private slots:
    void fromDICOM_ShouldReadTheLabelAndTheReferencedImages();
    void fromDICOM_ShouldReadTheGraphicObjects();
    void fromDICOM_ShouldReadTheTextObjects();
    void fromDICOM_ShouldAcceptColorPresentationStates();
    void fromDICOM_ShouldReturnNullAndSayWhy_data();
    void fromDICOM_ShouldReturnNullAndSayWhy();
    void fromDICOM_ShouldSkipTheGraphicObjectsThatAreIncompleteAndSayThatSomeAreNotApplied();

    void getNotApplied_ShouldNameWhatIsAskedForAndNotApplied_data();
    void getNotApplied_ShouldNameWhatIsAskedForAndNotApplied();

    void getAnnotationsFor_ShouldReturnTheAnnotationsOfTheImageAndFrame();
    void getAnnotationsFor_ShouldApplyAnAnnotationWithoutImagesToAllTheReferencedImages();

    void getUnresolvedReferences_ShouldReturnTheReferencesThatAreNotAvailable();

    void addPresentationState_ShouldNotAddANullOrARepeatedState();
    void getPresentationStatesReferencing_ShouldReturnTheStatesThatReferTheInstance();
};

void test_GraphicPresentationState::fromDICOM_ShouldReadTheLabelAndTheReferencedImages()
{
    GraphicPresentationState state = read(createDataset(GrayscaleClass, {"1.1", "1.2"}));

    QVERIFY(!state.isNull());
    QCOMPARE(state.getSopInstanceUid(), QString("1.2.3.77"));
    QCOMPARE(state.getLabel(), QString("Annotations"));
    QCOMPARE(state.getDescription(), QString("A description"));
    QCOMPARE(state.getReferencedImages().size(), 2);
    QVERIFY(state.references("1.1"));
    QVERIFY(state.references("1.2"));
    QVERIFY(!state.references("1.3"));
    QVERIFY(state.getNotApplied().isEmpty());
}

void test_GraphicPresentationState::fromDICOM_ShouldReadTheGraphicObjects()
{
    DcmDataset *dataset = createDataset(GrayscaleClass, {"1.1"});
    DcmItem *annotation = addAnnotation(dataset, {"1.1"});
    addGraphic(annotation, "POLYLINE", "PIXEL", "10\\20\\30\\40\\50\\60", "Y");
    addGraphic(annotation, "CIRCLE", "DISPLAY", "0.5\\0.5\\0.75\\0.5");
    addGraphic(annotation, "POINT", "PIXEL", "7\\8");
    addGraphic(annotation, "ELLIPSE", "PIXEL", "0\\5\\10\\5\\5\\0\\5\\10");

    GraphicPresentationState state = read(dataset);
    QList<GraphicPresentationState::Annotation> annotations = state.getAnnotationsFor("1.1");

    QCOMPARE(annotations.size(), 1);
    QCOMPARE(annotations.first().layer, QString("LAYER1"));
    QCOMPARE(annotations.first().graphics.size(), 4);

    const GraphicPresentationState::GraphicObject &polyline = annotations.first().graphics.at(0);
    QVERIFY(polyline.type == GraphicPresentationState::GraphicObject::Type::Polyline);
    QVERIFY(polyline.units == GraphicPresentationState::Units::Pixel);
    QVERIFY(polyline.filled);
    QCOMPARE(polyline.points, QList<QPointF>({QPointF(10, 20), QPointF(30, 40), QPointF(50, 60)}));

    const GraphicPresentationState::GraphicObject &circle = annotations.first().graphics.at(1);
    QVERIFY(circle.type == GraphicPresentationState::GraphicObject::Type::Circle);
    QVERIFY(circle.units == GraphicPresentationState::Units::Display);
    QVERIFY(!circle.filled);
    QCOMPARE(circle.points, QList<QPointF>({QPointF(0.5, 0.5), QPointF(0.75, 0.5)}));

    QVERIFY(annotations.first().graphics.at(2).type == GraphicPresentationState::GraphicObject::Type::Point);
    QCOMPARE(annotations.first().graphics.at(3).points.size(), 4);
}

void test_GraphicPresentationState::fromDICOM_ShouldReadTheTextObjects()
{
    DcmDataset *dataset = createDataset(GrayscaleClass, {"1.1"});
    DcmItem *annotation = addAnnotation(dataset, {"1.1"});
    DcmItem *anchored = nullptr;
    annotation->findOrCreateSequenceItem(DCM_TextObjectSequence, anchored, -2);
    anchored->putAndInsertString(DCM_UnformattedTextValue, "Look here");
    anchored->putAndInsertString(DCM_AnchorPoint, "12\\34");
    anchored->putAndInsertString(DCM_AnchorPointAnnotationUnits, "PIXEL");
    DcmItem *boxed = nullptr;
    annotation->findOrCreateSequenceItem(DCM_TextObjectSequence, boxed, -2);
    boxed->putAndInsertString(DCM_UnformattedTextValue, "In a box");
    boxed->putAndInsertString(DCM_BoundingBoxTopLeftHandCorner, "0.1\\0.2");
    boxed->putAndInsertString(DCM_BoundingBoxBottomRightHandCorner, "0.4\\0.3");
    boxed->putAndInsertString(DCM_BoundingBoxAnnotationUnits, "DISPLAY");

    QList<GraphicPresentationState::TextObject> texts = read(dataset).getAnnotationsFor("1.1").first().texts;

    QCOMPARE(texts.size(), 2);
    QCOMPARE(texts.at(0).text, QString("Look here"));
    QVERIFY(texts.at(0).hasAnchor);
    QCOMPARE(texts.at(0).anchor, QPointF(12, 34));
    QVERIFY(!texts.at(0).hasBox);
    QVERIFY(!texts.at(1).hasAnchor);
    QVERIFY(texts.at(1).hasBox);
    QVERIFY(texts.at(1).boxUnits == GraphicPresentationState::Units::Display);
    // The coordinates are stored as floats
    QVERIFY(qAbs(texts.at(1).boxTopLeft.x() - 0.1) < 1e-6 && qAbs(texts.at(1).boxTopLeft.y() - 0.2) < 1e-6);
    QVERIFY(qAbs(texts.at(1).boxBottomRight.x() - 0.4) < 1e-6 && qAbs(texts.at(1).boxBottomRight.y() - 0.3) < 1e-6);
}

void test_GraphicPresentationState::fromDICOM_ShouldAcceptColorPresentationStates()
{
    QVERIFY(!read(createDataset(ColorClass, {"1.1"})).isNull());
}

void test_GraphicPresentationState::fromDICOM_ShouldReturnNullAndSayWhy_data()
{
    QTest::addColumn<DcmDataset*>("dataset");
    QTest::addColumn<QString>("expectedError");

    QTest::newRow("another kind of document") << createDataset("1.2.840.10008.5.1.4.1.1.2", {"1.1"}) << QString("It is not a softcopy presentation state.");
    QTest::newRow("no images") << createDataset(GrayscaleClass, {}) << QString("The presentation state does not refer to any image.");
}

void test_GraphicPresentationState::fromDICOM_ShouldReturnNullAndSayWhy()
{
    QFETCH(DcmDataset*, dataset);
    QFETCH(QString, expectedError);

    QString error;
    GraphicPresentationState state = read(dataset, &error);

    QVERIFY(state.isNull());
    QCOMPARE(error, expectedError);
}

void test_GraphicPresentationState::fromDICOM_ShouldSkipTheGraphicObjectsThatAreIncompleteAndSayThatSomeAreNotApplied()
{
    DcmDataset *dataset = createDataset(GrayscaleClass, {"1.1"});
    DcmItem *annotation = addAnnotation(dataset, {"1.1"});
    addGraphic(annotation, "POLYLINE", "PIXEL", "10\\20");            // one point only
    addGraphic(annotation, "SPLINE", "PIXEL", "10\\20\\30\\40");      // not a type of the standard
    addGraphic(annotation, "POLYLINE", "PIXEL", "10\\20\\30\\40");

    GraphicPresentationState state = read(dataset);

    QCOMPARE(state.getAnnotationsFor("1.1").first().graphics.size(), 1);
    QCOMPARE(state.getNotApplied(), QStringList({"Some graphic objects"}));
}

void test_GraphicPresentationState::getNotApplied_ShouldNameWhatIsAskedForAndNotApplied_data()
{
    QTest::addColumn<QString>("tag");
    QTest::addColumn<QString>("value");
    QTest::addColumn<QStringList>("expected");

    QTest::newRow("display shutter") << "shutter" << "RECTANGULAR" << QStringList({"Display shutter"});
    QTest::newRow("a rotation") << "rotation" << "90" << QStringList({"Rotation or flip"});
    QTest::newRow("no rotation") << "rotation" << "0" << QStringList();
    QTest::newRow("a flip") << "flip" << "Y" << QStringList({"Rotation or flip"});
    QTest::newRow("no flip") << "flip" << "N" << QStringList();
    QTest::newRow("an inverse presentation LUT") << "shape" << "INVERSE" << QStringList({"Presentation LUT"});
    QTest::newRow("an identity presentation LUT") << "shape" << "IDENTITY" << QStringList();
    QTest::newRow("a window and level") << "voi" << "" << QStringList({"Window and level"});
}

void test_GraphicPresentationState::getNotApplied_ShouldNameWhatIsAskedForAndNotApplied()
{
    QFETCH(QString, tag);
    QFETCH(QString, value);
    QFETCH(QStringList, expected);

    DcmDataset *dataset = createDataset(GrayscaleClass, {"1.1"});

    if (tag == "shutter")
    {
        dataset->putAndInsertString(DCM_ShutterShape, qPrintable(value));
    }
    else if (tag == "rotation")
    {
        dataset->putAndInsertString(DCM_ImageRotation, qPrintable(value));
    }
    else if (tag == "flip")
    {
        dataset->putAndInsertString(DCM_ImageHorizontalFlip, qPrintable(value));
    }
    else if (tag == "shape")
    {
        dataset->putAndInsertString(DCM_PresentationLUTShape, qPrintable(value));
    }
    else if (tag == "voi")
    {
        DcmItem *voi = nullptr;
        dataset->findOrCreateSequenceItem(DCM_SoftcopyVOILUTSequence, voi, -2);
        voi->putAndInsertString(DCM_WindowCenter, "40");
        voi->putAndInsertString(DCM_WindowWidth, "400");
    }

    QCOMPARE(read(dataset).getNotApplied(), expected);
}

void test_GraphicPresentationState::getAnnotationsFor_ShouldReturnTheAnnotationsOfTheImageAndFrame()
{
    DcmDataset *dataset = createDataset(GrayscaleClass, {"1.1", "1.2"});
    DcmItem *first = addAnnotation(dataset, {"1.1"});
    addGraphic(first, "POINT", "PIXEL", "1\\1");
    DcmItem *second = addAnnotation(dataset, {"1.2"});
    addGraphic(second, "POINT", "PIXEL", "2\\2");
    // The third one applies to the frames 2 and 3 of the instance 1.2
    DcmItem *third = addAnnotation(dataset, {});
    addGraphic(third, "POINT", "PIXEL", "3\\3");
    DcmItem *referenced = nullptr;
    third->findOrCreateSequenceItem(DCM_ReferencedImageSequence, referenced, -2);
    referenced->putAndInsertString(DCM_ReferencedSOPClassUID, "1.2.840.10008.5.1.4.1.1.2");
    referenced->putAndInsertString(DCM_ReferencedSOPInstanceUID, "1.2");
    referenced->putAndInsertString(DCM_ReferencedFrameNumber, "2\\3");

    GraphicPresentationState state = read(dataset);

    QCOMPARE(state.getAnnotationsFor("1.1").size(), 1);
    QCOMPARE(state.getAnnotationsFor("1.1").first().graphics.first().points.first(), QPointF(1, 1));
    QCOMPARE(state.getAnnotationsFor("1.2", 1).size(), 1);
    QCOMPARE(state.getAnnotationsFor("1.2", 2).size(), 2);
    QCOMPARE(state.getAnnotationsFor("1.2", 3).size(), 2);
    QCOMPARE(state.getAnnotationsFor("1.2", 4).size(), 1);
    QVERIFY(state.getAnnotationsFor("1.9").isEmpty());
}

void test_GraphicPresentationState::getAnnotationsFor_ShouldApplyAnAnnotationWithoutImagesToAllTheReferencedImages()
{
    DcmDataset *dataset = createDataset(GrayscaleClass, {"1.1", "1.2"});
    DcmItem *annotation = addAnnotation(dataset, {});
    addGraphic(annotation, "POINT", "PIXEL", "1\\1");

    GraphicPresentationState state = read(dataset);

    QCOMPARE(state.getAnnotationsFor("1.1").size(), 1);
    QCOMPARE(state.getAnnotationsFor("1.2").size(), 1);
    QVERIFY(state.getAnnotationsFor("1.3").isEmpty());
}

void test_GraphicPresentationState::getUnresolvedReferences_ShouldReturnTheReferencesThatAreNotAvailable()
{
    GraphicPresentationState state = read(createDataset(GrayscaleClass, {"1.1", "1.2", "1.3"}));

    QList<GraphicPresentationState::ReferencedImage> unresolved = state.getUnresolvedReferences({"1.1", "1.3"});

    QCOMPARE(unresolved.size(), 1);
    QCOMPARE(unresolved.first().sopInstanceUid, QString("1.2"));
}

void test_GraphicPresentationState::addPresentationState_ShouldNotAddANullOrARepeatedState()
{
    Study study;
    GraphicPresentationState state = read(createDataset(GrayscaleClass, {"1.1"}));

    QVERIFY(!study.addPresentationState(GraphicPresentationState()));
    QVERIFY(study.addPresentationState(state));
    QVERIFY(!study.addPresentationState(state));
    QCOMPARE(study.getPresentationStates().size(), 1);
}

void test_GraphicPresentationState::getPresentationStatesReferencing_ShouldReturnTheStatesThatReferTheInstance()
{
    Study study;
    DcmDataset *first = createDataset(GrayscaleClass, {"1.1", "1.2"});
    first->putAndInsertString(DCM_SOPInstanceUID, "5.1");
    DcmDataset *second = createDataset(GrayscaleClass, {"1.2"});
    second->putAndInsertString(DCM_SOPInstanceUID, "5.2");
    study.addPresentationState(read(first));
    study.addPresentationState(read(second));

    QCOMPARE(study.getPresentationStatesReferencing("1.1").size(), 1);
    QCOMPARE(study.getPresentationStatesReferencing("1.2").size(), 2);
    QVERIFY(study.getPresentationStatesReferencing("1.9").isEmpty());
}

DECLARE_TEST(test_GraphicPresentationState)

#include "test_graphicpresentationstate.moc"
