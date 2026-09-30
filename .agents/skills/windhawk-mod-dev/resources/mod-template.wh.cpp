// ==WindhawkMod==
// @id              my-custom-windhawk-mod
// @name            My Custom Windhawk Mod
// @description     Desktop customization mod developed by Jed Mamosto.
// @version         1.0.0
// @author          Jed Mamosto
// @include         explorer.exe
// @compilerOptions -lole32 -loleaut32 -lshcore -ld2d1 -ldwrite -ldwmapi -lgdi32 -luser32 -lshell32
// @license         MIT
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# My Custom Windhawk Mod
Documentation and setup instructions for this mod.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- MySetting: true
  $name: Enable My Feature
  $description: Toggles custom behavior.
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <dwmapi.h>

// Subsystem companion headers:
// #include "my_feature_engine.hpp"

struct {
    bool mySetting = true;
} g_settings;

void LoadSettings() {
    g_settings.mySetting = Wh_GetIntSetting(L"MySetting") != 0;
}

BOOL Wh_ModInit() {
    Wh_Log(L"Init %s", WH_MOD_ID);
    LoadSettings();

    // Set up hooks or create UI overlay here
    return TRUE;
}

void Wh_ModUninit() {
    Wh_Log(L"Uninit %s", WH_MOD_ID);
}

void Wh_ModSettingsChanged() {
    Wh_Log(L"Settings changed");
    LoadSettings();
}
