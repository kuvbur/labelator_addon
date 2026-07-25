//------------ kuvbur 2026 ------------
// LayoutOptimizer Core - Phase 3: Candidate Generation & Filtering
// Pure C++17, no ACAPI dependencies

#include "CandidateGenerator.hpp"
#include "LayoutOptimizer.hpp"
#include <cmath>
#include <algorithm>

namespace LayoutOptimizer {

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

static inline double DegToRad (double deg) {
    return deg * 3.14159265358979323846 / 180.0;
}

Candidate CandidateGenerator::MakeCandidateInternal (
    const Point2D& anchor,
    double angleDeg,
    double distanceMm,
    const Rect& textBoxSize,
    bool isFallback
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
        cand.leaderPolyline.clear();
        cand.leaderPolyline.push_back (anchor);
        // For simplicity, shoulder at half distance
        double shoulderDist = distanceMm * 0.5;
        cand.leaderPolyline.push_back ({
            anchor.x + shoulderDist * std::cos (rad),
            anchor.y + shoulderDist * std::sin (rad)
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
// CandidateGenerator implementation
// -----------------------------------------------------------------------------

std::vector<Candidate> CandidateGenerator::GenerateForLeader (
    const Point2D& anchor,
    const LeaderLayout& currentLayout,
    const std::vector<Rect>& obstacles,
    const Rect& viewBoundary
) {
    std::vector<Candidate> candidates;
    candidates.reserve (kRawCandidates + 1);

    // Text box size from current layout (preserve dimensions)
    Rect textBoxSize = currentLayout.textBox;
    textBoxSize.Normalize();

    // 1. Generate 24 raw candidates (8 angles × 3 distances)
    for (int ai = 0; ai < kNumAngles; ++ai) {
        double angle = kCandidateAnglesDeg[ai];
        for (int di = 0; di < kNumDistances; ++di) {
            double dist = kCandidateDistancesMm[di];
            Candidate cand = MakeCandidateInternal (anchor, angle, dist, textBoxSize, false);
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

    // 3. Filter candidates (mark invalid instead of removing, fallback bypasses filters)
    std::vector<std::vector<Point2D>> obstaclePolylines; // Phase 4 placeholder

    for (size_t i = 0; i < candidates.size(); ++i) {
        Candidate& cand = candidates[i];
        bool isFallback = (i == candidates.size() - 1);

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

std::vector<Candidate> CandidateGenerator::GenerateForDimension (
    const Point2D& measurePoint,
    const DimensionLayout& currentLayout,
    const std::vector<Rect>& obstacles,
    const Rect& viewBoundary
) {
    std::vector<Candidate> candidates;
    candidates.reserve (kRawCandidates + 1);

    Rect textBoxSize = currentLayout.textBox;
    textBoxSize.Normalize();

    // 1. Generate 24 raw candidates (same grid)
    for (int ai = 0; ai < kNumAngles; ++ai) {
        double angle = kCandidateAnglesDeg[ai];
        for (int di = 0; di < kNumDistances; ++di) {
            double dist = kCandidateDistancesMm[di];
            Candidate cand = MakeCandidateInternal (measurePoint, angle, dist, textBoxSize, false);
            // For dimensions, offset is perpendicular distance from measure line
            cand.offset = dist;
            candidates.push_back (std::move (cand));
        }
    }

    // 2. Guaranteed fallback candidate
    Candidate fallback;
    fallback.textPosition = currentLayout.textPosition;
    fallback.textBox = currentLayout.textBox;
    fallback.offset = currentLayout.offset;
    fallback.valid = true;
    candidates.push_back (std::move (fallback));

    // 3. Filter (fallback bypasses, mark invalid instead of filtering)
    std::vector<std::vector<Point2D>> obstaclePolylines; // Phase 4

    for (size_t i = 0; i < candidates.size(); ++i) {
        Candidate& cand = candidates[i];
        bool isFallback = (i == candidates.size() - 1);

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

} // namespace LayoutOptimizer