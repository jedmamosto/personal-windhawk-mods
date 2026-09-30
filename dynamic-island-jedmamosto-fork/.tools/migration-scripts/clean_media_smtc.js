const fs = require('fs');
const path = require('path');

const filePath = path.resolve('dynamic-island-jedmamosto-fork/media_smtc_engine.hpp');
let code = fs.readFileSync(filePath, 'utf8');

// 1. Add #include "icon_process_engine.hpp" after #include "island_common.hpp"
code = code.replace(
    '#include "island_common.hpp"',
    '#include "island_common.hpp"\n#include "icon_process_engine.hpp"'
);

// 2. Remove ProcessImageNameForPid, ProcessImageNameForWindow, CopyWindowIcon, getProcessIcon, getWindowIcon, IconToPixels
const startToRemove1 = code.indexOf('inline bool ProcessImageNameForPid(');
const endToRemove1 = code.indexOf('// ----------------------------------------------------------------------------\n// WinRT IRandomAccessStreamReference to Bytes');

if (startToRemove1 !== -1 && endToRemove1 !== -1) {
    code = code.slice(0, startToRemove1) + code.slice(endToRemove1);
}

// 3. Remove LRU cache and Friendly helpers that are in icon_process_engine.hpp
const startToRemove2 = code.indexOf('// ----------------------------------------------------------------------------\n// Process Icon Caching for Media Players');
const endToRemove2 = code.indexOf('// ----------------------------------------------------------------------------\n// SMTC Session Selection & Resolution');

if (startToRemove2 !== -1 && endToRemove2 !== -1) {
    code = code.slice(0, startToRemove2) + code.slice(endToRemove2);
}

fs.writeFileSync(filePath, code, 'utf8');
console.log('Successfully cleaned media_smtc_engine.hpp!');
