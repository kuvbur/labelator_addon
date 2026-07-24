//------------ kuvbur 2022 ------------
#include "ACAPinc.h"
#include "APIEnvir.h"
#ifdef TESTING
    #include "TestFunc.hpp"
#endif

// -----------------------------------------------------------------------------
// Оптимизация расположения выносок
// -----------------------------------------------------------------------------
void Do_OptimizeAnnotations () {

    return;
} // Do_OptimizeAnnotations

static GSErrCode MenuCommandHandler (const API_MenuParams *menuParams) {
    GSErrCode err = NoError;
    const Int32 AddOnMenuID = ID_ADDON_MENU;
    switch (menuParams->menuItemRef.menuResID) {
    case AddOnMenuID:
        switch (menuParams->menuItemRef.itemIndex) {
        case MonAll_CommandID:
            Do_OptimizeAnnotations();
            break;
        default:
            break;
    }
    (void)err;
    return NoError;
}

API_AddonType __ACDLL_CALL CheckEnvironment (API_EnvirParams *envir) {
#ifdef TESTING
    DBprnt ("CheckEnvironment");
    TestFunc::Test ();
#endif
    RSGetIndString (&envir->addOnInfo.name, ID_ADDON_INFO, AddOnNameID, ACAPI_GetOwnResModule ());
    RSGetIndString (
        &envir->addOnInfo.description, ID_ADDON_INFO, AddOnDescriptionID, ACAPI_GetOwnResModule ());
    ACAPI_KeepInMemory (true);
    return APIAddon_Preload;
}

GSErrCode __ACDLL_CALL RegisterInterface (void) {
#if defined(TESTING)
    DBprnt ("RegisterInterface");
#endif
    GSErrCode err = NoError;
    err = ACAPI_Register_Menu (ID_ADDON_MENU, ID_ADDON_PROMT, MenuCode_Tools, MenuFlag_Default);
    ACAPI_KeepInMemory (true);
    return err;
}

GSErrCode __ACENV_CALL Initialize (void) {
    ACAPI_KeepInMemory (true);
    return ACAPI_Install_MenuHandler (ID_ADDON_MENU, MenuCommandHandler);
}

GSErrCode __ACENV_CALL FreeData (void) {
#if defined(TESTING)
    DBprnt ("!!!!!FreeData");
#endif
    return NoError;
}
