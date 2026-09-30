const { spawn } = require('child_process');
const fs = require('fs');

const filePath = 'C:\\Users\\ASUS\\Personal Windhawk Mods\\dynamic-island-jedmamosto-fork\\dynamic-island-jedmamosto-fork.wh.cpp';
const data = fs.readFileSync(filePath, 'utf8');

const ps = spawn('powershell', ['-NoProfile', '-Command', '[Console]::InputEncoding = [System.Text.Encoding]::UTF8; Set-Clipboard -Value ([Console]::In.ReadToEnd())'], {
    stdio: ['pipe', 'inherit', 'inherit']
});

ps.stdin.write(data, 'utf8');
ps.stdin.end();

ps.on('close', (code) => {
    console.log('UTF-8 Clipboard successfully updated with exit code:', code);
});
