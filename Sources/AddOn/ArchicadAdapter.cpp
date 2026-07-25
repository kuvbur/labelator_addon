//------------ kuvbur 2026 ------------
// ArchicadAdapter.cpp — Phase 2 Implementation
// Collects Labels and Dimensions, normalizes to Paper Space mm (LayoutOptimizer types)

#include "ACAPinc.h"
#include "APIEnvir.h"
#include "APICommon25.h"
#include "ArchicadAdapter.hpp"
#include "LayoutOptimizer.hpp"
#include <algorithm>
#include <cstring>

namespace ArchicadAdapter {

    // -----------------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------------

    template <typename T> inline void BNZero (T &obj) { BNZeroMemory (&obj, sizeof (T)); }

    double GetActiveViewScaleDenominator () {
        double scale = 100.0;
        GSErrCode err = ACAPI_Database (APIDb_GetDrawingScaleID, &scale, nullptr);
        if (err == NoError && scale > 0.0)
            return scale;
        return 100.0;
    }

    LayoutOptimizer::Point2D ModelToPaperSpace (const API_Coord &modelCoord, double viewScaleDenom) {
        const double scaleFactor = 1000.0 / viewScaleDenom;
        return {
            modelCoord.x * scaleFactor,
            -modelCoord.y * scaleFactor
        };
    }

    bool IsElementLocked (const API_Guid &guid) {
        API_Element element = {};
        BNZero (element);
        element.header.guid = guid;
        GSErrCode err = ACAPI_Element_Get (&element);
        if (err != NoError)
            return false;

        if (element.header.floorInd & 0x8000)
            return true;

        API_Attribute layerAttr = {};
        BNZero (layerAttr);
        layerAttr.header.typeID = API_LayerID;
        layerAttr.header.index = element.header.layer;
        if (ACAPI_Attribute_Get (&layerAttr) == NoError) {
            if (layerAttr.layer.head.flags & 1)
                return true;
        }

        return false;
    }

    LayoutOptimizer::ElementId GetElementId (const API_Guid &guid) {
        LayoutOptimizer::ElementId id = 0;
        id |= static_cast<LayoutOptimizer::ElementId> (guid.time_low) << 32;
        id |= static_cast<LayoutOptimizer::ElementId> (guid.time_mid) << 16;
        id |= static_cast<LayoutOptimizer::ElementId> (guid.time_hi_and_version);
        return id;
    }

    // -----------------------------------------------------------------------------
    // Text box extraction
    // -----------------------------------------------------------------------------

    LayoutOptimizer::Rect GetTextBoxPaperSpace (const API_Element &elem, double viewScaleDenom) {
        LayoutOptimizer::Rect box = {};

        API_ElementMemo memo = {};
        BNZero (memo);
        GSErrCode err = ACAPI_Element_GetMemo (elem.header.guid, &memo, APIMemoMask_TextContent);
        if (err != NoError)
            return box;

        // Estimate text box dimensions
        double textWidthMm = 50.0; // default estimate
        double textHeightMm = 3.0; // default estimate

        if (memo.paragraphs != nullptr && memo.paragraphs[0] != nullptr) {
            const API_ParagraphType &para = memo.paragraphs[0][0];
            double fontSizeMm = para.height > 0.0 ? para.height * 1000.0 / viewScaleDenom : 2.5;
            double charWidthMm = fontSizeMm * 0.6;

            if (memo.textContent != nullptr && memo.textContent[0] != nullptr) {
                textWidthMm = strlen (memo.textContent[0]) * charWidthMm;
                textHeightMm = fontSizeMm * 1.2;
            }
        }

        // Use element position (refC for dimensions, begC for labels) as text insertion point
        API_Coord pos = {};
        if (elem.header.typeID == API_DimensionID) {
            pos = elem.dimension.refC;
        } else if (elem.header.typeID == API_LabelID) {
            pos = elem.label.begC;
        }

        LayoutOptimizer::Point2D p = ModelToPaperSpace (pos, viewScaleDenom);

        box.left = p.x;
        box.top = p.y - textHeightMm;
        box.right = p.x + textWidthMm;
        box.bottom = p.y;
        box.Normalize ();

        ACAPI_DisposeElemMemoHdls (&memo);
        return box;
    }

    // -----------------------------------------------------------------------------
    // Leader polyline extraction (Label elements)
    // -----------------------------------------------------------------------------

