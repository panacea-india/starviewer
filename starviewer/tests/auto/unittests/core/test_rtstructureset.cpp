/*@
    "name": "test_RtStructureSet",
    "requirements": []
 */

#include "autotest.h"
#include "rtstructureset.h"

#include "dicomtagreader.h"
#include "study.h"

#include <cmath>

#include <dcmtk/dcmdata/dctk.h>

using namespace udg;

namespace {

const char* const StructureSetClass = "1.2.840.10008.5.1.4.1.1.481.3";

// A structure set with two regions: "PTV" (red) with a square contour of 20 mm at z = 10 and one at z = 12.5, and "Cord" (green) with an open contour at z = 10.
// The caller (the DICOMTagReader) owns the dataset.
DcmDataset* createDataset(const char *sopClass = StructureSetClass)
{
    DcmDataset *dataset = new DcmDataset;
    dataset->putAndInsertString(DCM_SOPClassUID, sopClass);
    dataset->putAndInsertString(DCM_SOPInstanceUID, "1.2.3.88");
    dataset->putAndInsertString(DCM_StructureSetLabel, "Plan 1");

    DcmItem *frame = nullptr;
    dataset->findOrCreateSequenceItem(DCM_ReferencedFrameOfReferenceSequence, frame, -2);
    frame->putAndInsertString(DCM_FrameOfReferenceUID, "1.2.3.FOR");

    DcmItem *roi = nullptr;
    dataset->findOrCreateSequenceItem(DCM_StructureSetROISequence, roi, -2);
    roi->putAndInsertString(DCM_ROINumber, "1");
    roi->putAndInsertString(DCM_ROIName, "PTV");
    dataset->findOrCreateSequenceItem(DCM_StructureSetROISequence, roi, -2);
    roi->putAndInsertString(DCM_ROINumber, "2");
    roi->putAndInsertString(DCM_ROIName, "Cord");
    dataset->findOrCreateSequenceItem(DCM_StructureSetROISequence, roi, -2);
    roi->putAndInsertString(DCM_ROINumber, "3");
    roi->putAndInsertString(DCM_ROIName, "Empty");

    DcmItem *first = nullptr;
    dataset->findOrCreateSequenceItem(DCM_ROIContourSequence, first, -2);
    first->putAndInsertString(DCM_ReferencedROINumber, "1");
    first->putAndInsertString(DCM_ROIDisplayColor, "255\\0\\0");
    DcmItem *contour = nullptr;
    first->findOrCreateSequenceItem(DCM_ContourSequence, contour, -2);
    contour->putAndInsertString(DCM_ContourGeometricType, "CLOSED_PLANAR");
    contour->putAndInsertString(DCM_NumberOfContourPoints, "4");
    contour->putAndInsertString(DCM_ContourData, "10\\10\\10\\30\\10\\10\\30\\30\\10\\10\\30\\10");
    first->findOrCreateSequenceItem(DCM_ContourSequence, contour, -2);
    contour->putAndInsertString(DCM_ContourGeometricType, "CLOSED_PLANAR");
    contour->putAndInsertString(DCM_NumberOfContourPoints, "4");
    contour->putAndInsertString(DCM_ContourData, "10\\10\\12.5\\30\\10\\12.5\\30\\30\\12.5\\10\\30\\12.5");

    DcmItem *second = nullptr;
    dataset->findOrCreateSequenceItem(DCM_ROIContourSequence, second, -2);
    second->putAndInsertString(DCM_ReferencedROINumber, "2");
    second->putAndInsertString(DCM_ROIDisplayColor, "0\\255\\0");
    second->findOrCreateSequenceItem(DCM_ContourSequence, contour, -2);
    contour->putAndInsertString(DCM_ContourGeometricType, "OPEN_PLANAR");
    contour->putAndInsertString(DCM_NumberOfContourPoints, "3");
    contour->putAndInsertString(DCM_ContourData, "0\\0\\10\\5\\0\\10\\10\\5\\10");
    // A single point is not an outline
    second->findOrCreateSequenceItem(DCM_ContourSequence, contour, -2);
    contour->putAndInsertString(DCM_ContourGeometricType, "POINT");
    contour->putAndInsertString(DCM_NumberOfContourPoints, "1");
    contour->putAndInsertString(DCM_ContourData, "1\\2\\3");

    return dataset;
}

RtStructureSet read(DcmDataset *dataset, QString *error = nullptr)
{
    DICOMTagReader reader;
    reader.setDcmDataset("", dataset);
    return RtStructureSet::fromDICOM(reader, error);
}

RtStructureSet::PlaneGeometry axialGeometry()
{
    // The first image: its first pixel is at (-50, -50, 0) mm, with pixels of 2 x 2 mm
    RtStructureSet::PlaneGeometry geometry;
    geometry.origin = {{-50.0, -50.0, 0.0}};
    geometry.row = {{1.0, 0.0, 0.0}};
    geometry.column = {{0.0, 1.0, 0.0}};
    geometry.spacingX = 2.0;
    geometry.spacingY = 2.0;
    return geometry;
}

}

