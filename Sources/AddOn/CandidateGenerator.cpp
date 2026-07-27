//------------ kuvbur 2026 ------------
// LayoutOptimizer Core - Phase 3: Candidate Generation & Filtering
// Pure C++17, no ACAPI dependencies

#include "CandidateGenerator.hpp"
#include "LayoutOptimizer.hpp"
#include <cmath>
#include <algorithm>

namespace LayoutOptimizer {

// -----------------------------------------------------------------------------
// CandidateGenerator::MakeCandidateInternal
// -----------------------------------------------------------------------------

static inline double DegToRad (double deg) {
    return deg * 3.14159265358979323846 / 180.0;
}

Candidate CandidateGenerator::MakeCandidateInternal (
    const Point2D& anchor,
    double angleDeg,
    double distanceMm,
    const Rect& textBoxSize,
    bool isFallback,
    double shoulderLength
) {
    Candidate cand;
    cand.angle = angleDeg;
    cand.valid = true;

    if (isFallback) {
        // Fallback uses current text position (already set by caller)
        // textBox and leaderPolyline should be set by caller
    } else {
        // Compute text position from anchor + angle + distance
        double rad = DegToRad (angleDeg);
        cand.textPosition.x = anchor.x + distanceMm * std::cos (rad);
        cand.textPosition.y = anchor.y + distanceMm * std::sin (rad); // +Y down = clockwise angle

        // Build text box centered at textPosition, same size as textBoxSize
        double w = textBoxSize.Width();
        double h = textBoxSize.Height();
        cand.textBox.left = cand.textPosition.x - w * 0.5;
        cand.textBox.right = cand.textPosition.x + w * 0.5;
        cand.textBox.top = cand.textPosition.y - h * 0.5;
        cand.textBox.bottom = cand.textPosition.y + h * 0.5;

        // Build leader polyline: anchor -> shoulder -> textPosition
        // Use the provided shoulderLength (from LeaderGeometry) instead of half distance
        cand.leaderPolyline.clear();
        cand.leaderPolyline.push_back (anchor);
        cand.leaderPolyline.push_back ({
            anchor.x + shoulderLength * std::cos (rad),
            anchor.y + shoulderLength * std::sin (rad)
        });
        cand.leaderPolyline.push_back (cand.textPosition);
    }

    return cand;
}

// -----------------------------------------------------------------------------
// CandidateGenerator::PassesViewBoundary (Filter Step 1)
// -----------------------------------------------------------------------------

bool CandidateGenerator::PassesViewBoundary (const Candidate& cand, const Rect& viewBoundary) {
    if (!cand.valid) return false;
    // Text box must be fully inside view boundary (with epsilon)
    return !(cand.textBox.left < viewBoundary.left - 1e-4 ||
             cand.textBox.right > viewBoundary.right + 1e-4 ||
             cand.textBox.top < viewBoundary.top - 1e-4 ||
             cand.textBox.bottom > viewBoundary.bottom + 1e-4);
}

// -----------------------------------------------------------------------------
// CandidateGenerator::PassesForbiddenGeometry (Filter Step 2)
// -----------------------------------------------------------------------------

bool CandidateGenerator::PassesForbiddenGeometry (const Candidate& cand, const std::vector<Rect>& obstacles) {
    if (!cand.valid) return false;
    for (const Rect& obs : obstacles) {
        if (cand.textBox.Intersects (obs)) return false;
    }
    return true;
}

// -----------------------------------------------------------------------------
// CandidateGenerator::PassesObstacleLines (Filter Step 3) - Phase 4
// -----------------------------------------------------------------------------

bool CandidateGenerator::PassesObstacleLines (const Candidate& cand,
                                  const std::vector<std::vector<Point2D>>& obstaclePolylines) {
    if (!cand.valid) return false;
    // Check each segment of leader polyline against each obstacle polyline
    for (size_t i = 1; i < cand.leaderPolyline.size(); ++i) {
        const Point2D& p1 = cand.leaderPolyline[i-1];
        const Point2D& p2 = cand.leaderPolyline[i];
        for (const auto& obsPoly : obstaclePolylines) {
            for (size_t j = 1; j < obsPoly.size(); ++j) {
                if (SegmentsIntersect (p1, p2, obsPoly[j-1], obsPoly[j])) {
                    return false;
                }
            }
        }
    }
    return true;
}

// -----------------------------------------------------------------------------
// CandidateGenerator::GenerateForLeader
// -----------------------------------------------------------------------------

std::vector<Candidate> CandidateGenerator::GenerateForLeader (
    const Point2D& anchor,
    const LeaderLayout& currentLayout,
    const std::vector<Rect>& obstacles,
    const Rect& viewBoundary
) {
    std::vector<Candidate> candidates;
    candidates.reserve (kRawCandidates + 1 + 8); // 24 radial + 1 fallback + 8 local

    // Text box size from current layout (preserve dimensions)
    Rect textBoxSize = currentLayout.textBox;
    textBoxSize.Normalize();

    // Shoulder length from the original geometry (stored in leaderPolyline)
    // The shoulder is the second point in the polyline (index 1)
    double shoulderLength = 0.0;
    if (currentLayout.leaderPolyline.size() >= 2) {
        Point2D shoulderVec = currentLayout.leaderPolyline[1] - currentLayout.leaderPolyline[0];
        shoulderLength = std::sqrt(shoulderVec.x * shoulderVec.x + shoulderVec.y * shoulderVec.y);
    }

    // 1. Generate 24 raw candidates (8 angles × 3 distances)
    for (int ai = 0; ai < kNumAngles; ++ai) {
        double angle = kCandidateAnglesDeg[ai];
        for (int di = 0; di < kNumDistances; ++di) {
            double dist = kCandidateDistancesMm[di];
            Candidate cand = MakeCandidateInternal (anchor, angle, dist, textBoxSize, false, shoulderLength);
            candidates.push_back (std::move (cand));
        }
    }

    // 2. Generate guaranteed fallback candidate (current position)
    Candidate fallback;
    fallback.textPosition = currentLayout.textPosition;
    fallback.textBox = currentLayout.textBox;
    fallback.leaderPolyline = currentLayout.leaderPolyline;
    fallback.leaderAngle = currentLayout.leaderAngle;
    fallback.valid = true; // Unconditionally valid (bypasses filters)
    candidates.push_back (std::move (fallback));

    // 3. Generate LOCAL candidates: small perturbations around fallback
    // This allows the solver to nudge labels by 1-2mm to resolve tiny overlaps
    // instead of jumping to faraway 5/10/20mm grid positions.
    const std::array<Point2D, 8> kLocalDirs = {{
        {-2.0,  0.0}, { 2.0,  0.0},  // left, right
        { 0.0, -2.0}, { 0.0,  2.0},  // up, down (Y down = positive)
        {-1.4, -1.4}, { 1.4, -1.4},  // up-left, up-right
        {-1.4,  1.4}, { 1.4,  1.4},  // down-left, down-right
    }};
    for (const Point2D& dir : kLocalDirs) {
        Candidate local;
        local.textPosition = {
            currentLayout.textPosition.x + dir.x,
            currentLayout.textPosition.y + dir.y
        };
        double w = textBoxSize.Width();
        double h = textBoxSize.Height();
        local.textBox.left = local.textPosition.x - w * 0.5;
        local.textBox.right = local.textPosition.x + w * 0.5;
        local.textBox.top = local.textPosition.y - h * 0.5;
        local.textBox.bottom = local.textPosition.y + h * 0.5;
        local.leaderPolyline = currentLayout.leaderPolyline;
        // Update only the end point of leader (text position)
        if (local.leaderPolyline.size() >= 3) {
            local.leaderPolyline[2] = local.textPosition;
        }
        local.angle = currentLayout.leaderAngle;
        local.valid = true;
        candidates.push_back (std::move (local));
    }

    // 3. Filter candidates (mark invalid instead of removing, fallback bypasses filters)
    // Fallback is at index kRawCandidates (24), local candidates follow
    std::vector<std::vector<Point2D>> obstaclePolylines; // Phase 4 placeholder
    const size_t fallbackIndex = kRawCandidates; // 24

    for (size_t i = 0; i < candidates.size(); ++i) {
        Candidate& cand = candidates[i];
        bool isFallback = (i == fallbackIndex);

        if (isFallback) {
            // Fallback always passes, already valid = true
            continue;
        }

        if (!PassesViewBoundary (cand, viewBoundary) ||
            !PassesForbiddenGeometry (cand, obstacles) ||
            !PassesObstacleLines (cand, obstaclePolylines)) {
            cand.valid = false;
        }
    }

    return candidates;
}

// -----------------------------------------------------------------------------
// CandidateGenerator::GenerateForDimension
// -----------------------------------------------------------------------------

std::vector<Candidate> CandidateGenerator::GenerateForDimension (
    const Point2D& measurePoint,
    const DimensionLayout& currentLayout,
    const Point2D& direction,
    const std::vector<Rect>& obstacles,
    const Rect& viewBoundary
) {
    std::vector<Candidate> candidates;
    candidates.reserve (kRawCandidates + 1 + 5); // 24 radial + 1 fallback + 5 perp offsets

    Rect textBoxSize = currentLayout.textBox;
    textBoxSize.Normalize();

    // 1. Generate 24 raw candidates (8 angles × 3 distances)
    // For dimensions: candidates are radial positions around the measure point
    for (int ai = 0; ai < kNumAngles; ++ai) {
        double angle = kCandidateAnglesDeg[ai];
        for (int di = 0; di < kNumDistances; ++di) {
            double dist = kCandidateDistancesMm[di];
            Candidate cand = MakeCandidateInternal (measurePoint, angle, dist, textBoxSize, false, 0.0);
            // For dimensions, offset is the perpendicular distance from the dimension line
            // We compute it as the projection onto the perpendicular direction
            Point2D perpDir = {-direction.y, direction.x};
            double offset = cand.textPosition.x * perpDir.x + cand.textPosition.y * perpDir.y
                          - measurePoint.x * perpDir.x - measurePoint.y * perpDir.y;
            cand.offset = offset;
            candidates.push_back (std::move (cand));
        }
    }

    // 2. Additional candidates: perpendicular offsets along dimension line
    // This allows text to slide along the dimension line (parallel offsets)
    constexpr double kPerpOffsets[5] = {-20.0, -10.0, 0.0, 10.0, 20.0};
    for (double off : kPerpOffsets) {
        Candidate cand;
        Point2D perpDir = {-direction.y, direction.x};
        cand.textPosition = {
            currentLayout.textPosition.x + perpDir.x * off,
            currentLayout.textPosition.y + perpDir.y * off
        };
        cand.offset = off; // This is the parallel offset along the dimension line
        cand.angle = 0.0;

        double w = textBoxSize.Width();
        double h = textBoxSize.Height();
        cand.textBox.left = cand.textPosition.x - w * 0.5;
        cand.textBox.right = cand.textPosition.x + w * 0.5;
        cand.textBox.top = cand.textPosition.y - h * 0.5;
        cand.textBox.bottom = cand.textPosition.y + h * 0.5;
        cand.leaderPolyline = { cand.textPosition };

        // Quick boundary check
        if (cand.textBox.left < viewBoundary.left || cand.textBox.right > viewBoundary.right ||
            cand.textBox.top < viewBoundary.top || cand.textBox.bottom > viewBoundary.bottom) {
            cand.valid = false;
        } else {
            cand.valid = true;
        }
        candidates.push_back (std::move (cand));
    }

    // 3. Guaranteed fallback candidate (current position)
    Candidate fallback;
    fallback.textPosition = currentLayout.textPosition;
    fallback.textBox = currentLayout.textBox;
    fallback.offset = currentLayout.offset;
    fallback.valid = true;
    candidates.push_back (std::move (fallback));

    // 4. Filter candidates (fallback bypasses filters)
    std::vector<std::vector<Point2D>> obstaclePolylines; // Phase 4 placeholder

    for (size_t i = 0; i < candidates.size(); ++i) {
        Candidate& cand = candidates[i];
        bool isFallback = (i == candidates.size() - 1);

        if (isFallback) {
            continue;
        }

        if (!PassesViewBoundary (cand, viewBoundary) ||
            !PassesForbiddenGeometry (cand, obstacles) ||
            !PassesObstacleLines (cand, obstaclePolylines)) {
            cand.valid = false;
        }
    }

    return candidates;
}

} // namespace LayoutOptimizer