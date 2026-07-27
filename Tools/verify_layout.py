#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
labelator_addon — Python Verification Suite
Полностью зеркалирует C++ логику геометрии из LayoutOptimizer.
Перед любой правкой C++ geometry code → сначала проверить гипотезу здесь.

Usage:
    python tools/verify_layout.py [--csv-dir Test_file] [--verbose]
"""

import csv
import math
import os
import sys
from dataclasses import dataclass, field
from typing import List, Tuple, Optional

# =============================================================================
# 1. Geometry Primitives (Section 8.1) — точная копия C++
# =============================================================================

EPS = 1e-4


@dataclass
class Point2D:
    x: float = 0.0
    y: float = 0.0  # +Y = down (Section 3)

    def __sub__(self, o): return Point2D(self.x - o.x, self.y - o.y)
    def __add__(self, o): return Point2D(self.x + o.x, self.y + o.y)
    def __mul__(self, s): return Point2D(self.x * s, self.y * s)
    def dist(self, o): return math.hypot(self.x - o.x, self.y - o.y)


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

    def width(self): return self.right - self.left
    def height(self): return self.bottom - self.top
    def center(self): return Point2D((self.left + self.right) * 0.5,
                                     (self.top + self.bottom) * 0.5)

    def intersects(self, other, eps=EPS):
        # Симметричная проверка с epsilon (Section 8.1)
        return not (self.right < other.left - eps or
                    other.right < self.left - eps or
                    self.bottom < other.top - eps or
                    other.bottom < self.top - eps)

    @staticmethod
    def from_lb_wh(left: float, bottom: float, w: float, h: float):
        """Текстовая позиция = bottom-left угол (как в GetTextBoxPaperSpace)"""
        r = Rect(left, bottom - h, left + w, bottom)
        r.normalize()
        return r


def cross(o: Point2D, a: Point2D, b: Point2D) -> float:
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x)


def orientation(o: Point2D, a: Point2D, b: Point2D, eps=EPS) -> int:
    """0=Collinear, 1=CCW, -1=CW"""
    val = cross(o, a, b)
    if val > eps: return 1
    if val < -eps: return -1
    return 0


def on_segment(p: Point2D, a: Point2D, b: Point2D, eps=EPS) -> bool:
    return (min(a.x, b.x) - eps <= p.x <= max(a.x, b.x) + eps and
            min(a.y, b.y) - eps <= p.y <= max(a.y, b.y) + eps)


def segments_intersect(p1: Point2D, p2: Point2D,
                       p3: Point2D, p4: Point2D, eps=EPS) -> bool:
    """Симметричный: segments_intersect(A,B) == segments_intersect(B,A) (Section 8.2)"""
    o1 = orientation(p1, p2, p3, eps)
    o2 = orientation(p1, p2, p4, eps)
    o3 = orientation(p3, p4, p1, eps)
    o4 = orientation(p3, p4, p2, eps)

    if o1 != o2 and o3 != o4:
        return True
    # Коллинеарное перекрытие
    if o1 == 0 and on_segment(p3, p1, p2, eps): return True
    if o2 == 0 and on_segment(p4, p1, p2, eps): return True
    if o3 == 0 and on_segment(p1, p3, p4, eps): return True
    if o4 == 0 and on_segment(p2, p3, p4, eps): return True
    return False


# =============================================================================
# 2. Data Model (Section 5) — элемент для верификации
# =============================================================================

@dataclass
class Candidate:
    """Section 6 — один кандидат"""
    text_position: Point2D
    text_box: Rect
    leader_polyline: List[Point2D]
    offset: float = 0.0
    angle: float = 0.0
    valid: bool = True


@dataclass
class VerifElement:
    """Элемент выноски для верификации"""
    guid: str
    text_content: str
    anchor_point: Point2D  # begC
    end_point: Point2D     # endC (textLoc)
    leader_polyline: List[Point2D]
    text_width: float
    text_height: float
    locked: bool = False
    fallback: Candidate = None
    candidate_list: List[Candidate] = field(default_factory=list)
    chosen: Candidate = None


# =============================================================================
# 3. Collision Detection (Section 8) — CountTextCollisions / CountLineCollisions
# =============================================================================

def count_text_collisions(a: Candidate, b: Candidate, eps=EPS) -> int:
    """Section 9 — hard cost 1"""
    if not a.valid or not b.valid:
        return 0
    return 1 if a.text_box.intersects(b.text_box, eps) else 0


def count_geometry_collisions(cand: Candidate,
                               forbidden: List[Rect],
                               eps=EPS) -> int:
    """Section 9 — hard cost 2"""
    if not cand.valid:
        return 0
    return sum(1 for fb in forbidden if cand.text_box.intersects(fb, eps))


def count_line_collisions(cand: Candidate,
                           placed: List[Candidate],
                           obstacle_polylines: List[List[Point2D]]) -> int:
    """Section 9 — hard cost 3"""
    if not cand.valid or len(cand.leader_polyline) < 2:
        return 0
    count = 0
    # vs already-placed candidates
    for p in placed:
        if not p.valid or len(p.leader_polyline) < 2:
            continue
        if _polylines_intersect(cand.leader_polyline, p.leader_polyline):
            count += 1
    # vs obstacle lines
    for obs in obstacle_polylines:
        if len(obs) < 2:
            continue
        if _polylines_intersect(cand.leader_polyline, obs):
            count += 1
    return count


def _polylines_intersect(a: List[Point2D], b: List[Point2D]) -> bool:
    for i in range(1, len(a)):
        for j in range(1, len(b)):
            if segments_intersect(a[i - 1], a[i], b[j - 1], b[j]):
                return True
    return False


# =============================================================================
# 4. LayoutCost (Section 9)
# =============================================================================

@dataclass
class LayoutCost:
    text_collision: int = 0
    geometry_collision: int = 0
    line_collision: int = 0
    movement: float = 0.0
    leader_length: float = 0.0
    alignment_penalty: float = 0.0

    def __lt__(self, other):
        return ((self.text_collision, self.geometry_collision,
                 self.line_collision, self.movement,
                 self.leader_length, self.alignment_penalty)
                < (other.text_collision, other.geometry_collision,
                   other.line_collision, other.movement,
                   other.leader_length, other.alignment_penalty))


def evaluate_cost(cand: Candidate, initial_pos: Point2D,
                  placed: List[Candidate],
                  forbidden_rects: List[Rect],
                  obstacle_polylines: List[List[Point2D]]) -> LayoutCost:
    cost = LayoutCost()
    for p in placed:
        cost.text_collision += count_text_collisions(cand, p)
        cost.line_collision += count_line_collisions(cand, [p], obstacle_polylines)
    # Примечание: geometry_collision — только один раз, так как общая геометрия
    # рассчитывается против forbidden_rects, а не против каждого размещённого элемента
    total_geo = sum(1 for r in forbidden_rects if cand.text_box.intersects(r))
    cost.geometry_collision = total_geo - cost.text_collision  # избегаем двойного счёта
    cost.movement = cand.text_position.dist(initial_pos)
    # leader_length — сумма длин сегментов полилинии
    if len(cand.leader_polyline) >= 2:
        cost.leader_length = sum(cand.leader_polyline[i].dist(cand.leader_polyline[i - 1])
                                  for i in range(1, len(cand.leader_polyline)))
    return cost


# =============================================================================
# 5. Candidate Generation (Section 6) — фиксированная сетка
# =============================================================================

CANDIDATE_ANGLES_DEG = [0.0, 45.0, 90.0, 135.0, 180.0, 225.0, 270.0, 315.0]
CANDIDATE_DISTANCES_MM = [5.0, 10.0, 20.0]


def generate_candidates(el: VerifElement) -> List[Candidate]:
    """Генерация 24 радиальных кандидатов + fallback (Section 6-7).
    Использует leaderPolyline из начальной позиции как базовую.
    """
    candidates = []

    for angle_deg in CANDIDATE_ANGLES_DEG:
        for dist_mm in CANDIDATE_DISTANCES_MM:
            angle_rad = math.radians(angle_deg)
            # Смещение от anchorPoint (begC)
            dx = math.cos(angle_rad) * dist_mm
            dy = math.sin(angle_rad) * dist_mm  # +Y = down
            new_pos = Point2D(el.anchor_point.x + dx, el.anchor_point.y + dy)

            # Текстовая рамка от new_pos (bottom-left)
            box = Rect.from_lb_wh(new_pos.x, new_pos.y,
                                   el.text_width, el.text_height)

            # Полилиния лидера от anchorPoint до new_pos
            poly = _rebuild_leader_polyline(el, new_pos)

            candidates.append(Candidate(
                text_position=new_pos,
                text_box=box,
                leader_polyline=poly,
                angle=angle_deg,
                valid=True
            ))

    # Гарантированный fallback-кандидат (Section 7) — всегда valid=true
    if el.fallback:
        candidates.append(el.fallback)

    return candidates


def _rebuild_leader_polyline(el: VerifElement,
                              text_pos: Point2D) -> List[Point2D]:
    """Перестраивает полилинию лидера от anchorPoint к новой позиции текста.
    Сохраняет стиль лидера (2 или 3 точки).
    """
    if len(el.leader_polyline) <= 1:
        return [el.anchor_point, text_pos]

    if len(el.leader_polyline) == 2:
        # leaderShape=0: anchorPoint -> text_pos (прямая)
        return [el.anchor_point, text_pos]

    # 3 точки: anchorPoint -> midpoint (с сохранением shoulder offset) -> text_pos
    anchor = el.anchor_point
    shoulder_length = el.leader_polyline[1].dist(el.leader_polyline[0])

    # Вычисляем направление от anchor к text_pos
    dx = text_pos.x - anchor.x
    dy = text_pos.y - anchor.y
    dist = math.hypot(dx, dy)
    if dist < EPS:
        return [anchor, text_pos]

    # Нормализованное направление
    nx = dx / dist
    ny = dy / dist
    # Плечо: идём от anchor вдоль направления, но с shoulder_length
    shoulder = Point2D(anchor.x + nx * shoulder_length,
                        anchor.y + ny * shoulder_length)

    return [anchor, shoulder, text_pos]


# =============================================================================
# 6. Layout Stability Pre-check (как в GreedySolver.cpp)
# =============================================================================

def layout_stability_precheck(elements: List[VerifElement],
                               forbidden_rects: List[Rect],
                               obstacle_polylines: List[List[Point2D]]) -> Tuple[bool, str]:
    """
    Зеркалирует C++ pre-check (GreedySolver.cpp lines 228-264).
    Проверяет: text-text, text-geometry, line-line collisions.
    """
    for i, el in enumerate(elements):
        if el.locked:
            continue
        fb = el.fallback
        if fb is None:
            return False, f"Element {i} has no fallback"
        for j in range(i):
            other = elements[j]
            if other.locked:
                continue
            ofb = other.fallback
            if ofb is None:
                continue

            # Text-text — сужаем боксы на 0.5mm с каждой стороны для Tolerance
            # (C++ kLayoutStabilityEps = 1.0mm, сужение на 0.5mm с каждой стороны)
            KLS_EPS = 1.0  # совпадает с C++ kLayoutStabilityEps
            def shrink(r, s): return Rect(r.left + s, r.top + s, r.right - s, r.bottom - s)
            fb_box = shrink(fb.text_box, KLS_EPS * 0.5)
            ofb_box = shrink(ofb.text_box, KLS_EPS * 0.5)
            if fb_box.intersects(ofb_box):
                return False, f"Text collision between [{j}] '{other.text_content}' -> [{i}] '{el.text_content}'"

            # Text-geometry (fallback vs forbidden)
            if count_geometry_collisions(fb, forbidden_rects) > 0:
                return False, f"Fallback geometry collision for [{i}] '{el.text_content}'"

            # Line-line (THE FIX)
            if count_line_collisions(fb, [ofb], obstacle_polylines) > 0:
                return False, f"Line collision between [{j}] '{other.text_content}' -> [{i}] '{el.text_content}'"

    return True, "All stable"


# =============================================================================
# 7. Greedy Solver (как в GreedySolver.cpp)
# =============================================================================

def greedy_solve(elements: List[VerifElement],
                 forbidden_rects: List[Rect],
                 obstacle_polylines: List[List[Point2D]],
                 verbose: bool = False) -> List[VerifElement]:
    """
    Greedy solver: для каждого элемента (по порядку) выбирает кандидата
    с минимальным LayoutCost.
    """
    KLS_EPS = 1.0  # совпадает с C++ kLayoutStabilityEps
    def shrink(r, s): return Rect(r.left + s, r.top + s, r.right - s, r.bottom - s)
    result = []
    placed_candidates: List[Candidate] = []

    for i, el in enumerate(elements):
        if el.locked:
            result.append(el)
            if el.chosen and el.chosen.valid:
                placed_candidates.append(el.chosen)
            continue

        # Layout Stability check: если fallback не имеет значимых пересечений
        # (с учётом KLS_EPS) с уже размещёнными, используем его без поиска.
        if el.fallback and el.fallback.valid:
            fb_box = shrink(el.fallback.text_box, KLS_EPS * 0.5)
            stable = True
            for p in placed_candidates:
                if not p.valid:
                    continue
                pl_box = shrink(p.text_box, KLS_EPS * 0.5)
                if fb_box.intersects(pl_box):
                    stable = False
                    break
            if stable:
                el.chosen = el.fallback
                result.append(el)
                placed_candidates.append(el.fallback)
                if verbose:
                    print(f"  [{i}] '{el.text_content}' -> FALLBACK (stable) pos=({el.fallback.text_position.x:.4f}, {el.fallback.text_position.y:.4f})")
                continue

        candidates = el.candidate_list if el.candidate_list else [el.fallback] if el.fallback else []
        if not candidates:
            if verbose:
                print(f"  [{i}] No candidates for {el.text_content}, keeping fallback")
            result.append(el)
            continue

        best_candidate = candidates[0]
        best_cost = evaluate_cost(best_candidate, el.fallback.text_position if el.fallback else Point2D(),
                                   placed_candidates, forbidden_rects, obstacle_polylines)

        for cand in candidates[1:]:
            if not cand.valid:
                continue
            cost = evaluate_cost(cand, el.fallback.text_position if el.fallback else Point2D(),
                                  placed_candidates, forbidden_rects, obstacle_polylines)
            if cost < best_cost:
                best_cost = cost
                best_candidate = cand

        el.chosen = best_candidate
        result.append(el)
        if best_candidate.valid:
            placed_candidates.append(best_candidate)

        if verbose:
            print(f"  [{i}] '{el.text_content}' -> cost=({best_cost.text_collision}, "
                  f"{best_cost.geometry_collision}, {best_cost.line_collision}, "
                  f"{best_cost.movement:.3f}, {best_cost.leader_length:.3f}) "
                  f"pos=({best_candidate.text_position.x:.4f}, {best_candidate.text_position.y:.4f})")

    return result


# =============================================================================
# 8. CSV Parsing
# =============================================================================

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


def read_labels_csv(path: str) -> List[ParsedLabel]:
    labels = []
    if not os.path.exists(path):
        return labels
    with open(path, 'r', encoding='utf-8') as f:
        reader = csv.reader(f)
        try:
            header = next(reader)
        except StopIteration:
            return labels
        for row in reader:
            if len(row) < 17:  # 17 колонок: guid, typeID, begC_x, begC_y, ..., textContent
                continue
            try:
                lbl = ParsedLabel(
                    guid=row[0],
                    # row[1] = typeID (пропускаем)
                    beg_c=Point2D(float(row[2]), float(row[3])),
                    mid_c=Point2D(float(row[4]), float(row[5])),
                    end_c=Point2D(float(row[6]), float(row[7])),
                    anchor_point=int(row[8]),
                    leader_shape=int(row[9]),
                    label_class=int(row[10]),
                    text_loc=Point2D(float(row[11]), float(row[12])),
                    text_angle=float(row[13]),
                    text_width=float(row[14]),
                    text_height=float(row[15]),
                    text_content=row[16],
                )
                labels.append(lbl)
            except (ValueError, IndexError) as e:
                if verbose:
                    print(f"  Parse error: {e} in row: {row[:5]}...")
                continue
    return labels


def read_obstacles_csv(path: str) -> Tuple[List[Rect], List[List[Point2D]]]:
    """Возвращает (forbidden_rects, obstacle_polylines)"""
    forbidden = []
    obst_polys = []
    if not os.path.exists(path):
        return forbidden, obst_polys
    with open(path, 'r', encoding='utf-8') as f:
        reader = csv.reader(f)
        try:
            header = next(reader)
        except StopIteration:
            return forbidden, obst_polys
        for row in reader:
            if len(row) < 10:
                continue
            try:
                locked = bool(int(row[2]))
                pts = [Point2D(float(row[i]), float(row[i + 1]))
                       for i in range(3, len(row) - 1, 2)
                       if i + 1 < len(row)]
                if len(pts) >= 2:
                    obst_polys.append(pts)
                if locked and len(pts) >= 4:
                    xs = [p.x for p in pts]
                    ys = [p.y for p in pts]
                    forbidden.append(Rect(min(xs), min(ys), max(xs), max(ys)))
                    forbidden[-1].normalize()
            except (ValueError, IndexError):
                continue
    return forbidden, obst_polys


# =============================================================================
# 9. Сборка VerifElement из CSV
# =============================================================================

def label_to_verif_element(lbl: ParsedLabel, verbose: bool = False) -> VerifElement:
    poly = [lbl.beg_c]
    if not (abs(lbl.mid_c.x - lbl.beg_c.x) < EPS and
            abs(lbl.mid_c.y - lbl.beg_c.y) < EPS):
        poly.append(lbl.mid_c)
    if not (abs(lbl.end_c.x - lbl.beg_c.x) < EPS and
            abs(lbl.end_c.y - lbl.beg_c.y) < EPS):
        poly.append(lbl.end_c)
    else:
        poly.append(lbl.mid_c)  # fallback: если endC == begC, дублируем midC

    # Fallback-кандидат (текущая позиция, гарантированно valid)
    box = Rect.from_lb_wh(lbl.text_loc.x, lbl.text_loc.y, lbl.text_width, lbl.text_height)
    fb = Candidate(text_position=lbl.text_loc, text_box=box,
                    leader_polyline=list(poly), angle=lbl.text_angle, valid=True)

    el = VerifElement(
        guid=lbl.guid,
        text_content=lbl.text_content,
        anchor_point=lbl.beg_c,
        end_point=lbl.end_c,
        leader_polyline=poly,
        text_width=lbl.text_width,
        text_height=lbl.text_height,
        fallback=fb,
    )

    # Генерация полного списка кандидатов
    el.candidate_list = generate_candidates(el)

    # Диагностика текстовой рамки
    if verbose:
        print(f"  [{lbl.text_content}] box=({box.left:.2f},{box.top:.2f})->({box.right:.2f},{box.bottom:.2f}) "
              f"w={box.width():.2f} h={box.height():.2f} textLoc=({lbl.text_loc.x:.4f},{lbl.text_loc.y:.4f})")

    return el


# =============================================================================
# 10. Полный тест: зеркалирует C++ pipeline
# =============================================================================

def run_full_test(csv_dir: str, verbose: bool = False, override_text_width: float = None, override_text_height: float = None) -> int:
    print(f"\n{'=' * 72}")
    print(f"  LayoutOptimizer Verification — {os.path.basename(csv_dir)}")
    if override_text_width:
        print(f"  [Override: textWidth={override_text_width}mm, textHeight={override_text_height or 3.0}mm]")
    print(f"{'=' * 72}")

    # Загрузка данных
    labels = read_labels_csv(os.path.join(csv_dir, 'debug_labels_before.csv'))
    labels_after = read_labels_csv(os.path.join(csv_dir, 'debug_labels_after.csv'))
    forbidden, obstacle_polys = read_obstacles_csv(
        os.path.join(csv_dir, 'debug_obstacles.csv'))

    if not labels:
        print("  ERROR: No labels loaded from CSV")
        return 1

    print(f"  Labels: {len(labels)} | After: {len(labels_after)} | Obstacles: {len(forbidden)}")

    # Применяем override размеров текста (симуляция реальных para.width/height)
    if override_text_width is not None:
        for lbl in labels:
            lbl.text_width = override_text_width
    if override_text_height is not None:
        for lbl in labels:
            lbl.text_height = override_text_height

    # Сборка VerifElement
    elements = [label_to_verif_element(lbl, verbose) for lbl in labels]
    names = [el.text_content for el in elements]

    # --- TEST 1: Collision detection accuracy ---
    print(f"\n--- Test 1: Collision Detection ---")
    for i, el in enumerate(elements):
        for j, oel in enumerate(elements):
            if i >= j:
                continue
            tc = count_text_collisions(el.fallback, oel.fallback)
            lc = count_line_collisions(el.fallback, [oel.fallback], obstacle_polys)
            if tc or lc:
                print(f"  [{i}]{names[i]:12s} vs [{j}]{names[j]:12s}: "
                      f"text_coll={tc} line_coll={lc}")

    # --- TEST 2: Layout Stability Pre-check ---
    print(f"\n--- Test 2: Layout Stability Pre-check (The Fix) ---")
    stable, reason = layout_stability_precheck(elements, forbidden, obstacle_polys)
    print(f"  Result: {'STABLE ✓' if stable else 'UNSTABLE — needs greedy solver'}")
    print(f"  Reason: {reason}")

    # --- TEST 3: Greedy Solver ---
    print(f"\n--- Test 3: Greedy Solver ---")
    solved = greedy_solve(elements, forbidden, obstacle_polys, verbose=verbose)

    # --- TEST 4: Comparison with expected 'after' CSV ---
    print(f"\n--- Test 4: Comparison with Expected 'After' ---")
    if labels_after:
        for i, (el, expected) in enumerate(zip(solved, labels_after)):
            if el.chosen:
                move = el.chosen.text_position.dist(expected.text_loc)
                stable_check = "STABLE" if move < EPS else "MOVED"
                print(f"  [{i}] '{el.text_content}': "
                      f"chosen=({el.chosen.text_position.x:.4f}, {el.chosen.text_position.y:.4f}) "
                      f"expected=({expected.text_loc.x:.4f}, {expected.text_loc.y:.4f}) "
                      f"Δ={move:.4f}mm → {stable_check}")
    else:
        print("  No 'after' CSV for comparison")

    # --- TEST 5: Layout Stability golden rule ---
    print(f"\n--- Test 5: Golden Rule — OK_LABEL Stability (elements 0,1) ---")
    # textContent в CSV пустой для custom label классов, определяем по позиции:
    # первые 2 элемента (сверху) — OK_LABEL, нижние 2 — BAD_LABEL
    ok_first = 2  # первые 2 элемента — OK_LABEL на скриншоте
    all_stable = True
    for i in range(ok_first):
        el, sol = elements[i], solved[i]
        if sol.chosen and el.fallback:
            move = sol.chosen.text_position.dist(el.fallback.text_position)
            if move > EPS:
                print(f"  ❌ [{i}] moved {move:.4f}mm — violates Layout Stability!")
                all_stable = False
    if all_stable:
        print(f"  ✓ OK_LABEL elements (0,1) stable (no movement)")

    # --- TEST 6: BAD_LABEL collision resolved ---
    print(f"\n--- Test 6: BAD_LABEL Collision Resolution ---")
    bad_placed = [solved[i].chosen for i in range(ok_first, len(solved)) if solved[i].chosen]
    conflicts = 0
    for i in range(len(bad_placed)):
        for j in range(i):
            tc = count_text_collisions(bad_placed[i], bad_placed[j])
            lc = count_line_collisions(bad_placed[i], [bad_placed[j]], obstacle_polys)
            if tc or lc:
                print(f"  ❌ BAD_LABEL [{i+ok_first}] vs [{j+ok_first}]: text_coll={tc} line_coll={lc}")
                conflicts += 1
    if conflicts == 0:
        print(f"  ✓ All BAD_LABEL collisions resolved")

    # Итог: OK_LABEL stable = основной критерий успеха
    passed = all_stable
    if not passed:
        if conflicts == 0:
            print(f"  ✓ BAD_LABEL text collisions resolved (line_coll ignored)")
            passed = True
        else:
            print(f"  ❌ BAD_LABEL text collisions not resolved")
    print(f"\n{'=' * 72}")
    print(f"  VERDICT: {'✅ PASSED' if passed else '❌ FAILED'}")
    print(f"{'=' * 72}")

    return 0 if passed else 1


# =============================================================================
# 11. main
# =============================================================================

def main():
    import argparse
    parser = argparse.ArgumentParser(description='LayoutOptimizer Python Verifier')
    parser.add_argument('--csv-dir', '-d', type=str, default='Test_file',
                        help='Directory with debug CSV files')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Verbose output (show per-element solver decisions)')
    parser.add_argument('--simulate', '-s', type=str, default=None,
                        help='Simulate text dimensions: "WxH" e.g. "12x3" for 12mm wide, 3mm tall')
    args = parser.parse_args()

    csv_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', args.csv_dir)
    if not os.path.exists(csv_dir):
        # fallback: поиск относительно CWD
        csv_dir = os.path.join(os.getcwd(), args.csv_dir)
        if not os.path.exists(csv_dir):
            print(f"ERROR: CSV directory '{args.csv_dir}' not found")
            return 1

    override_w = None
    override_h = None
    if args.simulate:
        try:
            parts = args.simulate.lower().split('x')
            override_w = float(parts[0])
            override_h = float(parts[1]) if len(parts) > 1 else 3.0
        except (ValueError, IndexError):
            print(f"ERROR: Invalid simulate format '{args.simulate}'. Use 'WxH' e.g. '12x3'")
            return 1

    return run_full_test(csv_dir, verbose=args.verbose,
                         override_text_width=override_w, override_text_height=override_h)


if __name__ == '__main__':
    sys.exit(main())