//------------ kuvbur 2026 ------------
#include "ACAPinc.h"
#include "APIEnvir.h"
#include "Dimension_Main.hpp"
#include "ResourceIds.hpp"
#include "ArchicadAdapter.hpp"
#include "DebugExport.hpp"
#ifdef TESTING
    #include "TestFunc.hpp"
#endif

// -----------------------------------------------------------------------------
// Оптимизация расположения выносок
// -----------------------------------------------------------------------------
void Do_OptimizeAnnotations () {
    // Экспорт геометрии ДО оптимизации для отладки
    GSErrCode exportErr = LayoutOptimizer::ExportLabelsGeometryToCSV ("debug_labels_before.csv");
    if (exportErr == NoError) {
        exportErr = LayoutOptimizer::ExportDimensionsGeometryToCSV ("debug_dimensions_before.csv");
    }
    if (exportErr == NoError) {
        exportErr = LayoutOptimizer::ExportObstaclesToCSV ("debug_obstacles.csv");
    }

    GSErrCode err = ArchicadAdapter::RunOptimization();
    if (err != NoError) {
        // Log error
        ACAPI_WriteReport("LayoutOptimizer: Optimization failed with error %d", false, err);
    }

    // Экспорт геометрии ПОСЛЕ оптимизации для отладки
    LayoutOptimizer::ExportLabelsGeometryToCSV ("debug_labels_after.csv");
    LayoutOptimizer::ExportDimensionsGeometryToCSV ("debug_dimensions_after.csv");
}

static GSErrCode MenuCommandHandler (const API_MenuParams *menuParams) {
    GSErrCode err = NoError;
    const Int32 AddOnMenuID = ID_ADDON_MENU;
    switch (menuParams->menuItemRef.menuResID) {
    case AddOnMenuID:
        switch (menuParams->menuItemRef.itemIndex) {
        case OptimizeAnnotations_CommandID:
            Do_OptimizeAnnotations ();
            break;
        default:
            break;
        }
        return err;
    }
    return err;
}

API_AddonType __ACDLL_CALL CheckEnvironment (API_EnvirParams *envir) {
    RSGetIndString (&envir->addOnInfo.name, ID_ADDON_INFO, 1, ACAPI_GetOwnResModule ());
    RSGetIndString (&envir->addOnInfo.description, ID_ADDON_INFO, 2, ACAPI_GetOwnResModule ());
    ACAPI_KeepInMemory (true);
    return APIAddon_Preload;
}

GSErrCode __ACDLL_CALL RegisterInterface (void) {
    GSErrCode err = NoError;
    err = ACAPI_Register_Menu (ID_ADDON_MENU, ID_ADDON_PROMT, MenuCode_Tools, MenuFlag_Default);
    ACAPI_KeepInMemory (true);
    return err;
}

GSErrCode __ACENV_CALL Initialize (void) {
    ACAPI_KeepInMemory (true);
    return ACAPI_Install_MenuHandler (ID_ADDON_MENU, MenuCommandHandler);
}

GSErrCode __ACENV_CALL FreeData (void) { return NoError; }
