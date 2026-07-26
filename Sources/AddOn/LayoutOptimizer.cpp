//------------ kuvbur 2026 ------------
// LayoutOptimizer Core - Phase 5: Greedy Solver & Fallback
// Pure C++17, no ACAPI dependencies

#include "LayoutOptimizer.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>
#include <vector>

// Define M_PI if not available (MSVC doesn't define it by default)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace LayoutOptimizer {

// -----------------------------------------------------------------------------
// Geometry helpers (not inline in header)
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Candidate cost evaluation
// -----------------------------------------------------------------------------
static LayoutCost EvaluateCandidateCost(const Candidate& cand,
                                        const DimensionElement& elem,
                                        const std::vector<Rect>& forbiddenRects,
                                        const std::vector<std::pair<Point2D, Point2D>>& obstacleLines,
                                        const Point2D& initialPos) {
    LayoutCost cost{};

    // Text-text collisions (with other elements' current positions)
    for (const auto& fr : forbiddenRects) {
        if (cand.textBox.Intersects(fr))
            ++cost.textCollision;
    }

    // Text-geometry collisions (forbidden rects)
    for (const auto& fr : forbiddenRects) {
        if (cand.textBox.Intersects(fr))
            ++cost.geometryCollision;
    }

    // Leader line crossings (dimensions have simple point polyline)
    for (size_t i = 1; i < cand.leaderPolyline.size(); ++i) {
        Point2D a = cand.leaderPolyline[i - 1];
        Point2D b = cand.leaderPolyline[i];
        for (const auto& obs : obstacleLines) {
            if (SegmentsIntersect(a, b, obs.first, obs.second))
                ++cost.lineCollision;
        }
    }

    // Movement from initial position
    double dx = cand.textPosition.x - initialPos.x;
    double dy = cand.textPosition.y - initialPos.y;
    cost.movement = std::sqrt(dx * dx + dy * dy);

    // Leader length
    for (size_t i = 1; i < cand.leaderPolyline.size(); ++i) {
        double ldx = cand.leaderPolyline[i].x - cand.leaderPolyline[i - 1].x;
        double ldy = cand.leaderPolyline[i].y - cand.leaderPolyline[i - 1].y;
        cost.leaderLength += std::sqrt(ldx * ldx + ldy * ldy);
    }

    // Alignment penalty (dimensions don't have leader angles, penalty = 0)
    cost.alignmentPenalty = 0.0;

    return cost;
}

// Overload for LeaderElement
static LayoutCost EvaluateCandidateCost(const Candidate& cand,
                                        const LeaderElement& elem,
                                        const std::vector<Rect>& forbiddenRects,
                                        const std::vector<std::pair<Point2D, Point2D>>& obstacleLines,
                                        const Point2D& initialPos) {
    LayoutCost cost{};

    // Text-text collisions (with other elements' current positions)
    for (const auto& fr : forbiddenRects) {
        if (cand.textBox.Intersects(fr))
            ++cost.textCollision;
    }

    // Text-geometry collisions (forbidden rects)
    for (const auto& fr : forbiddenRects) {
        if (cand.textBox.Intersects(fr))
            ++cost.geometryCollision;
    }

    // Leader line crossings
    for (size_t i = 1; i < cand.leaderPolyline.size(); ++i) {
        Point2D a = cand.leaderPolyline[i - 1];
        Point2D b = cand.leaderPolyline[i];
        for (const auto& obs : obstacleLines) {
            if (SegmentsIntersect(a, b, obs.first, obs.second))
                ++cost.lineCollision;
        }
    }

    // Movement from initial position
    double dx = cand.textPosition.x - initialPos.x;
    double dy = cand.textPosition.y - initialPos.y;
    cost.movement = std::sqrt(dx * dx + dy * dy);

    // Leader length
    for (size_t i = 1; i < cand.leaderPolyline.size(); ++i) {
        double ldx = cand.leaderPolyline[i].x - cand.leaderPolyline[i - 1].x;
        double ldy = cand.leaderPolyline[i].y - cand.leaderPolyline[i - 1].y;
        cost.leaderLength += std::sqrt(ldx * ldx + ldy * ldy);
    }

    // Alignment penalty (deviation from 8 compass directions)
    if (cand.leaderPolyline.size() >= 2) {
        Point2D dir = cand.leaderPolyline[1] - cand.leaderPolyline[0];
        double len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (len > 1e-6) {
            dir.x /= len; dir.y /= len;
            double bestDot = -1.0;
            constexpr double angles[8] = {0, 45, 90, 135, 180, 225, 270, 315};
            for (double ang : angles) {
                double rad = ang * M_PI / 180.0;
                double dot = dir.x * std::cos(rad) + dir.y * std::sin(rad);
                if (dot > bestDot) bestDot = dot;
            }
            cost.alignmentPenalty = 1.0 - bestDot; // 0 = perfect alignment
        }
    }

    return cost;
}

