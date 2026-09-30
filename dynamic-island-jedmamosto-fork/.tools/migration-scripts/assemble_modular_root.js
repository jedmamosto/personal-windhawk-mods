const fs = require('fs');
const path = require('path');

const modDir = __dirname;
const srcFile = path.join(modDir, 'dynamic-island-jedmamosto-fork.wh.cpp.monolith.bak');
const dstFile = path.join(modDir, 'dynamic-island-jedmamosto-fork.wh.cpp');

const content = fs.readFileSync(srcFile, 'utf8');
const lines = content.split(/\r?\n/);

// Metadata, Readme, and Settings up to line 455
const settingsEndIdx = lines.findIndex(l => l.includes('// ==/WindhawkModSettings=='));
if (settingsEndIdx === -1) throw new Error('Settings end marker not found');

const headerBlock = lines.slice(0, settingsEndIdx + 1).join('\n');

// Tool runner and callbacks starting at BOOL WhTool_ModInit()
const toolInitIdx = lines.findIndex(l => l.includes('BOOL WhTool_ModInit()'));
if (toolInitIdx === -1) throw new Error('WhTool_ModInit not found');

const footerBlock = lines.slice(toolInitIdx).join('\n');

const modularRoot = `${headerBlock}

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

// Windhawk API
#include <windows.h>

// Modular Architecture Companion Headers
#include "island_common.hpp"
#include "palette_color_engine.hpp"
#include "icon_process_engine.hpp"
#include "weather_location_engine.hpp"
#include "telemetry_privacy_engine.hpp"
#include "media_smtc_engine.hpp"
#include "battery_dashboard.hpp"
#include "notification_engine.hpp"
#include "agy_telemetry_engine.hpp"
#include "bluetooth_dnd_engine.hpp"
#include "ui_painters_engine.hpp"
#include "window_hook_manager.hpp"

${footerBlock}
`;

fs.writeFileSync(dstFile, modularRoot, 'utf8');
const lineCount = modularRoot.split('\n').length;
console.log('Successfully generated modular dynamic-island-jedmamosto-fork.wh.cpp! Lines:', lineCount);
