//------------ kuvbur 2026 ------------
// LayoutOptimizer Core - Phase 3: Candidate Generation & Filtering
// Pure C++17, no ACAPI dependencies

#pragma once
#if !defined (CANDIDATE_GENERATOR_HPP)
#define CANDIDATE_GENERATOR_HPP

#include "LayoutOptimizer.hpp"
#include <vector>

namespace LayoutOptimizer {

// -----------------------------------------------------------------------------
// CandidateGenerator class
// Generates 8 angles × 3 distances = 24 raw candidates per element
// plus the guaranteed fallback candidate (current position)
// -----------------------------------------------------------------------------

struct CandidateGenerator {
    // Generate candidates for a Leader element
    // anchor = LeaderGeometry::anchorPoint, currentLayout = LeaderLayout
    // textBoxSize = current text box dimensions (preserved)
    // obstacles = forbidden geometry rects (for filtering step 2)
    // obstaclePolylines = obstacle line segments (for filtering step 3)
    // viewBoundary = drawing boundary rect (for filtering step 1)
    static std::vector<Candidate> GenerateForLeader (
        const Point2D& anchor,
        const LeaderLayout& currentLayout,
        const std::vector<Rect>& obstacles,
        const std::vector<std::vector<Point2D>>& obstaclePolylines,
        const Rect& viewBoundary
    );

    // Generate candidates for a Dimension element
    // measurePoint = first point of DimensionGeometry::measurePoints
    // currentLayout = DimensionLayout
    // direction = DimensionGeometry::direction (normalized, for perpendicular offsets)
    // obstacles = forbidden geometry rects (for filtering step 2)
    // obstaclePolylines = obstacle line segments (for filtering step 3)
    // viewBoundary = drawing boundary rect (for filtering step 1)
    static std::vector<Candidate> GenerateForDimension (
        const Point2D& measurePoint,
        const DimensionLayout& currentLayout,
        const Point2D& direction,
        const std::vector<Rect>& obstacles,
        const std::vector<std::vector<Point2D>>& obstaclePolylines,
        const Rect& viewBoundary
    );

private:
    // Helper: build candidate at specific angle/distance
    // shoulderLength is used for leaders (from LeaderGeometry), ignored for dimensions (pass 0)
    static Candidate MakeCandidateInternal (
        const Point2D& anchor,
        double angleDeg,
        double distanceMm,
        const Rect& textBoxSize,
        bool isFallback,
        double shoulderLength
    );

    // Filter Step 1: View boundary check
    static bool PassesViewBoundary (const Candidate& cand, const Rect& viewBoundary);

    // Filter Step 2: Forbidden geometry check
    static bool PassesForbiddenGeometry (const Candidate& cand, const std::vector<Rect>& obstacles);

    // Filter Step 3: Obstacle line crossing check (Phase 4)
    static bool PassesObstacleLines (const Candidate& cand,
                                      const std::vector<std::vector<Point2D>>& obstaclePolylines);
};

} // namespace LayoutOptimizer

#endif // CANDIDATE_GENERATOR_HPP