// -----------------------------------------------------------------------------
// Candidate generation for Leaders
// -----------------------------------------------------------------------------
static std::vector<Candidate> GenerateLeaderCandidates(const LeaderElement& elem,
                                                       const Rect& viewBounds,
                                                       const std::vector<Rect>& forbiddenRects,
                                                       const std::vector<std::pair<Point2D, Point2D>>& obstacleLines) {
    std::vector<Candidate> candidates;

    constexpr double kAngles[8] = {0, 45, 90, 135, 180, 225, 270, 315};
    constexpr double kDistances[3] = {5.0, 10.0, 20.0};

    for (double angDeg : kAngles) {
        double rad = angDeg * M_PI / 180.0;
        double dx = std::cos(rad);
        double dy = std::sin(rad);

        for (double dist : kDistances) {
            Candidate cand;
            cand.textPosition = { elem.geo.anchorPoint.x + dx * dist,
                                  elem.geo.anchorPoint.y + dy * dist };
            cand.angle = angDeg;

            // Build leader polyline: anchor -> shoulder -> text
            cand.leaderPolyline.clear();
            cand.leaderPolyline.push_back(elem.geo.anchorPoint);
            Point2D shoulder = { elem.geo.anchorPoint.x + dx * elem.geo.shoulderLength,
                                 elem.geo.anchorPoint.y + dy * elem.geo.shoulderLength };
            cand.leaderPolyline.push_back(shoulder);
            cand.leaderPolyline.push_back(cand.textPosition);

            // Estimate text box (use current box size as proxy)
            double w = elem.layout.textBox.Width();
            double h = elem.layout.textBox.Height();
            cand.textBox = Rect::FromCenter(cand.textPosition, w, h);

            // Quick boundary check
            if (cand.textBox.left < viewBounds.left || cand.textBox.right > viewBounds.right ||
                cand.textBox.top < viewBounds.top || cand.textBox.bottom > viewBounds.bottom) {
                cand.valid = false;
            }

            candidates.push_back(cand);
        }
    }

    // Guaranteed fallback: current position
    Candidate fallback;
    fallback.textPosition = elem.layout.textPosition;
    fallback.textBox = elem.layout.textBox;
    fallback.leaderPolyline = elem.layout.leaderPolyline;
    fallback.angle = elem.layout.leaderAngle;
    fallback.valid = true; // always valid
    candidates.push_back(fallback);

    return candidates;
}

// -----------------------------------------------------------------------------
// Candidate generation for Dimensions
// -----------------------------------------------------------------------------
static std::vector<Candidate> GenerateDimensionCandidates(const DimensionElement& elem,
                                                          const Rect& viewBounds,
                                                          const std::vector<Rect>& forbiddenRects,
                                                          const std::vector<std::pair<Point2D, Point2D>>& obstacleLines) {
    std::vector<Candidate> candidates;

    // Dimensions: offset along perpendicular to dimension line
    constexpr double kOffsets[5] = {-20.0, -10.0, 0.0, 10.0, 20.0};

    Point2D perp = { -elem.geo.direction.y, elem.geo.direction.x }; // rotate 90° CCW

    for (double off : kOffsets) {
        Candidate cand;
        cand.textPosition = { elem.layout.textPosition.x + perp.x * off,
                              elem.layout.textPosition.y + perp.y * off };
        cand.offset = off;
        cand.angle = 0.0; // dimensions don't have leader angle

        // Text box
        double w = elem.layout.textBox.Width();
        double h = elem.layout.textBox.Height();
        cand.textBox = Rect::FromCenter(cand.textPosition, w, h);

        // Boundary check
        if (cand.textBox.left < viewBounds.left || cand.textBox.right > viewBounds.right ||
            cand.textBox.top < viewBounds.top || cand.textBox.bottom > viewBounds.bottom) {
            cand.valid = false;
        }

        // No leader polyline for dimensions (or just a point)
        cand.leaderPolyline = { cand.textPosition };

        candidates.push_back(cand);
    }

    // Guaranteed fallback: current position
    Candidate fallback;
    fallback.textPosition = elem.layout.textPosition;
    fallback.textBox = elem.layout.textBox;
    fallback.offset = elem.layout.offset;
    fallback.valid = true;
    candidates.push_back(fallback);

    return candidates;
}