    std::vector<LayoutOptimizer::Point2D> GetLeaderPolylinePaperSpace (const API_Element &labelElem,
                                                                       double viewScaleDenom) {
        std::vector<LayoutOptimizer::Point2D> poly;

        API_Coord begC = labelElem.label.begC;
        API_Coord midC = labelElem.label.midC;
        API_Coord endC = labelElem.label.endC;

        poly.push_back (ModelToPaperSpace (begC, viewScaleDenom));
        if (midC.x != begC.x || midC.y != begC.y)
            poly.push_back (ModelToPaperSpace (midC, viewScaleDenom));
        if (endC.x != begC.x || endC.y != begC.y)
            poly.push_back (ModelToPaperSpace (endC, viewScaleDenom));

        return poly;
    }

    // -----------------------------------------------------------------------------
    // Dimension measure points extraction
    // -----------------------------------------------------------------------------

    std::vector<LayoutOptimizer::Point2D> GetDimensionMeasurePoints (const API_Element &dimElem,
                                                                     double viewScaleDenom) {
        std::vector<LayoutOptimizer::Point2D> pts;

        const API_DimensionType &dim = dimElem.dimension;

        pts.push_back (ModelToPaperSpace (dim.refC, viewScaleDenom));

        API_Coord dirEnd = {
            dim.refC.x + dim.direction.x,
            dim.refC.y + dim.direction.y
        };
        pts.push_back (ModelToPaperSpace (dirEnd, viewScaleDenom));

        return pts;
    }

    // -----------------------------------------------------------------------------
    // Element collection
    // -----------------------------------------------------------------------------

    CollectResult CollectElements () {
        CollectResult result;

        double viewScaleDenom = GetActiveViewScaleDenominator ();

        // Collect Labels (API_LabelID)
        {
            GS::Array<API_Guid> guids;
            GSErrCode err = ACAPI_Element_GetElemList (API_LabelID, &guids);
            if (err == NoError) {
                for (UInt32 i = 0; i < guids.GetSize (); ++i) {
                    const API_Guid &guid = guids[i];
                    API_Element elem = {};
                    BNZero (elem);
                    elem.header.guid = guid;

                    err = ACAPI_Element_Get (&elem);
                    if (err != NoError)
                        continue;

                    LayoutOptimizer::LeaderElement le;
                    le.id = GetElementId (guid);
                    le.locked = IsElementLocked (guid);

                    le.geo.anchorPoint = ModelToPaperSpace (elem.label.begC, viewScaleDenom);

                    le.layout.leaderPolyline = GetLeaderPolylinePaperSpace (elem, viewScaleDenom);
                    le.layout.textBox = GetTextBoxPaperSpace (elem, viewScaleDenom);
                    le.layout.textPosition = le.layout.textBox.Center ();

                    if (le.layout.leaderPolyline.size () >= 2) {
                        LayoutOptimizer::Point2D d = le.layout.leaderPolyline[1] - le.layout.leaderPolyline[0];
                        le.geo.shoulderLength = std::sqrt (d.x * d.x + d.y * d.y);
                    }

                    result.leaders.push_back (std::move (le));
                }
            }
        }

        // Collect Dimensions (API_DimensionID)
        {
            GS::Array<API_Guid> guids;
            GSErrCode err = ACAPI_Element_GetElemList (API_DimensionID, &guids);
            if (err == NoError) {
                for (UInt32 i = 0; i < guids.GetSize (); ++i) {
                    const API_Guid &guid = guids[i];
                    API_Element elem = {};
                    BNZero (elem);
                    elem.header.guid = guid;

                    err = ACAPI_Element_Get (&elem);
                    if (err != NoError)
                        continue;

                    LayoutOptimizer::DimensionElement de;
                    de.id = GetElementId (guid);
                    de.locked = IsElementLocked (guid);

                    de.geo.measurePoints = GetDimensionMeasurePoints (elem, viewScaleDenom);

                    if (de.geo.measurePoints.size () >= 2) {
                        LayoutOptimizer::Point2D dir = de.geo.measurePoints[1] - de.geo.measurePoints[0];
                        double len = std::sqrt (dir.x * dir.x + dir.y * dir.y);
                        if (len > 1e-6) {
                            de.geo.direction = {dir.x / len, dir.y / len};
                        }
                    }

                    de.layout.textBox = GetTextBoxPaperSpace (elem, viewScaleDenom);
                    de.layout.textPosition = de.layout.textBox.Center ();

                    result.dimensions.push_back (std::move (de));
                }
            }
        }

        return result;
    }

} // namespace ArchicadAdapter