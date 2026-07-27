//------------ kuvbur 2026 ------------
// LayoutOptimizerTests.cpp — C++ Unit Tests (pure C++17, NO ACAPI)
// Reads CSV fixtures, tests GreedySolver logic
// TDD: Write failing test first, then make it pass

#include "LayoutOptimizer.hpp"
#include "CandidateGenerator.hpp"
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <iostream>
#include <cassert>

namespace LayoutOptimizerTests {

using LayoutOptimizer::Point2D;
using LayoutOptimizer::Rect;
using LayoutOptimizer::Candidate;
using LayoutOptimizer::ElementInput;
using LayoutOptimizer::ElementType;
using LayoutOptimizer::SolveResult;
using LayoutOptimizer::CandidateGenerator;
using LayoutOptimizer::Solve;
using LayoutOptimizer::EvaluateCandidate;
using LayoutOptimizer::CountTextCollisions;
using LayoutOptimizer::CountGeometryCollisions;
using LayoutOptimizer::CountLineCollisions;
using LayoutOptimizer::ComputeMovement;
using LayoutOptimizer::ComputeLeaderLength;
using LayoutOptimizer::ComputeAlignmentPenalty;

// -----------------------------------------------------------------------------
// Test Assertions
// -----------------------------------------------------------------------------
static void AssertPointEqual(const Point2D& a, const Point2D& b, double eps = 1e-4, const char* msg = "") {
    if (std::abs(a.x - b.x) > eps || std::abs(a.y - b.y) > eps) {
        std::cerr << "ASSERT FAILED: " << msg << " ((" << a.x << "," << a.y << ") != (" << b.x << "," << b.y << "))\n";
        std::exit(1);
    }
}
static void AssertTrue(bool cond, const char* msg = "") {
    if (!cond) {
        std::cerr << "ASSERT FAILED: " << msg << "\n";
        std::exit(1);
    }
}

// -----------------------------------------------------------------------------
// RED TEST 1: Single element with no conflicts stays at fallback
// -----------------------------------------------------------------------------
static void Test_SingleElement_NoMovement() {
    std::cout << "[TEST] SingleElement_NoMovement\n";
    
    // Create a single element with fallback candidate
    ElementInput input;
    input.type = ElementType::Leader;
    input.locked = false;
    input.id = 1;
    input.initialTextPosition = { 10, 10 };
    input.candidates.clear();

    // Fallback candidate at initial position
    Candidate fallback;
    fallback.textPosition = { 10, 10 };
    fallback.textBox = Rect::FromCenter({10, 10}, 12, 3);
    fallback.leaderPolyline = { {0, 0}, {5, 0}, {10, 10} };
    fallback.leaderAngle = 0;
    fallback.valid = true;
    input.candidates.push_back(fallback);

    // One local nudge candidate
    Candidate nudge;
    nudge.textPosition = { 12, 10 };
    nudge.textBox = Rect::FromCenter({12, 10}, 12, 3);
    nudge.leaderPolyline = { {0, 0}, {5, 0}, {12, 10} };
    nudge.valid = true;
    input.candidates.push_back(nudge);

    std::vector<ElementInput> elements = { input };
    std::vector<Rect> forbidden;
    std::vector<std::vector<Point2D>> obstacles;

    SolveResult result = Solve(elements, forbidden, obstacles);

    // Should pick fallback (index 0)
    AssertPointEqual(result.selectedCandidates[0].textPosition, {10, 10}, 1e-4, "Single element should not move");
    AssertTrue(result.zeroCollisionSolution, "Should be zero-collision");
    
    std::cout << "  PASSED\n";
}

// -----------------------------------------------------------------------------
// RED TEST 2: Two far-apart elements - both should stay at fallback
// -----------------------------------------------------------------------------
static void Test_TwoElements_OK_LABEL_Stable() {
    std::cout << "[TEST] TwoElements_OK_LABEL_Stable\n";
    
    std::vector<ElementInput> elements;

    // Element 0
    ElementInput e0;
    e0.type = ElementType::Leader; e0.locked = false; e0.id = 1;
    e0.initialTextPosition = { 10, 10 };
    e0.candidates = {
        { {10,10}, Rect::FromCenter({10,10},12,3), {{0,0},{5,0},{10,10}}, 0, true },
        { {12,10}, Rect::FromCenter({12,10},12,3), {{0,0},{5,0},{12,10}}, 0, true },
        { {10,10}, Rect::FromCenter({10,10},12,3), {{0,0},{5,0},{10,10}}, 0, true } // fallback
    };
    elements.push_back(e0);

    // Element 1 - far away, no overlap
    ElementInput e1;
    e1.type = ElementType::Leader; e1.locked = false; e1.id = 2;
    e1.initialTextPosition = { 10, 50 };
    e1.candidates = {
        { {10,50}, Rect::FromCenter({10,50},12,3), {{0,0},{5,0},{10,50}}, 0, true },
        { {12,50}, Rect::FromCenter({12,50},12,3), {{0,0},{5,0},{12,50}}, 0, true },
        { {10,50}, Rect::FromCenter({10,50},12,3), {{0,0},{5,0},{10,50}}, 0, true }
    };
    elements.push_back(e1);

    std::vector<Rect> forbidden;
    std::vector<std::vector<Point2D>> obstacles;

    SolveResult result = Solve(elements, forbidden, obstacles);

    AssertPointEqual(result.selectedCandidates[0].textPosition, {10, 10}, 1e-4, "Element 0 should not move");
    AssertPointEqual(result.selectedCandidates[1].textPosition, {10, 50}, 1e-4, "Element 1 should not move");
    AssertTrue(result.zeroCollisionSolution, "Should be zero-collision");
    
    std::cout << "  PASSED\n";
}

// -----------------------------------------------------------------------------
// RED TEST 3: Two overlapping elements - one must move via nudge candidate
// -----------------------------------------------------------------------------
static void Test_TwoElements_BAD_LABEL_Resolved() {
    std::cout << "[TEST] TwoElements_BAD_LABEL_Resolved\n";
    
    std::vector<ElementInput> elements;

    // Element 0 at x=10
    ElementInput e0;
    e0.type = ElementType::Leader; e0.locked = false; e0.id = 1;
    e0.initialTextPosition = { 10, 10 };
    e0.candidates = {
        { {10,10}, Rect::FromCenter({10,10},12,3), {{0,0},{5,0},{10,10}}, 0, true },
        { {12,10}, Rect::FromCenter({12,10},12,3), {{0,0},{5,0},{12,10}}, 0, true },
        { {8,10}, Rect::FromCenter({8,10},12,3), {{0,0},{5,0},{8,10}}, 0, true },
        { {10,10}, Rect::FromCenter({10,10},12,3), {{0,0},{5,0},{10,10}}, 0, true }
    };
    elements.push_back(e0);

    // Element 1 at x=18 - overlaps with element 0 (10±6 vs 18±6 = 4-16 vs 12-24, overlap 12-16)
    ElementInput e1;
    e1.type = ElementType::Leader; e1.locked = false; e1.id = 2;
    e1.initialTextPosition = { 18, 10 };
    e1.candidates = {
        { {18,10}, Rect::FromCenter({18,10},12,3), {{0,0},{5,0},{18,10}}, 0, true },
        { {20,10}, Rect::FromCenter({20,10},12,3), {{0,0},{5,0},{20,10}}, 0, true },
        { {16,10}, Rect::FromCenter({16,10},12,3), {{0,0},{5,0},{16,10}}, 0, true },
        { {18,10}, Rect::FromCenter({18,10},12,3), {{0,0},{5,0},{18,10}}, 0, true }
    };
    elements.push_back(e1);

    std::vector<Rect> forbidden;
    std::vector<std::vector<Point2D>> obstacles;

    SolveResult result = Solve(elements, forbidden, obstacles);

    // At least one element should move to resolve overlap
    bool moved = false;
    for (size_t i = 0; i < result.selectedCandidates.size(); ++i) {
        const auto& cand = result.selectedCandidates[i];
        const auto& init = elements[i].initialTextPosition;
        if (std::abs(cand.textPosition.x - init.x) > 0.1 || std::abs(cand.textPosition.y - init.y) > 0.1) {
            moved = true;
            break;
        }
    }
    AssertTrue(moved, "At least one element should move to resolve collision");
    AssertTrue(!result.zeroCollisionSolution, "Should not be zero-collision");
    
    std::cout << "  PASSED\n";
}

// -----------------------------------------------------------------------------
// RED TEST 4: Three elements in chain - middle one resolves
static void Test_ThreeElements_ChainConflict() {
    std::cout << "[TEST] ThreeElements_ChainConflict\n";
    
    std::vector<ElementInput> elements;
    const double y0 = 10;
    const double x = 10;

    for (int i = 0; i < 3; ++i) {
        ElementInput e;
        e.type = ElementType::Leader; e.locked = false; e.id = i + 1;
        e.initialTextPosition = { x, y0 + i * 3 };
        e.candidates = {
            { {x, y0 + i * 3}, Rect::FromCenter({x, y0 + i * 3}, 12, 3), {{0,0},{5,0},{x, y0 + i * 3}}, 0, true },
            { {x+2, y0 + i * 3}, Rect::FromCenter({x+2, y0 + i * 3}, 12, 3), {{0,0},{5,0},{x+2, y0 + i * 3}}, 0, true },
            { {x-2, y0 + i * 3}, Rect::FromCenter({x-2, y0 + i * 3}, 12, 3), {{0,0},{5,0},{x-2, y0 + i * 3}}, 0, true },
            { {x, y0 + i * 3}, Rect::FromCenter({x, y0 + i * 3}, 12, 3), {{0,0},{5,0},{x, y0 + i * 3}}, 0, true }
        };
        elements.push_back(e);
    }

    std::vector<Rect> forbidden;
    std::vector<std::vector<Point2D>> obstacles;

    SolveResult result = Solve(elements, forbidden, obstacles);

    // Verify no overlaps with 1mm shrink tolerance
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = i + 1; j < 3; ++j) {
            const auto& a = result.selectedCandidates[i].textBox;
            const auto& b = result.selectedCandidates[j].textBox;
            double eps = 0.5;
            bool intersects = !(a.right < b.left - eps || b.right < a.left - eps ||
                                a.bottom < b.top - eps || b.bottom < a.top - eps);
            AssertTrue(!intersects, "Elements should not overlap after resolution");
        }
    }
    
    std::cout << "  PASSED\n";
}

