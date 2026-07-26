//------------ kuvbur 2026 ------------
// ArchicadAdapter — Phase 2: Collect & Normalize Elements
// ACAPI allowed here. Core (LayoutOptimizer) must NOT include this.
// See AGENTS.md Section 2 Rule 1, Section 4 (Architecture), Section 5 (Data Model).

#pragma once
#if !defined (ARCHICAD_ADAPTER_HPP)
#define ARCHICAD_ADAPTER_HPP

#include "ACAPinc.h"
#include "APIEnvir.h"
#include "LayoutOptimizer.hpp"

namespace ArchicadAdapter {

// -----------------------------------------------------------------------------
// Element collection & normalization to LayoutOptimizer types
// -----------------------------------------------------------------------------

// Collect all relevant annotation elements from current view/floor plan
// Returns Leaders (labels) and Dimensions ready for LayoutOptimizer
struct CollectResult {
    std::vector<LayoutOptimizer::LeaderElement> leaders;
    std::vector<API_Guid> leaderGuids;
    std::vector<LayoutOptimizer::DimensionElement> dimensions;
    std::vector<API_Guid> dimensionGuids;
    GSErrCode error = NoError;
};

// Main entry: collect all label and dimension elements on active floor plan view
CollectResult CollectElements ();

// Convert API_Coord (model space) to Paper Space mm per Section 3:
//   +X right, +Y down, units = mm on paper
LayoutOptimizer::Point2D ModelToPaperSpace (const API_Coord& modelCoord, double viewScale);

// Extract text bounding box in Paper Space mm from API_Element (Label or Dimension)
LayoutOptimizer::Rect GetTextBoxPaperSpace (const API_Element& elem, double viewScale);

// Extract leader polyline (anchor -> shoulder -> text) in Paper Space mm
std::vector<LayoutOptimizer::Point2D> GetLeaderPolylinePaperSpace (const API_Element& labelElem, double viewScale);

// Extract dimension measure points in Paper Space mm
std::vector<LayoutOptimizer::Point2D> GetDimensionMeasurePoints (const API_Element& dimElem, double viewScale);

// Get view scale denominator (e.g., 100 for 1:100)
double GetActiveViewScaleDenominator ();

// Check if element is locked (user-locked or layer-locked)
bool IsElementLocked (const API_Guid& guid);

// Get element ID as uint64 for deterministic ordering
LayoutOptimizer::ElementId GetElementId (const API_Guid& guid);

// Full pipeline: Collect -> Solve -> Apply (wrapped in Undo scope)
GSErrCode RunOptimization ();

} // namespace ArchicadAdapter

#endif // ARCHICAD_ADAPTER_HPP