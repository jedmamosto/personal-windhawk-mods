const fs = require('fs');
const path = require('path');

const filePath = path.resolve('dynamic-island-jedmamosto-fork/media_smtc_engine.hpp');
const lines = fs.readFileSync(filePath, 'utf8').split(/\r?\n/);

// Remove lines 106 to 282 (0-indexed: 105 to 282)
// Remove lines 347 to 490 (0-indexed: 346 to 490)
const toDelete = new Set();
for (let i = 105; i < 282; i++) {
    toDelete.add(i);
}
for (let i = 346; i < 490; i++) {
    toDelete.add(i);
}

const filtered = lines.filter((_, idx) => !toDelete.has(idx));
fs.writeFileSync(filePath, filtered.join('\n'), 'utf8');
console.log('Successfully pruned media_smtc_engine.hpp! Original lines:', lines.length, 'New lines:', filtered.length);