// -----------------------------------------------------------------------------
// CSV Fixture Tests
// -----------------------------------------------------------------------------

struct ParsedLabel {
    std::string guid;
    Point2D begC, midC, endC;
    int anchorPoint;
    int leaderShape;
    int labelClass;
    Point2D textLoc;
    double textAngle;
    double textWidth;
    double textHeight;
    std::string textContent;
};

static std::vector<ParsedLabel> ReadLabelsCSV(const std::string& path) {
    std::vector<ParsedLabel> labels;
    std::ifstream file(path);
    if (!file.is_open()) return labels;
    std::string line;
    std::getline(file, line); // skip header
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string token;
        ParsedLabel l;
        std::getline(ss, l.guid, ',');
        std::getline(ss, token, ','); l.begC.x = std::stod(token);
        std::getline(ss, token, ','); l.begC.y = std::stod(token);
        std::getline(ss, token, ','); l.midC.x = std::stod(token);
        std::getline(ss, token, ','); l.midC.y = std::stod(token);
        std::getline(ss, token, ','); l.endC.x = std::stod(token);
        std::getline(ss, token, ','); l.endC.y = std::stod(token);
        std::getline(ss, token, ','); l.anchorPoint = std::stoi(token);
        std::getline(ss, token, ','); l.leaderShape = std::stoi(token);
        std::getline(ss, token, ','); l.labelClass = std::stoi(token);
        std::getline(ss, token, ','); l.textLoc.x = std::stod(token);
        std::getline(ss, token, ','); l.textLoc.y = std::stod(token);
        std::getline(ss, token, ','); l.textAngle = std::stod(token);
        std::getline(ss, token, ','); l.textWidth = std::stod(token);
        std::getline(ss, token, ','); l.textHeight = std::stod(token);
        std::getline(ss, l.textContent, ',');
        labels.push_back(l);
    }
    return labels;
}

