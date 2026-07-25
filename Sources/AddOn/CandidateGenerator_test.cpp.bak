// Phase 3 Candidate Generation — Unit Tests
// Tests per AGENTS.md Section 12:
// - Fixed grid generation (8 angles × 3 distances = 24 candidates + fallback = 25)
// - Fallback candidate always present and valid
// - View boundary filtering
// - Forbidden geometry filtering

#include "LayoutOptimizer.hpp"
#include "CandidateGenerator.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <string>

int passed = 0;
int failed = 0;

#define ASSERT_TRUE(cond, msg) \
    do { if (cond) { ++passed; std::cout << "[PASS] " << msg << "\n"; } \
         else { ++failed; std::cout << "[FAIL] " << msg << "\n"; } } while(0)

#define ASSERT_EQ(a, b, msg) \
    do { if ((a) == (b)) { ++passed; std::cout << "[PASS] " << msg << "\n"; } \
         else { ++failed; std::cout << "[FAIL] " << msg << " (expected " << (b) << ", got " << (a) << ")\n"; } } while(0)

#define ASSERT_NEAR(a, b, eps, msg) \
    do { if (std::abs((a) - (b)) <= (eps)) { ++passed; std::cout << "[PASS] " << msg << "\n"; } \
         else { ++failed; std::cout << "[FAIL] " << msg << " (expected " << (b) << " ± " << (eps) << ", got " << (a) << ")\n"; } } while(0)

using namespace LayoutOptimizer;

