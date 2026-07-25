//------------ kuvbur 2026 ------------
// ArchicadAdapter.cpp — Phase 2 Implementation
// Collects Labels and Dimensions, normalizes to Paper Space mm (LayoutOptimizer types)

#include "ArchicadAdapter.hpp"
#include "APICommon25.h"
#include <algorithm>

namespace ArchicadAdapter {

using LO = LayoutOptimizer;

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

// BNZeroMemory wrapper for API structures
template<typename T>
inline void BNZero (T& obj) {
    BNZeroMemory (&obj, sizeof (T));
}

// Get active view scale denominator (1:100 -> 100)
double GetActiveViewScaleDenominator () {
    API_DatabaseInfo dbInfo = {};
    BNZero (dbInfo);
    GSErrCode err = ACAPI_Database (APIDb_GetCurrentDatabaseID, &dbInfo, nullptr);
    if (err != NoError) return 100.0; // fallback

    API_WindowInfo winInfo = {};
    BNZero (winInfo);
    err = ACAPI_Interface (APIIo_GetWindowInfoID, &winInfo, nullptr);
    if (err != NoError) return 100.0;

    // winInfo.scaleDenom is the denominator (100 for 1:100)
    return winInfo.scaleDenom > 0 ? static_cast<double> (winInfo.scaleDenom) : 100.0;
}

// Convert model coordinate to paper space (mm)
// Model coords are in meters; paper space = model * 1000 / scaleDenom
// Y-axis: model +Y up -> paper +Y down (flip)
LO::Point2D ModelToPaperSpace (const API_Coord& modelCoord, double viewScaleDenom) {
    const double scaleFactor = 1000.0 / viewScaleDenom; // m -> mm at scale
    return {
        modelCoord.x * scaleFactor,
        -modelCoord.y * scaleFactor  // flip Y: model up -> paper down
    };
}

// Check if element or its layer is locked
bool IsElementLocked (const API_Guid& guid) {
    API_Element element = {};
    BNZero (element);
    element.header.guid = guid;
    element.header.typeID = API_ZombieElemID;

    GSErrCode err = ACAPI_Element_Get (&element);
    if (err != NoError) return false;

    // Check element-level lock
    if (element.header.floorInd & 0x8000) return true; // lock bit in floorInd (common pattern)

    // Check layer lock
    API_Attribute layerAttr = {};
    BNZero (layerAttr);
    layerAttr.header.typeID = API_LayerID;
    layerAttr.header.index = element.header.layer;
    if (ACAPI_Attribute_Get (&layerAttr) == NoError) {
        // Layer flags: bit 0 = locked
        if (layerAttr.layer.locked) return true;
    }

    return false;
}

// Generate deterministic ElementId from GUID
LO::ElementId GetElementId (const API_Guid& guid) {
    // Use low 64 bits of GUID for deterministic ordering
    // GUID layout: time_low (32), time_mid (16), time_hi_and_version (16), clock_seq (16), node (48)
    LO::ElementId id = 0;
    id |= static_cast<LO::ElementId> (guid.time_low) << 32;
    id |= static_cast<LO::ElementId> (guid.time_mid) << 16;
    id |= static_cast<LO::ElementId> (guid.time_hi_and_version);
    return id;
}

// -----------------------------------------------------------------------------
// Text box extraction
// -----------------------------------------------------------------------------

LO::Rect GetTextBoxPaperSpace (const API_Element& elem, double viewScaleDenom) {
    LO::Rect box = {};

    // Get element memo for text block info
    API_ElementMemo memo = {};
    BNZero (memo);
    GSErrCode err = ACAPI_Element_GetMemo (elem.header.guid, &memo);
    if (err != NoError) return box;

    // Labels and Dimensions store text in memo.textBlocks
    if (memo.textBlocks != nullptr && memo.textBlocksSize > 0) {
        // First text block is typically the main text
        API_TextBlock* tb = memo.textBlocks;
        API_Coord pos = tb->location; // text insertion point in model space

        // Estimate text box from font size and string length (approximate)
        double fontSizeMm = tb->fontSize * 1000.0 / viewScaleDenom; // model mm -> paper mm
        double charWidthMm = fontSizeMm * 0.6; // rough average
        double textWidthMm = strlen (tb->text) * charWidthMm;
        double textHeightMm = fontSizeMm * 1.2;

        // Convert position to paper space
        LO::Point2D p = ModelToPaperSpace (pos, viewScaleDenom);

        // Text box: top-left = insertion point (approximate)
        // Alignment affects this; default assume left-top
        box.left = p.x;
        box.top = p.y - textHeightMm;
        box.right = p.x + textWidthMm;
        box.bottom = p.y;
        box.Normalize();
    }

    ACAPI_DisposeElemMemoHdls (&memo);
    return box;
}

// -----------------------------------------------------------------------------
// Leader polyline extraction (Label elements)
// -----------------------------------------------------------------------------

std::vector<LO::Point2D> GetLeaderPolylinePaperSpace (const API_Element& labelElem, double viewScaleDenom) {
    std::vector<LO::Point2D> poly;

    if (labelElem.header.typeID != API_LabelID) return poly;

    API_ElementMemo memo = {};
    BNZero (memo);
    GSErrCode err = ACAPI_Element_GetMemo (labelElem.header.guid, &memo);
    if (err != NoError) return poly;

    // Label polyline points are in memo.polygon
    if (memo.polygon != nullptr && memo.polygon->nVertices >= 2) {
        for (Int32 i = 0; i < memo.polygon->nVertices; ++i) {
            poly.push_back (ModelToPaperSpace (memo.polygon->coords[i], viewScaleDenom));
        }
    }

    ACAPI_DisposeElemMemoHdls (&memo);
    return poly;
}

// -----------------------------------------------------------------------------
// Dimension measure points extraction
// -----------------------------------------------------------------------------

std::vector<LO::Point2D> GetDimensionMeasurePoints (const API_Element& dimElem, double viewScaleDenom) {
    std::vector<LO::Point2D> pts;

    if (dimElem.header.typeID != API_DimensionID) return pts;

    API_ElementMemo memo = {};
    BNZero (memo);
    GSErrCode err = ACAPI_Element_GetMemo (dimElem.header.guid, &memo);
    if (err != NoError) return pts;

    // Dimension measure points in memo.polygon (or dimElem.dimension.lineType specific)
    if (memo.polygon != nullptr && memo.polygon->nVertices >= 2) {
        for (Int32 i = 0; i < memo.polygon->nVertices; ++i) {
            pts.push_back (ModelToPaperSpace (memo.polygon->coords[i], viewScaleDenom));
        }
    }

    ACAPI_DisposeElemMemoHdls (&memo);
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
        API_ElemTypeID typeID = API_LabelID;
        GS::Array<API_Guid> guids;
        GSErrCode err = ACAPI_Element_GetElemList (typeID, &guids);
        if (err == NoError) {
            for (const API_Guid& guid : guids) {
                API_Element elem = {};
                BNZero (elem);
                elem.header.guid = guid;
                elem.header.typeID = typeID;
                if (ACAPI_Element_Get (&elem) == NoError) {
                    LO::LeaderElement le;
                    le.id = GetElementId (guid);
                    le.locked = IsElementLocked (guid);

                    // Geometry (immutable): anchor point = first vertex of leader polyline
                    le.layout.leaderPolyline = GetLeaderPolylinePaperSpace (elem, viewScaleDenom);
                    if (!le.layout.leaderPolyline.empty()) {
                        le.geo.anchorPoint = le.layout.leaderPolyline[0];
                    }

                    // Layout (mutable): text box
                    le.layout.textBox = GetTextBoxPaperSpace (elem, viewScaleDenom);
                    le.layout.textPosition = le.layout.textBox.Center();

                    // Shoulder length: distance anchor->second point (if exists)
                    if (le.layout.leaderPolyline.size() >= 2) {
                        LO::Point2D d = le.layout.leaderPolyline[1] - le.layout.leaderPolyline[0];
                        le.geo.shoulderLength = std::sqrt (d.x * d.x + d.y * d.y);
                    }

                    result.leaders.push_back (std::move (le));
                }
            }
        }
    }

