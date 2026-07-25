//------------ kuvbur 2026 ------------
#pragma once
#if !defined (OPTIM_HPP)
#define	OPTIM_HPP

// -----------------------------------------------------------------------------
// Archicad-facing entry point for the Layout Optimizer Add-On feature.
//
// Boundary note (see AGENTS.md, Sections 2 & 4):
//   This file belongs to the ArchicadAdapter layer, so it is allowed to
//   include ACAPI/GSRoot headers. The actual optimization logic must NOT
//   live here — it belongs in LayoutOptimizer/ (core), which must stay free
//   of any ACAPI include. This file's job is only:
//     1. Collect elements via ACAPI calls.
//     2. Convert geometry to Paper Space mm (Section 3 axis convention).
//     3. Call into LayoutOptimizer::Optimize(...) — core, not implemented
//        yet; see AGENTS.md Section 13, Phases 1-6.
//     4. Wrap the write-back in ACAPI_CallUndoableCommand.
//
// NOTE: the function is named "Optimaize" (not "Optimize") — this is a typo,
// but it is already wired up as the callable command from Archicad. Do not
// rename it here without also updating the caller; flag a rename to a human
// before doing it project-wide.
// -----------------------------------------------------------------------------

void Optimaize ();

#endif