int main() {
    std::cout << "=== Phase 3 Candidate Generation Tests ===\n\n";

    // Test 1: Fixed grid constants match AGENTS.md
    {
        ASSERT_EQ(kCandidateAnglesDeg[0], 0.0, "Angle[0] = 0°");
        ASSERT_EQ(kCandidateAnglesDeg[1], 45.0, "Angle[1] = 45°");
        ASSERT_EQ(kCandidateAnglesDeg[2], 90.0, "Angle[2] = 90°");
        ASSERT_EQ(kCandidateAnglesDeg[3], 135.0, "Angle[3] = 135°");
        ASSERT_EQ(kCandidateAnglesDeg[4], 180.0, "Angle[4] = 180°");
        ASSERT_EQ(kCandidateAnglesDeg[5], 225.0, "Angle[5] = 225°");
        ASSERT_EQ(kCandidateAnglesDeg[6], 270.0, "Angle[6] = 270°");
        ASSERT_EQ(kCandidateAnglesDeg[7], 315.0, "Angle[7] = 315°");
        ASSERT_EQ(kCandidateDistancesMm[0], 5.0, "Dist[0] = 5mm");
        ASSERT_EQ(kCandidateDistancesMm[1], 10.0, "Dist[1] = 10mm");
        ASSERT_EQ(kCandidateDistancesMm[2], 20.0, "Dist[2] = 20mm");
        ASSERT_EQ(kNumAngles, 8, "8 angles");
        ASSERT_EQ(kNumDistances, 3, "3 distances");
        ASSERT_EQ(kRawCandidates, 24, "24 raw candidates");
    }

    // Test 2: GenerateForLeader produces correct count (24 + 1 fallback)
    {
        Point2D anchor{100.0, 100.0};
        LeaderLayout current;
        current.textPosition = {110.0, 90.0};
        current.textBox = {105.0, 85.0, 115.0, 95.0};
        current.leaderPolyline = {{100.0, 100.0}, {105.0, 95.0}, {110.0, 90.0}};
        current.leaderAngle = 45.0;

        std::vector<Rect> obstacles;
        Rect viewBoundary{0.0, 0.0, 1000.0, 1000.0};

        std::vector<Candidate> cands = CandidateGenerator::GenerateForLeader(
            anchor, current, obstacles, viewBoundary
        );

        ASSERT_EQ(cands.size(), 25, "Leader: 24 grid + 1 fallback = 25 candidates");
        
        // Fallback is last and always valid
        ASSERT_TRUE(cands[24].valid, "Fallback candidate is valid");
        ASSERT_NEAR(cands[24].textPosition.x, current.textPosition.x, 1e-9, "Fallback textPosition.x");
        ASSERT_NEAR(cands[24].textPosition.y, current.textPosition.y, 1e-9, "Fallback textPosition.y");
    }

    // Test 3: GenerateForDimension produces correct count
    {
        Point2D measurePoint{100.0, 100.0};
        DimensionLayout current;
        current.textPosition = {110.0, 90.0};
        current.textBox = {105.0, 85.0, 115.0, 95.0};
        current.offset = 0.0;

        std::vector<Rect> obstacles;
        Rect viewBoundary{0.0, 0.0, 1000.0, 1000.0};

        std::vector<Candidate> cands = CandidateGenerator::GenerateForDimension(
            measurePoint, current, obstacles, viewBoundary
        );

        ASSERT_EQ(cands.size(), 25, "Dimension: 24 grid + 1 fallback = 25 candidates");
        ASSERT_TRUE(cands[24].valid, "Fallback candidate is valid");
    }

    // Test 4: Grid candidates at correct positions (angle 0° = +X, clockwise)
    {
        Point2D anchor{0.0, 0.0};
        LeaderLayout current;
        current.textBox = {-5.0, -3.0, 5.0, 3.0}; // 10x6 box
        std::vector<Rect> obstacles;
        Rect viewBoundary{-100.0, -100.0, 100.0, 100.0};

        std::vector<Candidate> cands = CandidateGenerator::GenerateForLeader(
            anchor, current, obstacles, viewBoundary
        );

        // Check angle 0° (East, +X), distance 5mm
        Candidate& c0 = cands[0];
        ASSERT_NEAR(c0.textPosition.x, 5.0, 1e-9, "Angle 0°, dist 5: x = +5");
        ASSERT_NEAR(c0.textPosition.y, 0.0, 1e-9, "Angle 0°, dist 5: y = 0");

        // Angle 90° (South, +Y), distance 5mm
        Candidate& c90 = cands[6];
        ASSERT_NEAR(c90.textPosition.x, 0.0, 1e-9, "Angle 90°, dist 5: x = 0");
        ASSERT_NEAR(c90.textPosition.y, 5.0, 1e-9, "Angle 90°, dist 5: y = +5");

        // Angle 180° (West, -X), distance 10mm
        Candidate& c180 = cands[13];
        ASSERT_NEAR(c180.textPosition.x, -10.0, 1e-9, "Angle 180°, dist 10: x = -10");
        ASSERT_NEAR(c180.textPosition.y, 0.0, 1e-9, "Angle 180°, dist 10: y = 0");

        // Angle 270° (North, -Y), distance 20mm
        Candidate& c270 = cands[20];
        ASSERT_NEAR(c270.textPosition.x, 0.0, 1e-9, "Angle 270°, dist 20: x = 0");
        ASSERT_NEAR(c270.textPosition.y, -20.0, 1e-9, "Angle 270°, dist 20: y = -20");
    }

    // Test 5: View boundary filtering rejects out-of-bounds
    {
        Point2D anchor{500.0, 500.0};
        LeaderLayout current;
        current.textBox = {495.0, 495.0, 505.0, 505.0};
        std::vector<Rect> obstacles;
        Rect viewBoundary{0.0, 0.0, 100.0, 100.0}; // Small view, anchor outside

        std::vector<Candidate> cands = CandidateGenerator::GenerateForLeader(
            anchor, current, obstacles, viewBoundary
        );

        // Only fallback should be valid (all 24 grid candidates outside boundary)
        int validCount = 0;
        for (const auto& c : cands) {
            if (c.valid) ++validCount;
        }
        ASSERT_EQ(validCount, 1, "Only fallback valid when all grid candidates outside view boundary");
        ASSERT_TRUE(cands[24].valid, "Fallback always valid");
    }

    // Test 6: Forbidden geometry filtering
    {
        Point2D anchor{100.0, 100.0};
        LeaderLayout current;
        current.textBox = {95.0, 95.0, 105.0, 105.0}; // 10x10 box
        std::vector<Rect> obstacles;
        obstacles.push_back({102.0, 102.0, 112.0, 112.0}); // Obstacle overlapping some candidates
        Rect viewBoundary{0.0, 0.0, 1000.0, 1000.0};

        std::vector<Candidate> cands = CandidateGenerator::GenerateForLeader(
            anchor, current, obstacles, viewBoundary
        );

        // Fallback always valid
        ASSERT_TRUE(cands[24].valid, "Fallback valid despite obstacles");

        // Some grid candidates should be invalid due to obstacle intersection
        int invalidCount = 0;
        for (size_t i = 0; i < 24; ++i) {
            if (!cands[i].valid) ++invalidCount;
        }
        ASSERT_TRUE(invalidCount > 0, "Some candidates invalid due to forbidden geometry");
    }

    // Test 7: Fallback candidate preserves current layout exactly
    {
        Point2D anchor{100.0, 100.0};
        LeaderLayout current;
        current.textPosition = {123.45, 67.89};
        current.textBox = {120.0, 64.0, 126.0, 71.0};
        current.leaderPolyline = {{100.0, 100.0}, {111.0, 83.0}, {123.45, 67.89}};
        current.leaderAngle = 45.0;

        std::vector<Rect> obstacles;
        Rect viewBoundary{0.0, 0.0, 1000.0, 1000.0};

        std::vector<Candidate> cands = CandidateGenerator::GenerateForLeader(
            anchor, current, obstacles, viewBoundary
        );

        Candidate& fb = cands[24];
        ASSERT_NEAR(fb.textPosition.x, 123.45, 1e-9, "Fallback textPosition.x exact");
        ASSERT_NEAR(fb.textPosition.y, 67.89, 1e-9, "Fallback textPosition.y exact");
        ASSERT_NEAR(fb.textBox.left, 120.0, 1e-9, "Fallback textBox left");
        ASSERT_NEAR(fb.textBox.right, 126.0, 1e-9, "Fallback textBox right");
        ASSERT_EQ(fb.leaderPolyline.size(), 3, "Fallback leader polyline size");
        ASSERT_NEAR(fb.leaderAngle, 45.0, 1e-9, "Fallback leader angle");
    }

    // Test 8: Text box size preserved across candidates
    {
        Point2D anchor{0.0, 0.0};
        LeaderLayout current;
        current.textBox = {-10.0, -5.0, 10.0, 5.0}; // 20x10 box
        std::vector<Rect> obstacles;
        Rect viewBoundary{-1000.0, -1000.0, 1000.0, 1000.0};

        std::vector<Candidate> cands = CandidateGenerator::GenerateForLeader(
            anchor, current, obstacles, viewBoundary
        );

        for (size_t i = 0; i < 24; ++i) {
            double w = cands[i].textBox.Width();
            double h = cands[i].textBox.Height();
            ASSERT_NEAR(w, 20.0, 1e-9, "Candidate " + std::to_string(i) + " width = 20");
            ASSERT_NEAR(h, 10.0, 1e-9, "Candidate " + std::to_string(i) + " height = 10");
        }
    }

    std::cout << "\n=== Results: " << passed << " passed, " << failed << " failed ===\n";
    return failed == 0 ? 0 : 1;
}