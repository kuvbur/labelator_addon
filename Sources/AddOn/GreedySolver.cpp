//------------ kuvbur 2026 ------------
// LayoutOptimizer Core - Phase 5: Greedy Solver & Fallback
// Pure C++17, no ACAPI dependencies

#include "LayoutOptimizer.hpp"
#include <algorithm>
#include <vector>
#include <cmath>
#include <tuple>

namespace LayoutOptimizer {

// -----------------------------------------------------------------------------
// Helper: Count text-text collisions (AABB intersection)
// -----------------------------------------------------------------------------
int CountTextCollisions(const Candidate& a, const Candidate& b) {
    if (!a.valid || !b.valid) return 0;
    return a.textBox.Intersects(b.textBox) ? 1 : 0;
}

// -----------------------------------------------------------------------------
// Helper: Count text-geometry collisions
// -----------------------------------------------------------------------------
int CountGeometryCollisions(const Candidate& cand,
                            const std::vector<Rect>& forbiddenGeometry) {
    if (!cand.valid) return 0;
    int count = 0;
    for (const Rect& obs : forbiddenGeometry) {
        if (cand.textBox.Intersects(obs)) ++count;
    }
    return count;
}

// -----------------------------------------------------------------------------
// Helper: Count leader polyline vs obstacle line collisions
// -----------------------------------------------------------------------------
int CountLineCollisions(const Candidate& cand,
                        const std::vector<Candidate>& placedCandidates,
                        const std::vector<std::vector<Point2D>>& obstaclePolylines) {
    if (!cand.valid) return 0;
    int count = 0;

    // Check against already-placed candidates' leader polylines
    for (const Candidate& placed : placedCandidates) {
        if (!placed.valid) continue;
        for (size_t i = 1; i < cand.leaderPolyline.size(); ++i) {
            const Point2D& p1 = cand.leaderPolyline[i-1];
            const Point2D& p2 = cand.leaderPolyline[i];
            for (size_t j = 1; j < placed.leaderPolyline.size(); ++j) {
                if (SegmentsIntersect(p1, p2, placed.leaderPolyline[j-1], placed.leaderPolyline[j])) {
                    ++count;
                }
            }
        }
    }

    // Check against fixed obstacle polylines
    for (size_t i = 1; i < cand.leaderPolyline.size(); ++i) {
        const Point2D& p1 = cand.leaderPolyline[i-1];
        const Point2D& p2 = cand.leaderPolyline[i];
        for (const auto& obsPoly : obstaclePolylines) {
            for (size_t j = 1; j < obsPoly.size(); ++j) {
                if (SegmentsIntersect(p1, p2, obsPoly[j-1], obsPoly[j])) {
                    ++count;
                }
            }
        }
    }
    return count;
}

// -----------------------------------------------------------------------------
// Helper: Movement distance from initial position
// -----------------------------------------------------------------------------
double ComputeMovement(const Candidate& cand, const Point2D& initialPos) {
    double dx = cand.textPosition.x - initialPos.x;
    double dy = cand.textPosition.y - initialPos.y;
    return std::sqrt(dx * dx + dy * dy);
}

// -----------------------------------------------------------------------------
// Helper: Leader polyline length
// -----------------------------------------------------------------------------
double ComputeLeaderLength(const Candidate& cand) {
    double len = 0.0;
    for (size_t i = 1; i < cand.leaderPolyline.size(); ++i) {
        double dx = cand.leaderPolyline[i].x - cand.leaderPolyline[i-1].x;
        double dy = cand.leaderPolyline[i].y - cand.leaderPolyline[i-1].y;
        len += std::sqrt(dx * dx + dy * dy);
    }
    return len;
}

// -----------------------------------------------------------------------------
// Helper: Alignment penalty (deviation from 0°, 90°, 180°, 270°)
// -----------------------------------------------------------------------------
double ComputeAlignmentPenalty(const Candidate& cand) {
    if (cand.leaderPolyline.size() < 2) return 0.0;
    
    // Compute primary leader angle from anchor to text
    const Point2D& anchor = cand.leaderPolyline.front();
    const Point2D& textPos = cand.textPosition;
    double dx = textPos.x - anchor.x;
    double dy = textPos.y - anchor.y;
    
    if (dx == 0.0 && dy == 0.0) return 0.0;
    
    double angleRad = std::atan2(dy, dx);
    double angleDeg = angleRad * 180.0 / 3.14159265358979323846;
    if (angleDeg < 0) angleDeg += 360.0;
    
    // Find nearest cardinal angle (0, 90, 180, 270)
    double minDev = 360.0;
    for (double cardinal : {0.0, 90.0, 180.0, 270.0}) {
        double dev = std::abs(angleDeg - cardinal);
        dev = std::min(dev, 360.0 - dev);
        minDev = std::min(minDev, dev);
    }
    return minDev;
}

// -----------------------------------------------------------------------------
// Main candidate evaluation
// -----------------------------------------------------------------------------
LayoutCost EvaluateCandidate(const Candidate& cand,
                             const ElementInput& element,
                             const std::vector<Candidate>& placedCandidates,
                             const std::vector<Rect>& forbiddenGeometry,
                             const std::vector<std::vector<Point2D>>& obstaclePolylines) {
    LayoutCost cost;
    
    if (!cand.valid) {
        // Invalid candidate gets max hard collisions to deprioritize
        cost.textCollision = kInvalidCandidatePenalty;
        cost.geometryCollision = kInvalidCandidatePenalty;
        cost.lineCollision = kInvalidCandidatePenalty;
        return cost;
    }
    
    // Hard 1: Text-text collisions with already placed elements
    for (const Candidate& placed : placedCandidates) {
        cost.textCollision += CountTextCollisions(cand, placed);
    }
    
    // Hard 2: Text-geometry (forbidden) collisions
    cost.geometryCollision = CountGeometryCollisions(cand, forbiddenGeometry);
    
    // Hard 3: Leader polyline crossings
    cost.lineCollision = CountLineCollisions(cand, placedCandidates, obstaclePolylines);
    
    // Soft 1: Movement from initial position
    cost.movement = ComputeMovement(cand, element.initialTextPosition);
    
    // Soft 2: Leader length
    cost.leaderLength = ComputeLeaderLength(cand);
    
    // Soft 3: Alignment penalty
    cost.alignmentPenalty = ComputeAlignmentPenalty(cand);
    
    return cost;
}

// -----------------------------------------------------------------------------
// Greedy Solve — Main entry point
// -----------------------------------------------------------------------------
SolveResult Solve(const std::vector<ElementInput>& elements,
                  const std::vector<Rect>& forbiddenGeometry,
                  const std::vector<std::vector<Point2D>>& obstaclePolylines) {
    SolveResult result;
    result.log.elementCount = elements.size();
    
    // Build placement priority order
    struct PrioritizedElement {
        PlacementPriority priority;
        size_t originalIndex;
    };
    
    std::vector<PrioritizedElement> prioritized;
    prioritized.reserve(elements.size());
    
    for (size_t i = 0; i < elements.size(); ++i) {
        const auto& el = elements[i];
        
        // Count invalid candidates
                int invalidCount = 0;
                for (const auto& c : el.candidates) {
                    if (!c.valid) ++invalidCount;
                }

                // Count locked neighbors that could potentially overlap (bbox overlap check)
                int lockedNeighbors = 0;
                for (const auto& other : elements) {
                    if (other.locked && other.id != el.id) {
                        // Check if element's initial text box could overlap with locked element's layout
                        // Use candidate text boxes for overlap detection
                        for (const auto& cand : el.candidates) {
                            for (const auto& otherCand : other.candidates) {
                                if (cand.textBox.Intersects(otherCand.textBox)) {
                                    lockedNeighbors++;
                                    break;
                                }
                            }
                            if (lockedNeighbors > 0) break; // Count each locked neighbor at most once per element
                        }
                    }
                }
        
        PlacementPriority pri;
        pri.typeRank = (el.type == ElementType::Dimension) ? 0 : 1;
        pri.lockedNeighborCount = -lockedNeighbors;  // Negate so more locked = smaller = sorted first
        pri.invalidCandidateCount = -invalidCount;    // Negate so more constrained = sorted first
        pri.id = el.id;
        
        prioritized.push_back({pri, i});
    }
    
    // Sort by PlacementPriority (ascending = highest priority first)
    std::sort(prioritized.begin(), prioritized.end(),
              [](const PrioritizedElement& a, const PrioritizedElement& b) {
                  return a.priority < b.priority;
              });
    
    // Greedy placement: for each element in priority order, pick best valid candidate
        std::vector<Candidate> placedCandidates;
        placedCandidates.reserve(elements.size());
        result.selectedCandidates.resize(elements.size());

        // --- Layout Stability pre-check (Phase 6.5) ---
        // Если ВСЕ fallback-позиции (последний кандидат каждого элемента) не имеют
        // пересечений друг с другом (текст, геометрия, линии), используем их без изменений.
        // Это гарантирует Layout Stability: элементы без конфликтов не двигаются,
        // даже если generated-кандидаты имеют меньший leaderLength/alignmentPenalty.
        bool allFallbacksStable = true;
        for (size_t i = 0; i < elements.size() && allFallbacksStable; ++i) {
            const auto& el = elements[i];
            if (el.locked) continue;
            const Candidate& fallback = el.candidates.back();
            for (size_t j = 0; j < i; ++j) {
                const auto& other = elements[j];
                if (other.locked) continue;
                const Candidate& otherFallback = other.candidates.back();
                if (CountTextCollisions(fallback, otherFallback) > 0 ||
                    CountGeometryCollisions(fallback, forbiddenGeometry) > 0 ||
                    CountLineCollisions(fallback, {otherFallback}, obstaclePolylines) > 0) {
                    allFallbacksStable = false;
                    break;
                }
            }
        }

        if (allFallbacksStable && elements.size() > 0) {
            // Все элементы стабильны — используем fallback-позиции
            for (const auto& pe : prioritized) {
                const auto& element = elements[pe.originalIndex];
                const Candidate& fallback = element.candidates.back();
                result.selectedCandidates[pe.originalIndex] = fallback;
                result.zeroCollisionSolution = true;
            }
            result.log.finalConflictCount = 0;
            result.log.totalCandidatesGenerated = 0;
            for (const auto& el : elements) {
                result.log.totalCandidatesGenerated += el.candidates.size();
            }
            return result;
        }

        // Debug: log why stability check failed
        // std::cerr << "[LayoutOptimizer] Layout stability check failed, running greedy solver\n";

        bool allZeroCollision = true;
            int totalConflicts = 0;

        for (const auto& pe : prioritized) {
            const auto& element = elements[pe.originalIndex];

            // Locked elements: use their fallback candidate (LAST in list, index = candidates.size() - 1)
            // Per AGENTS.md Section 7: fallback is added last and bypasses filters
            if (element.locked) {
                Candidate chosen = element.candidates.back();
                result.selectedCandidates[pe.originalIndex] = chosen;
                placedCandidates.push_back(chosen);
                continue;
            }

            // Find best candidate by LayoutCost (unified logic for valid + fallback)
            const Candidate* bestCand = nullptr;
            LayoutCost bestCost;
            bool hasBest = false;

            for (const auto& cand : element.candidates) {
                LayoutCost cost = EvaluateCandidate(cand, element, placedCandidates, forbiddenGeometry, obstaclePolylines);

                if (!hasBest || cost < bestCost) {
                    bestCost = cost;
                    bestCand = &cand;
                    hasBest = true;
                }
            }

            // Safety: if no candidates at all (shouldn't happen per AGENTS.md Section 7),
            // create a minimal fallback candidate at initial position
            if (!bestCand) {
                Candidate fallback;
                fallback.textPosition = element.initialTextPosition;
                fallback.textBox = {element.initialTextPosition.x - kFallbackBoxSizeMm, element.initialTextPosition.y - kFallbackBoxSizeMm,
                                   element.initialTextPosition.x + kFallbackBoxSizeMm, element.initialTextPosition.y + kFallbackBoxSizeMm};
                fallback.leaderPolyline = {element.initialTextPosition, element.initialTextPosition};
                fallback.valid = true;
                totalConflicts += 1;
                result.zeroCollisionSolution = false;
                result.selectedCandidates[pe.originalIndex] = fallback;
                placedCandidates.push_back(fallback);
                continue;
            }

            // Track if zero-collision solution and accumulate actual conflict count
            if (bestCand && (bestCost.textCollision > 0 || bestCost.geometryCollision > 0 || bestCost.lineCollision > 0)) {
                allZeroCollision = false;
                totalConflicts += bestCost.textCollision + bestCost.geometryCollision + bestCost.lineCollision;
            }

            Candidate chosen = *bestCand;
            result.selectedCandidates[pe.originalIndex] = chosen;
            placedCandidates.push_back(chosen);
        }

        result.log.finalConflictCount = totalConflicts;
        result.zeroCollisionSolution = allZeroCollision;
    result.log.totalCandidatesGenerated = 0;
    for (const auto& el : elements) {
        result.log.totalCandidatesGenerated += el.candidates.size();
    }
    
    // Fill OptimizationLog fields per AGENTS.md Section 12
    result.log.rejectedCandidates = 0;
    for (const auto& el : elements) {
        for (const auto& c : el.candidates) {
            if (!c.valid) ++result.log.rejectedCandidates;
        }
    }
    result.log.executionTimeMs = 0.0; // Placeholder - can add chrono timer later
    
    return result;
}

} // namespace LayoutOptimizer