//------------ kuvbur 2026 ------------
// ArchicadAdapter integration — Phase 2/6
// Calls into LayoutOptimizer core (Phase 1-5) and writes back changes (Phase 6)

#include "ACAPinc.h"
#include "APIEnvir.h"
#include "optim.hpp"
#include "ArchicadAdapter.hpp"
#include "LayoutOptimizer.hpp"

// -----------------------------------------------------------------------------
// Оптимизация расположения выносок
// -----------------------------------------------------------------------------
void Optimaize ()
{
    // Phase 2: Collect elements via ArchicadAdapter
    ArchicadAdapter::CollectResult collected = ArchicadAdapter::CollectElements ();
    if (collected.error != NoError) {
        WriteReport_Err ("CollectElements failed", collected.error);
        return;
    }

    // Phase 3-5: Core optimization (placeholder - to be implemented in LayoutOptimizer.cpp)
    // For now, just log what we collected
    WriteReport ("Collected %zu leaders and %zu dimensions",
                 collected.leaders.size(), collected.dimensions.size());

    // TODO(Phase 3-5): 
    // 1. Normalize to Paper Space mm (already done in adapter)
    // 2. Call LayoutOptimizer::Optimize(elements)
    // 3. Get back Candidate results for each element

    // Phase 6: Write Back Changes
    // Wrapped in undoable command per AGENTS.md Section 2 Rule 3
    GSErrCode err = ACAPI_CallUndoableCommand ("Optimize Layout", [&] () -> GSErrCode {
        // For each leader: update text position, leader polyline
        for (const auto& leader : collected.leaders) {
            // API_Element element = ...; Get by leader.id
            // Modify element.label.textPosition, element.label.polygon
            // ACAPI_Element_Change (&element, ...);
        }
        // For each dimension: update text position
        for (const auto& dim : collected.dimensions) {
            // Similar write-back
        }
        return NoError;
    });

    if (err != NoError) {
        WriteReport_Err ("Undoable command failed", err);
    }
}