static ElementInput LabelToElementInput(const ParsedLabel& l) {
    ElementInput input;
    input.type = ElementType::Leader;
    input.locked = false;
    input.id = std::hash<std::string>{}(l.guid);
    input.initialTextPosition = l.textLoc;
    input.candidates.clear();

    // Fallback candidate at current position
    Candidate fallback;
    fallback.textPosition = l.textLoc;
    fallback.textBox = Rect::FromCenter(l.textLoc, l.textWidth, l.textHeight);
    fallback.leaderPolyline = { l.begC, l.midC, l.endC };
    fallback.leaderAngle = l.textAngle;
    fallback.valid = true;
    input.candidates.push_back(fallback);

    // Generate candidates (mirror CandidateGenerator logic)
    const double kCandidateAnglesDeg[8] = {0, 45, 90, 135, 180, 225, 270, 315};
    const double kCandidateDistancesMm[3] = {5, 10, 20};
    double shoulderLength = 5.0;
    if (l.endC.x != l.midC.x || l.endC.y != l.midC.y) {
        shoulderLength = std::sqrt((l.midC.x - l.begC.x)*(l.midC.x - l.begC.x) + (l.midC.y - l.begC.y)*(l.midC.y - l.begC.y));
    }

    // Radial candidates
    for (int ai = 0; ai < 8; ++ai) {
        double angle = kCandidateAnglesDeg[ai];
        for (int di = 0; di < 3; ++di) {
            double dist = kCandidateDistancesMm[di];
            double rad = angle * 3.14159265358979323846 / 180.0;
            Candidate cand;
            cand.angle = angle;
            cand.textPosition = {
                l.begC.x + dist * std::cos(rad),
                l.begC.y + dist * std::sin(rad)
            };
            double w = l.textWidth, h = l.textHeight;
            cand.textBox.left = cand.textPosition.x - w * 0.5;
            cand.textBox.right = cand.textPosition.x + w * 0.5;
            cand.textBox.top = cand.textPosition.y - h * 0.5;
            cand.textBox.bottom = cand.textPosition.y + h * 0.5;
            cand.leaderPolyline = { l.begC,
                { l.begC.x + shoulderLength * std::cos(rad), l.begC.y + shoulderLength * std::sin(rad) },
                cand.textPosition };
            cand.valid = true;
            input.candidates.push_back(cand);
        }
    }

    // Local nudge candidates (±2mm)
    const std::array<Point2D, 8> kLocalDirs = {{
        {-2, 0}, {2, 0}, {0, -2}, {0, 2},
        {-1.4, -1.4}, {1.4, -1.4}, {-1.4, 1.4}, {1.4, 1.4}
    }};
    for (const auto& dir : kLocalDirs) {
        Candidate cand;
        cand.textPosition = { l.textLoc.x + dir.x, l.textLoc.y + dir.y };
        double w = l.textWidth, h = l.textHeight;
        cand.textBox.left = cand.textPosition.x - w * 0.5;
        cand.textBox.right = cand.textPosition.x + w * 0.5;
        cand.textBox.top = cand.textPosition.y - h * 0.5;
        cand.textBox.bottom = cand.textPosition.y + h * 0.5;
        cand.leaderPolyline = { l.begC, l.midC, cand.textPosition };
        cand.angle = l.textAngle;
        cand.valid = true;
        input.candidates.push_back(cand);
    }

    // Fallback is last
    input.candidates.back() = fallback;
    return input;
}

