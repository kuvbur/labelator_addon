//------------ kuvbur 2026 ------------
// ArchicadAdapter.cpp — Phase 2 Implementation + Refactoring
// Collects Labels and Dimensions, normalizes to Paper Space mm (LayoutOptimizer types)
// Integrates with LayoutOptimizer::CandidateGenerator and LayoutOptimizer::Solve

#include "ACAPinc.h"
#include "APIEnvir.h"
#include "APICommon25.h"
#include "ArchicadAdapter.hpp"
#include "LayoutOptimizer.hpp"
#include "CandidateGenerator.hpp"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace ArchicadAdapter {

    // -----------------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------------

    template <typename T> inline void BNZero (T &obj) { BNZeroMemory (&obj, sizeof (T)); }

    double GetActiveViewScaleDenominator () {
        double scale = 100.0;
        GSErrCode err = ACAPI_Database (APIDb_GetDrawingScaleID, &scale, nullptr);
        if (err == NoError && scale > 0.0)
            return scale;
        return 100.0;
    }

    LayoutOptimizer::Point2D ModelToPaperSpace (const API_Coord& modelCoord, double viewScaleDenom) {
        const double scaleFactor = 1000.0 / viewScaleDenom;
        return {
            modelCoord.x * scaleFactor,
            -modelCoord.y * scaleFactor  // Y-flip: model +Y up -> paper +Y down
        };
    }

    API_Coord PaperSpaceToModel (const LayoutOptimizer::Point2D& paperCoord, double viewScaleDenom) {
        const double scaleFactor = viewScaleDenom / 1000.0;
        return {
            paperCoord.x * scaleFactor,
            -paperCoord.y * scaleFactor
        };
    }

    // -----------------------------------------------------------------------------
    // Lock detection - используем стандартные API-флаги и атрибуты слоев
    // -----------------------------------------------------------------------------
    bool IsElementLocked (const API_Guid& guid) {
        API_Element element = {};
        BNZero (element);
        element.header.guid = guid;
        GSErrCode err = ACAPI_Element_Get (&element);
        if (err != NoError)
            return false;

        // 1. Проверка явной блокировки элемента через floorInd (0x8000 = locked)
        if (element.header.floorInd & 0x8000)
            return true;

        // 2. Проверка блокировки слоя
        API_Attribute layerAttr = {};
        BNZero (layerAttr);
        layerAttr.header.typeID = API_LayerID;
        layerAttr.header.index = element.header.layer;
        if (ACAPI_Attribute_Get (&layerAttr) == NoError) {
            if (layerAttr.layer.head.flags & 1)  // Layer locked flag
                return true;
        }

        return false;
    }

    LayoutOptimizer::ElementId GetElementId (const API_Guid& guid) {
        LayoutOptimizer::ElementId id = 0;
        id |= static_cast<LayoutOptimizer::ElementId> (guid.time_low) << 32;
        id |= static_cast<LayoutOptimizer::ElementId> (guid.time_mid) << 16;
        id |= static_cast<LayoutOptimizer::ElementId> (guid.time_hi_and_version);
        return id;
    }

    // -----------------------------------------------------------------------------
    // Точное извлечение текстового бокса из memo (API_ElementMemo)
    // -----------------------------------------------------------------------------
    LayoutOptimizer::Rect GetTextBoxPaperSpace (const API_Element& elem, double viewScaleDenom) {
        LayoutOptimizer::Rect box = {};

        API_ElementMemo memo = {};
        BNZero (memo);
        // AC25: Без маски (0) = получить все данные memo, включая Unicode paragraphs
        // (официальный пример Do_Label_Edit использует тот же паттерн)
        GSErrCode err = ACAPI_Element_GetMemo (elem.header.guid, &memo, 0);
        if (err != NoError)
            return box;

        double textWidthMm = 50.0;  // fallback — будет уточнён ниже
        double textHeightMm = 3.0;  // fallback — будет уточнён ниже

        // Извлекаем точный bounding box текста из paragraphs
        if (memo.paragraphs != nullptr && memo.paragraphs[0] != nullptr) {
            const API_ParagraphType& para = memo.paragraphs[0][0];
            // para.height — высота шрифта в модели (модельные единицы)
            double fontSizeMm = para.height > 0.0 ? para.height * 1000.0 / viewScaleDenom : 2.5;
            double charWidthMm = fontSizeMm * 0.6;

            // Ширина текста: из para.width, или из текста (если доступен), или оценка по числу символов
            if (para.width > 0.0) {
                // para.width задан явно (модельные единицы)
                textWidthMm = para.width * 1000.0 / viewScaleDenom;
            } else if (memo.textContent != nullptr && memo.textContent[0] != nullptr) {
                // Текст доступен — считаем по числу символов
                textWidthMm = strlen (memo.textContent[0]) * charWidthMm;
            } else {
                // Нет ни para.width, ни текста — оценка: 8 символов * ширина символа
                textWidthMm = 8.0 * charWidthMm;
            }
            textHeightMm = fontSizeMm * 1.2;
        } else if (elem.header.typeID == API_LabelID) {
            // Для custom label классов (labelClass != APILblClass_Text)
            // memo.paragraphs может быть null, используем textSize из элемента
            // textSize в модели — мм, но в Archicad API это может быть в pt
            double fontSizeMm = elem.label.textSize > 0.0 ? elem.label.textSize / viewScaleDenom : 2.5;
            double charWidthMm = fontSizeMm * 0.6;
            // Оценка: 8 символов * ширина символа для коротких выносок
            textWidthMm = 8.0 * charWidthMm;
            textHeightMm = fontSizeMm * 1.2;
        }

        // Позиция текста: refC для размеров, u.text.loc для выносок
        API_Coord pos = {};
        if (elem.header.typeID == API_DimensionID) {
            pos = elem.dimension.refC;
        } else if (elem.header.typeID == API_LabelID) {
            if (elem.label.labelClass == APILblClass_Text) {
                pos = elem.label.u.text.loc;
            } else {
                pos = elem.label.begC;
            }
        }

        LayoutOptimizer::Point2D p = ModelToPaperSpace (pos, viewScaleDenom);

        box.left = p.x;
        box.top = p.y - textHeightMm;
        box.right = p.x + textWidthMm;
        box.bottom = p.y;
        box.Normalize ();

        ACAPI_DisposeElemMemoHdls (&memo);
        return box;
    }

    // -----------------------------------------------------------------------------
    // Leader polyline extraction (Label elements)
    // -----------------------------------------------------------------------------
    std::vector<LayoutOptimizer::Point2D> GetLeaderPolylinePaperSpace (const API_Element& labelElem, double viewScaleDenom) {
        std::vector<LayoutOptimizer::Point2D> poly;

        API_Coord begC = labelElem.label.begC;
        API_Coord midC = labelElem.label.midC;
        API_Coord endC = labelElem.label.endC;

        poly.push_back (ModelToPaperSpace (begC, viewScaleDenom));
        if (midC.x != begC.x || midC.y != begC.y)
            poly.push_back (ModelToPaperSpace (midC, viewScaleDenom));
        if (endC.x != begC.x || endC.y != begC.y)
            poly.push_back (ModelToPaperSpace (endC, viewScaleDenom));

        return poly;
    }

    // -----------------------------------------------------------------------------
    // Dimension measure points extraction
    // -----------------------------------------------------------------------------
    std::vector<LayoutOptimizer::Point2D> GetDimensionMeasurePoints (const API_Element& dimElem, double viewScaleDenom) {
        std::vector<LayoutOptimizer::Point2D> pts;

        const API_DimensionType& dim = dimElem.dimension;

        pts.push_back (ModelToPaperSpace (dim.refC, viewScaleDenom));

        API_Coord dirEnd = {
            dim.refC.x + dim.direction.x,
            dim.refC.y + dim.direction.y
        };
        pts.push_back (ModelToPaperSpace (dirEnd, viewScaleDenom));

        return pts;
    }

    // -----------------------------------------------------------------------------
    // Element collection
    // -----------------------------------------------------------------------------
    CollectResult CollectElements () {
        CollectResult result;

        double viewScaleDenom = GetActiveViewScaleDenominator ();

        // Collect Labels (API_LabelID)
        {
            GS::Array<API_Guid> guids;
            GSErrCode err = ACAPI_Element_GetElemList (API_LabelID, &guids);
            if (err == NoError) {
                for (UInt32 i = 0; i < guids.GetSize (); ++i) {
                    const API_Guid& guid = guids[i];
                    API_Element elem = {};
                    BNZero (elem);
                    elem.header.guid = guid;

                    err = ACAPI_Element_Get (&elem);
                    if (err != NoError)
                        continue;

                    LayoutOptimizer::LeaderElement le;
                    le.id = GetElementId (guid);
                    le.locked = IsElementLocked (guid);

                    le.geo.anchorPoint = ModelToPaperSpace (elem.label.begC, viewScaleDenom);

                    le.layout.leaderPolyline = GetLeaderPolylinePaperSpace (elem, viewScaleDenom);
                    le.layout.textBox = GetTextBoxPaperSpace (elem, viewScaleDenom);
                    le.layout.textPosition = le.layout.textBox.Center ();

                    // Вычисляем shoulderLength из оригинальной геометрии (anchor -> shoulder)
                    if (le.layout.leaderPolyline.size () >= 2) {
                        LayoutOptimizer::Point2D d = le.layout.leaderPolyline[1] - le.layout.leaderPolyline[0];
                        le.geo.shoulderLength = std::sqrt (d.x * d.x + d.y * d.y);
                    }

                    result.leaderGuids.push_back (guid);
                    result.leaders.push_back (std::move (le));
                }
            }
        }

        // Collect Dimensions (API_DimensionID)
        {
            GS::Array<API_Guid> guids;
            GSErrCode err = ACAPI_Element_GetElemList (API_DimensionID, &guids);
            if (err == NoError) {
                for (UInt32 i = 0; i < guids.GetSize (); ++i) {
                    const API_Guid& guid = guids[i];
                    API_Element elem = {};
                    BNZero (elem);
                    elem.header.guid = guid;

                    err = ACAPI_Element_Get (&elem);
                    if (err != NoError)
                        continue;

                    LayoutOptimizer::DimensionElement de;
                    de.id = GetElementId (guid);
                    de.locked = IsElementLocked (guid);

                    de.geo.measurePoints = GetDimensionMeasurePoints (elem, viewScaleDenom);

                    if (de.geo.measurePoints.size () >= 2) {
                        LayoutOptimizer::Point2D dir = de.geo.measurePoints[1] - de.geo.measurePoints[0];
                        double len = std::sqrt (dir.x * dir.x + dir.y * dir.y);
                        if (len > 1e-6) {
                            de.geo.direction = {dir.x / len, dir.y / len};
                        }
                    }

                    de.layout.textBox = GetTextBoxPaperSpace (elem, viewScaleDenom);
                    de.layout.textPosition = de.layout.textBox.Center ();

                    result.dimensionGuids.push_back (guid);
                    result.dimensions.push_back (std::move (de));
                }
            }
        }

        return result;
    }

    // -----------------------------------------------------------------------------
    // Build forbidden geometry and obstacle polylines from locked elements
    // -----------------------------------------------------------------------------
    void BuildObstacleData (
        const CollectResult& collected,
        std::vector<LayoutOptimizer::Rect>& forbiddenGeometry,
        std::vector<std::vector<LayoutOptimizer::Point2D>>& obstaclePolylines
    ) {
        // Locked elements act as obstacles
        for (const auto& le : collected.leaders) {
            if (le.locked) {
                forbiddenGeometry.push_back (le.layout.textBox);
                if (le.layout.leaderPolyline.size () >= 2)
                    obstaclePolylines.push_back (le.layout.leaderPolyline);
            }
        }
        for (const auto& de : collected.dimensions) {
            if (de.locked) {
                forbiddenGeometry.push_back (de.layout.textBox);
            }
        }
    }

    // -----------------------------------------------------------------------------
    // Apply changes back to Archicad elements (wrapped in Undo scope)
    // -----------------------------------------------------------------------------
    GSErrCode ApplyChanges (const CollectResult& original,
                            const std::vector<LayoutOptimizer::LeaderElement>& solvedLeaders,
                            const std::vector<LayoutOptimizer::DimensionElement>& solvedDimensions,
                            double viewScaleDenom) {
        return ACAPI_CallUndoableCommand (GS::UniString ("Optimize Annotations"), [&] () -> GSErrCode {
            GSErrCode err = NoError;

            // Apply label changes
            for (size_t i = 0; i < solvedLeaders.size () && i < original.leaderGuids.size (); ++i) {
                const API_Guid& guid = original.leaderGuids[i];
                const LayoutOptimizer::LeaderElement& solved = solvedLeaders[i];

                if (solved.locked)
                    continue;

                API_Element elem = {};
                BNZero (elem);
                elem.header.guid = guid;

                err = ACAPI_Element_Get (&elem);
                if (err != NoError)
                    continue;

                // ВАЖНО: передаём nullptr вместо memo в ACAPI_Element_Change — 
                // мы изменяем только геометрию (midC/endC), текст не трогаем.
                // Это соответствует официальному примеру UpdateOneLabel (Element_Modify.cpp),
                // который меняет begC/midC/endC без работы с memo.

                // Build mask for label geometry changes
                // ВАЖНО: устанавливаем маску ТОЛЬКО на изменяемые поля.
                // begC (якорь) не меняется — не включаем его в маску.
                API_Element mask;
                BNZero (mask);
                ACAPI_ELEMENT_MASK_CLEAR (mask);
                ACAPI_ELEMENT_MASK_SET (mask, API_LabelType, midC);
                ACAPI_ELEMENT_MASK_SET (mask, API_LabelType, endC);
                if (elem.label.labelClass == APILblClass_Text) {
                    ACAPI_ELEMENT_MASK_SET (mask, API_LabelType, u.text.loc);
                    ACAPI_ELEMENT_MASK_SET (mask, API_LabelType, u.text.angle);
                } else {
                    // Для custom label классов (GDL) — позиция символа/текста
                    ACAPI_ELEMENT_MASK_SET (mask, API_LabelType, u.symbol.pos);
                }

                // Update label leader polyline (begC, midC, endC)
                // Гарантируем 3 точки для структурной целостности
                if (solved.layout.leaderPolyline.size () >= 1) {
                    elem.label.begC = PaperSpaceToModel (solved.layout.leaderPolyline[0], viewScaleDenom);
                }
                if (solved.layout.leaderPolyline.size () >= 2) {
                    elem.label.midC = PaperSpaceToModel (solved.layout.leaderPolyline[1], viewScaleDenom);
                } else {
                    // Если только 1 точка, дублируем для midC
                    elem.label.midC = elem.label.begC;
                }
                if (solved.layout.leaderPolyline.size () >= 3) {
                    elem.label.endC = PaperSpaceToModel (solved.layout.leaderPolyline[2], viewScaleDenom);
                } else {
                    // Если 2 точки, используем вторую как endC
                    elem.label.endC = elem.label.midC;
                }

                // Update text position for text labels
                // textPosition — центр textBox, а u.text.loc — левый-нижний угол.
                // Конвертируем центр → anchor, чтобы позиция не сдвигалась на каждом проходе (баг #1)
                if (elem.label.labelClass == APILblClass_Text) {
                    double halfW = solved.layout.textBox.Width () * 0.5;
                    double halfH = solved.layout.textBox.Height () * 0.5;
                    LayoutOptimizer::Point2D anchor = {
                        solved.layout.textPosition.x - halfW,
                        solved.layout.textPosition.y + halfH    // +Y вниз, anchor ниже центра
                    };
                    elem.label.u.text.loc = PaperSpaceToModel (anchor, viewScaleDenom);
                } else {
                    // Для custom label классов — позиция символа совпадает с endC
                    elem.label.u.symbol.pos = elem.label.endC;
                }

                err = ACAPI_Element_Change (&elem, &mask, nullptr, 0, true);
                if (err != NoError)
                    return err;
            }

            // Apply dimension changes
            for (size_t i = 0; i < solvedDimensions.size () && i < original.dimensionGuids.size (); ++i) {
                const API_Guid& guid = original.dimensionGuids[i];
                const LayoutOptimizer::DimensionElement& solved = solvedDimensions[i];

                if (solved.locked)
                    continue;

                API_Element elem = {};
                BNZero (elem);
                elem.header.guid = guid;

                err = ACAPI_Element_Get (&elem);
                if (err != NoError)
                    continue;

                // Get memo for dimension (required for ACAPI_Element_Change)
                // Get ALL memo data to preserve text content; we pass mask=0 to Change since we only modify geometry
                API_ElementMemo memo = {};
                BNZero (memo);
                err = ACAPI_Element_GetMemo (elem.header.guid, &memo);
                if (err != NoError || memo.paragraphs == nullptr) {
                    // No memo available, cannot modify safely
                    ACAPI_DisposeElemMemoHdls (&memo);
                    continue;
                }

                // Build mask for dimension text position
                API_Element mask;
                BNZero (mask);
                ACAPI_ELEMENT_MASK_CLEAR (mask);
                ACAPI_ELEMENT_MASK_SET (mask, API_DimensionType, defNote.pos);

                // Update text position - always update to the solved position
                elem.dimension.defNote.pos = PaperSpaceToModel (solved.layout.textPosition, viewScaleDenom);

                err = ACAPI_Element_Change (&elem, &mask, &memo, 0, true);
                ACAPI_DisposeElemMemoHdls (&memo);
                if (err != NoError)
                    return err;
            }

            return NoError;
        });
    }

    // -----------------------------------------------------------------------------
    // Full pipeline: Collect -> Generate Candidates -> Solve -> Apply
    // -----------------------------------------------------------------------------
    GSErrCode SolveAndApply () {
        // Collect elements
        CollectResult collected = CollectElements ();
        if (collected.error != NoError)
            return collected.error;

        // Build obstacle data from locked elements
        std::vector<LayoutOptimizer::Rect> forbiddenGeometry;
        std::vector<std::vector<LayoutOptimizer::Point2D>> obstaclePolylines;
        BuildObstacleData (collected, forbiddenGeometry, obstaclePolylines);

        // View boundary (paper space) - use a large default, could be computed from view
        LayoutOptimizer::Rect viewBoundary = LayoutOptimizer::Rect::FromCenter ({0, 0}, 10000, 10000);

        // Prepare input for LayoutOptimizer
        std::vector<LayoutOptimizer::ElementInput> inputs;

        // Add leaders - generate candidates via CandidateGenerator
        for (const auto& le : collected.leaders) {
            LayoutOptimizer::ElementInput input;
            input.type = LayoutOptimizer::ElementType::Leader;
            input.locked = le.locked;
            input.id = le.id;
            input.initialTextPosition = le.layout.textPosition;

            // Generate candidates using CandidateGenerator
            std::vector<LayoutOptimizer::Candidate> candidates =
                LayoutOptimizer::CandidateGenerator::GenerateForLeader (
                    le.geo.anchorPoint,
                    le.layout,
                    forbiddenGeometry,
                    viewBoundary
                );

            input.candidates = std::move (candidates);
            inputs.push_back (std::move (input));
        }

        // Add dimensions - generate candidates via CandidateGenerator
        for (const auto& de : collected.dimensions) {
            LayoutOptimizer::ElementInput input;
            input.type = LayoutOptimizer::ElementType::Dimension;
            input.locked = de.locked;
            input.id = de.id;
            input.initialTextPosition = de.layout.textPosition;

            // Use first measure point as anchor for candidate generation
            LayoutOptimizer::Point2D measurePoint = de.geo.measurePoints.empty ()
                ? LayoutOptimizer::Point2D{0, 0}
                : de.geo.measurePoints[0];

            std::vector<LayoutOptimizer::Candidate> candidates =
                LayoutOptimizer::CandidateGenerator::GenerateForDimension (
                    measurePoint,
                    de.layout,
                    de.geo.direction,
                    forbiddenGeometry,
                    viewBoundary
                );

            // Note: For dimensions, offset should be perpendicular distance.
            // The CandidateGenerator creates radial candidates; the solver will
            // evaluate them and the LayoutCost will handle alignment properly.

            input.candidates = std::move (candidates);
            inputs.push_back (std::move (input));
        }

        // Solve using LayoutOptimizer
        LayoutOptimizer::SolveResult solveResult = LayoutOptimizer::Solve (inputs, forbiddenGeometry, obstaclePolylines);

        // Extract solved leaders and dimensions
        std::vector<LayoutOptimizer::LeaderElement> solvedLeaders;
        std::vector<LayoutOptimizer::DimensionElement> solvedDimensions;

        size_t leaderIdx = 0;
        size_t dimIdx = 0;
        for (size_t i = 0; i < solveResult.selectedCandidates.size () && i < inputs.size (); ++i) {
            const LayoutOptimizer::Candidate& cand = solveResult.selectedCandidates[i];
            const LayoutOptimizer::ElementInput& input = inputs[i];

            if (input.type == LayoutOptimizer::ElementType::Leader && leaderIdx < collected.leaders.size ()) {
                LayoutOptimizer::LeaderElement le = collected.leaders[leaderIdx];
                le.layout.textPosition = cand.textPosition;
                le.layout.textBox = cand.textBox;
                le.layout.leaderPolyline = cand.leaderPolyline;
                le.layout.leaderAngle = cand.angle;
                solvedLeaders.push_back (std::move (le));
                ++leaderIdx;
            } else if (input.type == LayoutOptimizer::ElementType::Dimension && dimIdx < collected.dimensions.size ()) {
                LayoutOptimizer::DimensionElement de = collected.dimensions[dimIdx];
                de.layout.textPosition = cand.textPosition;
                de.layout.textBox = cand.textBox;
                de.layout.offset = cand.offset;
                solvedDimensions.push_back (std::move (de));
                ++dimIdx;
            }
        }

        // Apply changes
        double viewScaleDenom = GetActiveViewScaleDenominator ();
        return ApplyChanges (collected, solvedLeaders, solvedDimensions, viewScaleDenom);
    }

    GSErrCode RunOptimization () {
        return SolveAndApply ();
    }

} // namespace ArchicadAdapter