    // Collect Dimensions (API_DimensionID)
    {
        API_ElemTypeID typeID = API_DimensionID;
        GS::Array<API_Guid> guids;
        GSErrCode err = ACAPI_Element_GetElemList (typeID, &guids);
        if (err == NoError) {
            for (const API_Guid& guid : guids) {
                API_Element elem = {};
                BNZero (elem);
                elem.header.guid = guid;
                elem.header.typeID = typeID;
                if (ACAPI_Element_Get (&elem) == NoError) {
                    LO::DimensionElement de;
                    de.id = GetElementId (guid);
                    de.locked = IsElementLocked (guid);

                    // Geometry: measure points
                    de.geo.measurePoints = GetDimensionMeasurePoints (elem, viewScaleDenom);

                    // Direction: first segment direction
                    if (de.geo.measurePoints.size() >= 2) {
                        LO::Point2D dir = de.geo.measurePoints[1] - de.geo.measurePoints[0];
                        double len = std::sqrt (dir.x * dir.x + dir.y * dir.y);
                        if (len > 1e-6) {
                            de.geo.direction = {dir.x / len, dir.y / len};
                        }
                    }

                    // Layout: text box
                    de.layout.textBox = GetTextBoxPaperSpace (elem, viewScaleDenom);
                    de.layout.textPosition = de.layout.textBox.Center();

                    result.dimensions.push_back (std::move (de));
                }
            }
        }
    }

    return result;
}

} // namespace ArchicadAdapter