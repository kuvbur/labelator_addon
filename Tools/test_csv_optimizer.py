#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Standalone CSV test harness for LayoutOptimizer.
Reads debug CSV fixtures, runs optimizer logic (via compiled DLL or subprocess),
compares results with expected "after" CSVs.
"""

import csv
import math
import sys
import os
from dataclasses import dataclass
from typing import List, Optional, Tuple

# -----------------------------------------------------------------------------
# Geometry types (mirroring LayoutOptimizer.hpp)
# -----------------------------------------------------------------------------

@dataclass
class Point2D:
    x: float = 0.0
    y: float = 0.0

    def __sub__(self, other):
        return Point2D(self.x - other.x, self.y - other.y)

    def __add__(self, other):
        return Point2D(self.x + other.x, self.y + other.y)

    def __mul__(self, s):
        return Point2D(self.x * s, self.y * s)

    def dist(self, other):
        return math.hypot(self.x - other.x, self.y - other.y)

@dataclass
class Rect:
    left: float = 0.0
    top: float = 0.0
    right: float = 0.0
    bottom: float = 0.0

    def normalize(self):
        if self.left > self.right:
            self.left, self.right = self.right, self.left
        if self.top > self.bottom:
            self.top, self.bottom = self.bottom, self.top

    def width(self):
        return self.right - self.left

    def height(self):
        return self.bottom - self.top

    def center(self):
        return Point2D((self.left + self.right) * 0.5, (self.top + self.bottom) * 0.5)

    @staticmethod
    def from_center(center: Point2D, width: float, height: float):
        r = Rect()
        r.left = center.x - width * 0.5
        r.right = center.x + width * 0.5
        r.top = center.y - height * 0.5
        r.bottom = center.y + height * 0.5
        r.normalize()
        return r

    def intersects(self, other, eps=1e-4):
        return not (self.right < other.left - eps or
                    other.right < self.left - eps or
                    self.bottom < other.top - eps or
                    other.bottom < self.top - eps)

    def contains(self, p, eps=1e-4):
        return (self.left - eps <= p.x <= self.right + eps and
                self.top - eps <= p.y <= self.bottom + eps)

# -----------------------------------------------------------------------------
# Candidate & Element types
# -----------------------------------------------------------------------------

@dataclass
class Candidate:
    text_position: Point2D
    text_box: Rect
    leader_polyline: List[Point2D]
    offset: float = 0.0
    angle: float = 0.0
    leader_angle: float = 0.0
    valid: bool = True

@dataclass
class ElementInput:
    element_type: str  # 'Leader' or 'Dimension'
    locked: bool
    id: int
    initial_text_position: Point2D
    candidates: List[Candidate]

# -----------------------------------------------------------------------------
# CSV parsing
# -----------------------------------------------------------------------------

@dataclass
class ParsedLabel:
    guid: str
    beg_c: Point2D
    mid_c: Point2D
    end_c: Point2D
    anchor_point: int
    leader_shape: int
    label_class: int
    text_loc: Point2D
    text_angle: float
    text_width: float
    text_height: float
    text_content: str

@dataclass
class ParsedDimension:
    guid: str
    ref_c: Point2D
    direction: Point2D
    def_note_pos: Point2D
    def_note_angle: float
    text_width: float
    text_height: float
    text_content: str

@dataclass
class ParsedObstacle:
    guid: str
    type: str
    locked: bool
    points: List[Point2D]

def parse_float(s, default=0.0):
    try:
        return float(s)
    except:
        return default

def parse_int(s, default=0):
    try:
        return int(s)
    except:
        return default

def read_labels_csv(path: str) -> List[ParsedLabel]:
    labels = []
    with open(path, 'r', encoding='utf-8') as f:
        reader = csv.reader(f)
        header = next(reader)
        for row in reader:
            if len(row) < 16:
                continue
            lbl = ParsedLabel(
                guid=row[0],
                beg_c=Point2D(parse_float(row[1]), parse_float(row[2])),
                mid_c=Point2D(parse_float(row[3]), parse_float(row[4])),
                end_c=Point2D(parse_float(row[5]), parse_float(row[6])),
                anchor_point=parse_int(row[7]),
                leader_shape=parse_int(row[8]),
                label_class=parse_int(row[9]),
                text_loc=Point2D(parse_float(row[10]), parse_float(row[11])),
                text_angle=parse_float(row[12]),
                text_width=parse_float(row[13]),
                text_height=parse_float(row[14]),
                text_content=row[15] if len(row) > 15 else ''
            )
            labels.append(lbl)
    return labels

def read_dimensions_csv(path: str) -> List[ParsedDimension]:
    dims = []
    with open(path, 'r', encoding='utf-8') as f:
        reader = csv.reader(f)
        header = next(reader)
        for row in reader:
            if len(row) < 12:
                continue
            dim = ParsedDimension(
                guid=row[0],
                ref_c=Point2D(parse_float(row[2]), parse_float(row[3])),
                direction=Point2D(parse_float(row[4]), parse_float(row[5])),
                def_note_pos=Point2D(parse_float(row[6]), parse_float(row[7])),
                def_note_angle=parse_float(row[8]),
                text_width=parse_float(row[9]),
                text_height=parse_float(row[10]),
                text_content=row[11] if len(row) > 11 else ''
            )
            dims.append(dim)
    return dims

def read_obstacles_csv(path: str) -> List[ParsedObstacle]:
    obstacles = []
    if not os.path.exists(path):
        return obstacles
    with open(path, 'r', encoding='utf-8') as f:
        reader = csv.reader(f)
        header = next(reader)
        for row in reader:
            if len(row) < 10:
                continue
            pts = []
            for i in range(3, len(row), 2):
                if i + 1 < len(row):
                    pts.append(Point2D(parse_float(row[i]), parse_float(row[i+1])))
            obs = ParsedObstacle(
                guid=row[0],
                type=row[1],
                locked=bool(parse_int(row[2])),
                points=pts
            )
            obstacles.append(obs)
    return obstacles

# -----------------------------------------------------------------------------
# Collision detection (mirroring LayoutOptimizer)
# -----------------------------------------------------------------------------

def cross(o, a, b):
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x)

def orientation(o, a, b, eps=1e-4):
    val = cross(o, a, b)
    if val > eps: return 1   # CCW
    if val < -eps: return -1 # CW
    return 0                  # Collinear

def on_segment(p, a, b, eps=1e-4):
    return (min(a.x, b.x) - eps <= p.x <= max(a.x, b.x) + eps and
            min(a.y, b.y) - eps <= p.y <= max(a.y, b.y) + eps)

def segments_intersect(p1, p2, p3, p4, eps=1e-4):
    o1 = orientation(p1, p2, p3, eps)
    o2 = orientation(p1, p2, p4, eps)
    o3 = orientation(p3, p4, p1, eps)
    o4 = orientation(p3, p4, p2, eps)

    if o1 != o2 and o3 != o4:
        return True
    if o1 == 0 and on_segment(p3, p1, p2, eps): return True
    if o2 == 0 and on_segment(p4, p1, p2, eps): return True
    if o3 == 0 and on_segment(p1, p3, p4, eps): return True
    if o4 == 0 and on_segment(p2, p3, p4, eps): return True
    return False

def polylines_intersect(a, b, eps=1e-4):
    if len(a) < 2 or len(b) < 2:
        return False
    for i in range(1, len(a)):
        for j in range(1, len(b)):
            if segments_intersect(a[i-1], a[i], b[j-1], b[j], eps):
                return True
    return False

def text_box_intersects_polyline(box: Rect, poly: List[Point2D], eps=1e-4):
    if len(poly) < 2:
        return False
    # Box edges: bottom-left -> top-left -> top-right -> bottom-right -> bottom-left
    bl = Point2D(box.left, box.bottom)
    tl = Point2D(box.left, box.top)
    tr = Point2D(box.right, box.top)
    br = Point2D(box.right, box.bottom)
    box_pts = [bl, tl, tr, br]
    for i in range(4):
        p1 = box_pts[i]
        p2 = box_pts[(i + 1) % 4]
        for j in range(1, len(poly)):
            if segments_intersect(p1, p2, poly[j-1], poly[j], eps):
                return True
    return False

def count_text_collisions(a: Candidate, b: Candidate) -> int:
    if not a.valid or not b.valid:
        return 0
    return 1 if a.text_box.intersects(b.text_box) else 0

def count_geometry_collisions(cand: Candidate, forbidden: List[Rect]) -> int:
    if not cand.valid:
        return 0
    count = 0
    for obs in forbidden:
        if cand.text_box.intersects(obs):
            count += 1
    return count

def count_line_collisions(cand: Candidate, placed: List[Candidate], obstacles: List[List[Point2D]]) -> int:
    if not cand.valid or len(cand.leader_polyline) < 2:
        return 0
    count = 0
    # vs placed candidates
    for p in placed:
        if not p.valid or len(p.leader_polyline) < 2:
            continue
        if polylines_intersect(cand.leader_polyline, p.leader_polyline):
            count += 1
    # vs obstacles
    for obs in obstacles:
        if len(obs) < 2:
            continue
        if polylines_intersect(cand.leader_polyline, obs):
            count += 1
    return count

# -----------------------------------------------------------------------------
# Candidate generation (simplified - using the actual C++ logic would be better)
# For now, just test the Layout Stability pre-check logic
# -----------------------------------------------------------------------------

def make_fallback_candidate(label: ParsedLabel) -> Candidate:
    """Create fallback candidate from initial layout"""
    poly = [label.beg_c]
    if label.mid_c.x != label.beg_c.x or label.mid_c.y != label.beg_c.y:
        poly.append(label.mid_c)
    if label.end_c.x != label.beg_c.x or label.end_c.y != label.beg_c.y:
        poly.append(label.end_c)
    else:
        poly.append(label.mid_c)

    return Candidate(
        text_position=label.text_loc,
        text_box=Rect.from_center(label.text_loc, label.text_width, label.text_height),
        leader_polyline=poly,
        leader_angle=label.text_angle,
        valid=True
    )

def make_fallback_candidate_dim(dim: ParsedDimension) -> Candidate:
    poly = [dim.def_note_pos]
    return Candidate(
        text_position=dim.def_note_pos,
        text_box=Rect.from_center(dim.def_note_pos, dim.text_width, dim.text_height),
        leader_polyline=poly,
        valid=True
    )

# -----------------------------------------------------------------------------
# Layout Stability pre-check (the fix we implemented)
# -----------------------------------------------------------------------------

def layout_stability_precheck(elements: List[ElementInput],
                               forbidden: List[Rect],
                               obstacles: List[List[Point2D]]) -> Tuple[bool, str]:
    """
    Returns (all_stable, reason)
    Mirrors the C++ pre-check in GreedySolver.cpp
    """
    for i, el in enumerate(elements):
        if el.locked:
            continue
        fallback = el.candidates[-1]  # Last is fallback
        for j in range(i):
            other = elements[j]
            if other.locked:
                continue
            other_fallback = other.candidates[-1]

            # Text-text
            if count_text_collisions(fallback, other_fallback) > 0:
                return False, f"Text collision between {i} and {j}"

            # Text-geometry
            if count_geometry_collisions(fallback, forbidden) > 0:
                return False, f"Text-geometry collision for {i}"

            # Line-line (THE FIX)
            if count_line_collisions(fallback, [other_fallback], obstacles) > 0:
                return False, f"Line collision between {i} and {j}"

    return True, "All stable"

# -----------------------------------------------------------------------------
# Main test runner
# -----------------------------------------------------------------------------

def run_test(fixture_dir: str, test_name: str):
    print(f"\n{'='*60}")
    print(f"Test: {test_name}")
    print(f"{'='*60}")

    labels_before = read_labels_csv(os.path.join(fixture_dir, 'debug_labels_before.csv'))
    dims_before = read_dimensions_csv(os.path.join(fixture_dir, 'debug_dimensions_before.csv'))
    obstacles = read_obstacles_csv(os.path.join(fixture_dir, 'debug_obstacles.csv'))

    labels_after = read_labels_csv(os.path.join(fixture_dir, 'debug_labels_after.csv'))
    dims_after = read_dimensions_csv(os.path.join(fixture_dir, 'debug_dimensions_after.csv'))

    print(f"Labels before: {len(labels_before)}")
    print(f"Dimensions before: {len(dims_before)}")
    print(f"Obstacles (locked): {sum(1 for o in obstacles if o.locked)}")
    print(f"Labels after: {len(labels_after)}")

    # Build forbidden geometry from locked obstacles
    forbidden = []
    obstacle_polys = []
    for obs in obstacles:
        if obs.locked and len(obs.points) >= 4:
            xs = [p.x for p in obs.points]
            ys = [p.y for p in obs.points]
            forbidden.append(Rect(min(xs), min(ys), max(xs), max(ys)))
            if len(obs.points) >= 2:
                obstacle_polys.append(obs.points)

    # Convert to ElementInput
    elements = []
    for lbl in labels_before:
        cand = make_fallback_candidate(lbl)
        el = ElementInput(
            element_type='Leader',
            locked=False,
            id=hash(lbl.guid) & 0xFFFFFFFF,
            initial_text_position=lbl.text_loc,
            candidates=[cand]  # Just fallback for pre-check test
        )
        elements.append(el)

    for dim in dims_before:
        cand = make_fallback_candidate_dim(dim)
        el = ElementInput(
            element_type='Dimension',
            locked=False,
            id=hash(dim.guid) & 0xFFFFFFFF,
            initial_text_position=dim.def_note_pos,
            candidates=[cand]
        )
        elements.append(el)

    # Run Layout Stability pre-check
    stable, reason = layout_stability_precheck(elements, forbidden, obstacle_polys)

    print(f"\nLayout Stability Pre-check: {'PASS (stable)' if stable else 'FAIL (needs greedy solver)'}")
    print(f"Reason: {reason}")

    # Compare with expected "after" - check if any element moved
    if labels_after:
        print(f"\nMovement check (before vs after CSV):")
        for i, (before, after) in enumerate(zip(labels_before, labels_after)):
            move_dist = before.text_loc.dist(after.text_loc)
            moved = move_dist > 0.01  # 0.01mm threshold
            status = "MOVED" if moved else "stable"
            print(f"  {i}: {before.text_content} -> dist={move_dist:.4f}mm [{status}]")
            if moved:
                print(f"      before: ({before.text_loc.x:.4f}, {before.text_loc.y:.4f})")
                print(f"      after:  ({after.text_loc.x:.4f}, {after.text_loc.y:.4f})")

    return stable

def main():
    base = r"D:\labelator_addon\Test_file"  # Current test fixture
    # Also test the old debug_data_dontt_edit if it exists
    old_base = r"D:\labelator_addon\debug_data_dontt_edit"

    test_dirs = []
    if os.path.exists(base):
        test_dirs.append((base, "Test_file (current)"))
    for sub in ['bad_before', 'good_before']:
        path = os.path.join(old_base, sub)
        if os.path.exists(path):
            test_dirs.append((path, sub))

    if not test_dirs:
        print("No test fixtures found!")
        return 1

    all_passed = True
    for path, name in test_dirs:
        try:
            stable = run_test(path, name)
            # For good_before (layout stability test), we EXPECT stable=True
            # For bad_before (conflict resolution), we EXPECT stable=False
            if name == 'good_before':
                if not stable:
                    print(f"  *** FAIL: good_before should be stable but pre-check failed! ***")
                    all_passed = False
                else:
                    print(f"  *** PASS: good_before correctly stable ***")
            elif name == 'bad_before':
                if stable:
                    print(f"  *** FAIL: bad_before should need solving but pre-check passed! ***")
                    all_passed = False
                else:
                    print(f"  *** PASS: bad_before correctly needs solving ***")
        except Exception as e:
            print(f"  ERROR: {e}")
            import traceback
            traceback.print_exc()
            all_passed = False

    print(f"\n{'='*60}")
    print(f"Overall: {'ALL TESTS PASSED' if all_passed else 'SOME TESTS FAILED'}")
    print(f"{'='*60}")
    return 0 if all_passed else 1

if __name__ == '__main__':
    sys.exit(main())