class test_RtStructureSet : public QObject {

    Q_OBJECT

private slots:
    void fromDICOM_ShouldReadTheRegionsAndTheirContours();
    void fromDICOM_ShouldFail_WhenItIsNotAStructureSetOrHasNoContours();
    void fromDICOM_ShouldKeepTheFrameOfReference();

    void toPixel_ShouldGiveThePixelOfThePointInThePlane();
    void depthOf_ShouldGiveTheDistanceAlongTheNormal();

    void contoursAtDepth_ShouldGiveTheContoursOfTheSliceOnly();
    void contoursAtDepth_ShouldClosePolygonsAndLeaveOpenOnesOpen();
    void contoursAtDepth_ShouldUseTheTolerance();
    void contoursAtDepth_ShouldWorkInATiltedPlane();

    void study_ShouldKeepEachStructureSetOnce();
};

void test_RtStructureSet::fromDICOM_ShouldReadTheRegionsAndTheirContours()
{
    RtStructureSet set = read(createDataset());

    QVERIFY(!set.isNull());
    QCOMPARE(set.getSopInstanceUid(), QString("1.2.3.88"));
    QCOMPARE(set.getLabel(), QString("Plan 1"));
    // "Empty" has no contours and is left out
    QCOMPARE(set.getRois().size(), 2);
    QCOMPARE(set.getRois().at(0).name, QString("PTV"));
    QCOMPARE(set.getRois().at(0).color, QColor(255, 0, 0));
    QCOMPARE(set.getRois().at(0).contours.size(), 2);
    QVERIFY(set.getRois().at(0).contours.at(0).closed);
    QCOMPARE(set.getRois().at(0).contours.at(0).points.size(), 4);
    QCOMPARE(set.getRois().at(0).contours.at(1).points.at(0)[2], 12.5);
    QCOMPARE(set.getRois().at(1).name, QString("Cord"));
    QCOMPARE(set.getRois().at(1).color, QColor(0, 255, 0));
    // The point was left out: one open contour of 3 points
    QCOMPARE(set.getRois().at(1).contours.size(), 1);
    QVERIFY(!set.getRois().at(1).contours.at(0).closed);
    QCOMPARE(set.getNumberOfContours(), 3);
}

void test_RtStructureSet::fromDICOM_ShouldFail_WhenItIsNotAStructureSetOrHasNoContours()
{
    QString error;
    QVERIFY(read(createDataset("1.2.840.10008.5.1.4.1.1.2"), &error).isNull());
    QVERIFY(!error.isEmpty());

    DcmDataset *empty = new DcmDataset;
    empty->putAndInsertString(DCM_SOPClassUID, StructureSetClass);
    error.clear();
    QVERIFY(read(empty, &error).isNull());
    QVERIFY(!error.isEmpty());
}

void test_RtStructureSet::fromDICOM_ShouldKeepTheFrameOfReference()
{
    RtStructureSet set = read(createDataset());

    QVERIFY(set.isInFrameOfReference("1.2.3.FOR"));
    QVERIFY(set.isInFrameOfReference(" 1.2.3.FOR "));
    QVERIFY(!set.isInFrameOfReference("1.2.3.OTHER"));
    QVERIFY(!set.isInFrameOfReference(""));
}

void test_RtStructureSet::toPixel_ShouldGiveThePixelOfThePointInThePlane()
{
    RtStructureSet::PlaneGeometry geometry = axialGeometry();

    QPointF pixel = RtStructureSet::toPixel(geometry, {{10.0, 30.0, 99.0}});
    QCOMPARE(pixel, QPointF(30.0, 40.0));
    // The first pixel is 0, 0
    QCOMPARE(RtStructureSet::toPixel(geometry, {{-50.0, -50.0, 0.0}}), QPointF(0.0, 0.0));
}

