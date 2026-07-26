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

    API_Coord PaperSpaceToModel (const LayoutOptimizer::Point2D &paperCoord, double viewScaleDenom) {
        const double scaleFactor = viewScaleDenom / 1000.0;
        return {
            paperCoord.x * scaleFactor,
            -paperCoord.y * scaleFactor
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

                    result.leaderGuids.push_back (guid);
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

                    result.dimensionGuids.push_back (guid);
                    result.dimensions.push_back (std::move (de));
                }
            }
        }

        return result;
    }

    // -----------------------------------------------------------------------------
    // Apply changes back to Archicad elements (wrapped in Undo scope)
    // -----------------------------------------------------------------------------

    GSErrCode ApplyChanges (const CollectResult& original,
                            const std::vector<LayoutOptimizer::LeaderElement>& solvedLeaders,
                            const std::vector<LayoutOptimizer::DimensionElement>& solvedDimensions,
                            double viewScaleDenom) {
        // Wrap all modifications in a single undoable command
        return ACAPI_CallUndoableCommand (GS::UniString ("Optimize Annotations"), [&] () -> GSErrCode {
            GSErrCode err = NoError;

            // Apply label changes
            for (size_t i = 0; i < solvedLeaders.size () && i < original.leaderGuids.size (); ++i) {
                const API_Guid &guid = original.leaderGuids[i];
                const LayoutOptimizer::LeaderElement &solved = solvedLeaders[i];

                if (solved.locked)
                    continue;

                API_Element elem = {};
                BNZero (elem);
                elem.header.guid = guid;

                err = ACAPI_Element_Get (&elem);
                if (err != NoError)
                    continue;

                // Build mask for label geometry changes
                API_Element mask;
                BNZero (mask);
                ACAPI_ELEMENT_MASK_CLEAR (mask);
                ACAPI_ELEMENT_MASK_SET (mask, API_LabelType, begC);
                ACAPI_ELEMENT_MASK_SET (mask, API_LabelType, midC);
                ACAPI_ELEMENT_MASK_SET (mask, API_LabelType, endC);
                if (elem.label.labelClass == APILblClass_Text) {
                    ACAPI_ELEMENT_MASK_SET (mask, API_LabelType, u.text.loc);
                }

                // Update label leader polyline (begC, midC, endC)
                if (solved.layout.leaderPolyline.size () >= 1) {
                    elem.label.begC = PaperSpaceToModel (solved.layout.leaderPolyline[0], viewScaleDenom);
                }
                if (solved.layout.leaderPolyline.size () >= 2) {
                    elem.label.midC = PaperSpaceToModel (solved.layout.leaderPolyline[1], viewScaleDenom);
                }
                if (solved.layout.leaderPolyline.size () >= 3) {
                    elem.label.endC = PaperSpaceToModel (solved.layout.leaderPolyline[2], viewScaleDenom);
                } else if (solved.layout.leaderPolyline.size () == 2) {
                    // If only 2 points, use second point as endC as well
                    elem.label.endC = elem.label.midC;
                }

                // Update text position for text labels
                if (elem.label.labelClass == APILblClass_Text) {
                    elem.label.u.text.loc = PaperSpaceToModel (solved.layout.textPosition, viewScaleDenom);
                }

                // Change the element
                API_ElementMemo memo = {};
                BNZero (memo);
                err = ACAPI_Element_Change (&elem, &mask, &memo, 0, true);
                ACAPI_DisposeElemMemoHdls (&memo);
                if (err != NoError)
                    return err;
            }

            // Apply dimension changes
            for (size_t i = 0; i < solvedDimensions.size () && i < original.dimensionGuids.size (); ++i) {
                const API_Guid &guid = original.dimensionGuids[i];
                const LayoutOptimizer::DimensionElement &solved = solvedDimensions[i];

                if (solved.locked)
                    continue;

                API_Element elem = {};
                BNZero (elem);
                elem.header.guid = guid;

                err = ACAPI_Element_Get (&elem);
                if (err != NoError)
                    continue;

                // Build mask for dimension text position
                API_Element mask;
                BNZero (mask);
                ACAPI_ELEMENT_MASK_CLEAR (mask);
                ACAPI_ELEMENT_MASK_SET (mask, API_DimensionType, defNote.pos);

                // Update text position
                if (solved.layout.textPosition.x != 0.0 || solved.layout.textPosition.y != 0.0) {
                    elem.dimension.defNote.pos = PaperSpaceToModel (solved.layout.textPosition, viewScaleDenom);
                }

                // Change the element
                API_ElementMemo memo = {};
                BNZero (memo);
                err = ACAPI_Element_Change (&elem, &mask, &memo, 0, true);
                ACAPI_DisposeElemMemoHdls (&memo);
                if (err != NoError)
                    return err;
            }

            return NoError;
        });
    }

    // -----------------------------------------------------------------------------
    // Full pipeline: Collect -> Solve -> Apply
    // -----------------------------------------------------------------------------

    GSErrCode SolveAndApply () {
        // Collect elements
        CollectResult collected = CollectElements ();
        if (collected.error != NoError)
            return collected.error;

        // Prepare input for LayoutOptimizer
        std::vector<LayoutOptimizer::ElementInput> inputs;

        // Add leaders
        for (const auto &le : collected.leaders) {
            LayoutOptimizer::ElementInput input;
            input.type = LayoutOptimizer::ElementType::Leader;
            input.locked = le.locked;
            input.id = le.id;
            input.initialTextPosition = le.layout.textPosition;
            // Generate candidates for this leader (simplified: use current position as one candidate)
            LayoutOptimizer::Candidate cand;
            cand.textPosition = le.layout.textPosition;
            cand.textBox = le.layout.textBox;
            cand.leaderPolyline = le.layout.leaderPolyline;
            input.candidates.push_back (cand);
            inputs.push_back (std::move (input));
        }

        // Add dimensions
        for (const auto &de : collected.dimensions) {
            LayoutOptimizer::ElementInput input;
            input.type = LayoutOptimizer::ElementType::Dimension;
            input.locked = de.locked;
            input.id = de.id;
            input.initialTextPosition = de.layout.textPosition;
            LayoutOptimizer::Candidate cand;
            cand.textPosition = de.layout.textPosition;
            cand.textBox = de.layout.textBox;
            input.candidates.push_back (cand);
            inputs.push_back (std::move (input));
        }

        // Forbidden geometry (empty for now)
        std::vector<LayoutOptimizer::Rect> forbiddenGeometry;
        std::vector<std::vector<LayoutOptimizer::Point2D>> obstaclePolylines;

        // Solve
        LayoutOptimizer::SolveResult solveResult = LayoutOptimizer::Solve (inputs, forbiddenGeometry, obstaclePolylines);

        // Extract solved leaders and dimensions
        std::vector<LayoutOptimizer::LeaderElement> solvedLeaders;
        std::vector<LayoutOptimizer::DimensionElement> solvedDimensions;

        size_t leaderIdx = 0;
        size_t dimIdx = 0;
        for (size_t i = 0; i < solveResult.selectedCandidates.size () && i < inputs.size (); ++i) {
            const LayoutOptimizer::Candidate &cand = solveResult.selectedCandidates[i];
            const LayoutOptimizer::ElementInput &input = inputs[i];

            if (input.type == LayoutOptimizer::ElementType::Leader && leaderIdx < collected.leaders.size ()) {
                LayoutOptimizer::LeaderElement le = collected.leaders[leaderIdx];
                le.layout.textPosition = cand.textPosition;
                le.layout.textBox = cand.textBox;
                le.layout.leaderPolyline = cand.leaderPolyline;
                solvedLeaders.push_back (std::move (le));
                ++leaderIdx;
            } else if (input.type == LayoutOptimizer::ElementType::Dimension && dimIdx < collected.dimensions.size ()) {
                LayoutOptimizer::DimensionElement de = collected.dimensions[dimIdx];
                de.layout.textPosition = cand.textPosition;
                de.layout.textBox = cand.textBox;
                solvedDimensions.push_back (std::move (de));
                ++dimIdx;
            }
        }

        // Apply changes
        double viewScaleDenom = GetActiveViewScaleDenominator ();
        return ApplyChanges (collected, solvedLeaders, solvedDimensions, viewScaleDenom);
    }

    // -----------------------------------------------------------------------------
    // Public entry point
    // -----------------------------------------------------------------------------

    GSErrCode RunOptimization () {
        return SolveAndApply ();
    }

} // namespace ArchicadAdapter