static void Test_CSVFixture_Loaded() {
    std::cout << "[TEST] CSVFixture_Loaded\n";
    auto labels = ReadLabelsCSV("Test_file/debug_labels_before.csv");
    AssertTrue(!labels.empty(), "Should load at least one label");
    AssertTrue(labels.size() >= 4, "Should load 4 labels");
    std::cout << "  Loaded " << labels.size() << " labels\n";
    std::cout << "  PASSED\n";
}

static void Test_CSVFixture_OK_LABEL_Elements_Frozen() {
    std::cout << "[TEST] CSVFixture_OK_LABEL_Elements_Frozen\n";
    auto labels = ReadLabelsCSV("Test_file/debug_labels_before.csv");
    std::vector<ElementInput> elements;
    for (const auto& l : labels) {
        elements.push_back(LabelToElementInput(l));
    }

    std::vector<Rect> forbidden;
    std::vector<std::vector<Point2D>> obstacles;

    SolveResult result = Solve(elements, forbidden, obstacles);

    // Elements 0 and 1 are OK_LABEL - should stay at fallback
    AssertPointEqual(result.selectedCandidates[0].textPosition, elements[0].initialTextPosition, 1e-4, "Element 0 (OK_LABEL) should not move");
    AssertPointEqual(result.selectedCandidates[1].textPosition, elements[1].initialTextPosition, 1e-4, "Element 1 (OK_LABEL) should not move");

    // Elements 2 and 3 are BAD_LABEL - should move
    bool moved2 = std::abs(result.selectedCandidates[2].textPosition.x - elements[2].initialTextPosition.x) > 0.1 ||
                  std::abs(result.selectedCandidates[2].textPosition.y - elements[2].initialTextPosition.y) > 0.1;
    bool moved3 = std::abs(result.selectedCandidates[3].textPosition.x - elements[3].initialTextPosition.x) > 0.1 ||
                  std::abs(result.selectedCandidates[3].textPosition.y - elements[3].initialTextPosition.y) > 0.1;
    AssertTrue(moved2, "Element 2 (BAD_LABEL) should move");
    AssertTrue(moved3, "Element 3 (BAD_LABEL) should move");
    
    std::cout << "  PASSED\n";
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main() {
    std::cout << "========================================\n";
    std::cout << "LayoutOptimizer C++ Unit Tests\n";
    std::cout << "========================================\n";

    try {
        // RED tests - should FAIL initially
        Test_SingleElement_NoMovement();
        Test_TwoElements_OK_LABEL_Stable();
        Test_TwoElements_BAD_LABEL_Resolved();
        Test_ThreeElements_ChainConflict();
        
        // CSV fixture tests
        Test_CSVFixture_Loaded();
        Test_CSVFixture_OK_LABEL_Elements_Frozen();
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "UNKNOWN EXCEPTION\n";
        return 1;
    }

    std::cout << "========================================\n";
    std::cout << "ALL TESTS PASSED\n";
    std::cout << "========================================\n";
    return 0;
}

} // namespace LayoutOptimizerTests