void test_RtStructureSet::depthOf_ShouldGiveTheDistanceAlongTheNormal()
{
    RtStructureSet::PlaneGeometry geometry = axialGeometry();

    // row x column = +z
    QCOMPARE(RtStructureSet::depthOf(geometry, {{0.0, 0.0, 7.5}}), 7.5);
    QCOMPARE(RtStructureSet::depthOf(geometry, {{80.0, -20.0, -2.0}}), -2.0);
}

void test_RtStructureSet::contoursAtDepth_ShouldGiveTheContoursOfTheSliceOnly()
{
    RtStructureSet set = read(createDataset());
    RtStructureSet::PlaneGeometry geometry = axialGeometry();

    QList<RtStructureSet::DrawnContour> at10 = set.contoursAtDepth(geometry, 10.0, 1.25);
    // The square of the PTV and the open contour of the Cord, which are at z = 10
    QCOMPARE(at10.size(), 2);
    QCOMPARE(at10.at(0).roiName, QString("PTV"));
    QCOMPARE(at10.at(0).color, QColor(255, 0, 0));
    QCOMPARE(at10.at(0).pixels.at(0), QPointF(30.0, 30.0));
    QCOMPARE(at10.at(0).pixels.at(2), QPointF(40.0, 40.0));

    QList<RtStructureSet::DrawnContour> at125 = set.contoursAtDepth(geometry, 12.5, 1.25);
    QCOMPARE(at125.size(), 1);
    QCOMPARE(at125.at(0).roiName, QString("PTV"));

    QCOMPARE(set.contoursAtDepth(geometry, 50.0, 1.25).size(), 0);
}

void test_RtStructureSet::contoursAtDepth_ShouldClosePolygonsAndLeaveOpenOnesOpen()
{
    RtStructureSet set = read(createDataset());
    QList<RtStructureSet::DrawnContour> at10 = set.contoursAtDepth(axialGeometry(), 10.0, 1.25);

    QVERIFY(at10.at(0).closed);
    QCOMPARE(at10.at(0).pixels.size(), 5);
    QCOMPARE(at10.at(0).pixels.first(), at10.at(0).pixels.last());

    QVERIFY(!at10.at(1).closed);
    QCOMPARE(at10.at(1).pixels.size(), 3);
}

void test_RtStructureSet::contoursAtDepth_ShouldUseTheTolerance()
{
    RtStructureSet set = read(createDataset());
    RtStructureSet::PlaneGeometry geometry = axialGeometry();

    // The slice at 11.25 is between the contours at 10 and 12.5, both 1.25 away
    QCOMPARE(set.contoursAtDepth(geometry, 11.25, 1.25).size(), 3);
    QCOMPARE(set.contoursAtDepth(geometry, 11.25, 1.0).size(), 0);
}

void test_RtStructureSet::contoursAtDepth_ShouldWorkInATiltedPlane()
{
    // The same point at z = 10 on a plane that is tilted by 30 degrees around the x axis: its depth is along the tilted normal
    RtStructureSet::PlaneGeometry geometry;
    double angle = 30.0 * 3.14159265358979323846 / 180.0;
    geometry.origin = {{0.0, 0.0, 0.0}};
    geometry.row = {{1.0, 0.0, 0.0}};
    geometry.column = {{0.0, std::cos(angle), std::sin(angle)}};
    geometry.spacingX = 1.0;
    geometry.spacingY = 1.0;

    // normal = row x column = (0, -sin, cos)
    double depth = RtStructureSet::depthOf(geometry, {{0.0, 0.0, 10.0}});
    QVERIFY(std::abs(depth - 10.0 * std::cos(angle)) < 1e-9);

    QPointF pixel = RtStructureSet::toPixel(geometry, {{3.0, 0.0, 10.0}});
    QVERIFY(std::abs(pixel.x() - 3.0) < 1e-9);
    // The part of the point along the columns: 10 sin(30) = 5
    QVERIFY(std::abs(pixel.y() - 5.0) < 1e-9);
}

void test_RtStructureSet::study_ShouldKeepEachStructureSetOnce()
{
    Study study;
    RtStructureSet set = read(createDataset());

    QVERIFY(study.addRtStructureSet(set));
    QVERIFY(!study.addRtStructureSet(set));
    QVERIFY(!study.addRtStructureSet(RtStructureSet()));
    QCOMPARE(study.getRtStructureSets().size(), 1);
}

DECLARE_TEST(test_RtStructureSet)

#include "test_rtstructureset.moc"