// -----------------------------------------------------------------------------
// Greedy solver for Leaders
// -----------------------------------------------------------------------------
static void SolveLeadersGreedy(std::vector<LeaderElement>& leaders,
                               const std::vector<Rect>& forbiddenRects,
                               const std::vector<std::pair<Point2D, Point2D>>& obstacleLines,
                               const Rect& viewBounds) {
    // Build placement priority
    struct PriorityItem {
        size_t index;
        PlacementPriority priority;
        bool operator<(const PriorityItem& other) const {
            return priority < other.priority;
        }
    };

    std::vector<PriorityItem> order;
    order.reserve(leaders.size());

    for (size_t i = 0; i < leaders.size(); ++i) {
        if (leaders[i].locked) continue;

        PlacementPriority pr;
        pr.typeRank = 1; // Leaders after Dimensions
        // Count locked neighbors (simplified: elements whose textBox intersects this one's candidates)
        int lockedNeighbors = 0;
        for (const auto& other : leaders) {
            if (other.locked && other.layout.textBox.Intersects(leaders[i].layout.textBox))
                ++lockedNeighbors;
        }
        pr.lockedNeighborCount = -lockedNeighbors; // more locked neighbors = solve first
        pr.invalidCandidateCount = 0; // will compute after candidate gen
        pr.id = leaders[i].id;
        order.push_back({i, pr});
    }

    // Generate candidates first to compute invalidCandidateCount
    std::vector<std::vector<Candidate>> allCandidates(leaders.size());
    for (auto& item : order) {
        allCandidates[item.index] = GenerateLeaderCandidates(leaders[item.index], viewBounds, forbiddenRects, obstacleLines);
        item.priority.invalidCandidateCount = 0;
        for (const auto& c : allCandidates[item.index]) {
            if (!c.valid) ++item.priority.invalidCandidateCount;
        }
    }

    std::sort(order.begin(), order.end());

    // Greedy placement
    std::vector<Rect> placedBoxes = forbiddenRects; // start with static obstacles
    std::vector<std::pair<Point2D, Point2D>> placedLines = obstacleLines;

    for (const auto& item : order) {
        LeaderElement& elem = leaders[item.index];
        const auto& cands = allCandidates[item.index];

        LayoutCost bestCost;
        bool hasBest = false;
        size_t bestIdx = 0;

        for (size_t ci = 0; ci < cands.size(); ++ci) {
            if (!cands[ci].valid) continue;

            LayoutCost cost = EvaluateCandidateCost(cands[ci], elem, placedBoxes, placedLines, elem.layout.textPosition);

            if (!hasBest || cost < bestCost) {
                bestCost = cost;
                bestIdx = ci;
                hasBest = true;
            }
        }

        if (hasBest) {
            const Candidate& chosen = cands[bestIdx];
            elem.layout.textPosition = chosen.textPosition;
            elem.layout.textBox = chosen.textBox;
            elem.layout.leaderPolyline = chosen.leaderPolyline;
            elem.layout.leaderAngle = chosen.angle;

            placedBoxes.push_back(chosen.textBox);
            for (size_t i = 1; i < chosen.leaderPolyline.size(); ++i) {
                placedLines.emplace_back(chosen.leaderPolyline[i - 1], chosen.leaderPolyline[i]);
            }
        }
        // If no valid candidate (should not happen due to fallback), element keeps current position
    }
}

// -----------------------------------------------------------------------------
// Greedy solver for Dimensions
// -----------------------------------------------------------------------------
static void SolveDimensionsGreedy(std::vector<DimensionElement>& dims,
                                  const std::vector<Rect>& forbiddenRects,
                                  const std::vector<std::pair<Point2D, Point2D>>& obstacleLines,
                                  const Rect& viewBounds) {
    struct PriorityItem {
        size_t index;
        PlacementPriority priority;
        bool operator<(const PriorityItem& other) const {
            return priority < other.priority;
        }
    };

    std::vector<PriorityItem> order;
    order.reserve(dims.size());

    for (size_t i = 0; i < dims.size(); ++i) {
        if (dims[i].locked) continue;

        PlacementPriority pr;
        pr.typeRank = 0; // Dimensions first
        int lockedNeighbors = 0;
        for (const auto& other : dims) {
            if (other.locked && other.layout.textBox.Intersects(dims[i].layout.textBox))
                ++lockedNeighbors;
        }
        pr.lockedNeighborCount = -lockedNeighbors;
        pr.invalidCandidateCount = 0;
        pr.id = dims[i].id;
        order.push_back({i, pr});
    }

    std::vector<std::vector<Candidate>> allCandidates(dims.size());
    for (auto& item : order) {
        allCandidates[item.index] = GenerateDimensionCandidates(dims[item.index], viewBounds, forbiddenRects, obstacleLines);
        item.priority.invalidCandidateCount = 0;
        for (const auto& c : allCandidates[item.index]) {
            if (!c.valid) ++item.priority.invalidCandidateCount;
        }
    }

    std::sort(order.begin(), order.end());

    std::vector<Rect> placedBoxes = forbiddenRects;
    std::vector<std::pair<Point2D, Point2D>> placedLines = obstacleLines;

    for (const auto& item : order) {
        DimensionElement& elem = dims[item.index];
        const auto& cands = allCandidates[item.index];

        LayoutCost bestCost;
        bool hasBest = false;
        size_t bestIdx = 0;

        for (size_t ci = 0; ci < cands.size(); ++ci) {
            if (!cands[ci].valid) continue;

            LayoutCost cost = EvaluateCandidateCost(cands[ci], elem, placedBoxes, placedLines, elem.layout.textPosition);

            if (!hasBest || cost < bestCost) {
                bestCost = cost;
                bestIdx = ci;
                hasBest = true;
            }
        }

        if (hasBest) {
            const Candidate& chosen = cands[bestIdx];
            elem.layout.textPosition = chosen.textPosition;
            elem.layout.textBox = chosen.textBox;
            elem.layout.offset = chosen.offset;

            placedBoxes.push_back(chosen.textBox);
        }
    }
}
} // namespace LayoutOptimizer