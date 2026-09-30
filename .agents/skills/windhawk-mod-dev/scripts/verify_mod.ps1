param(
    [Parameter(Mandatory=$true)]
    [string]$SourcePath
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $SourcePath)) {
    Write-Error "Source file does not exist: $SourcePath"
    exit 1
}

$compiler = "C:\Program Files\Windhawk\Compiler\bin\clang++.exe"
$includeDir = "C:\Program Files\Windhawk\Compiler\include"

if (-not (Test-Path $compiler)) {
    Write-Error "Windhawk compiler not found at: $compiler"
    exit 1
}

$sourceFile = (Resolve-Path $SourcePath).Path
$sourceDir = Split-Path -Parent $sourceFile
$fileName = [System.IO.Path]::GetFileName($sourceFile)
$modId = $fileName -replace '\.wh\.cpp$', '' -replace '\.wh$', '' -replace '\.cpp$', ''

Write-Host "Verifying mod: $modId" -ForegroundColor Cyan
Write-Host "Source file  : $sourceFile" -ForegroundColor Gray
Write-Host "Include dir  : $sourceDir" -ForegroundColor Gray

$argList = @(
    "-fsyntax-only",
    "-std=c++20",
    "-DUNICODE",
    "-D_UNICODE",
    "-DWH_MOD",
    "-DWH_MOD_ID=L\`"$modId\`"",
    "`"-I$includeDir`"",
    "`"-I$sourceDir`"",
    "-include", "`"$includeDir\windhawk_api.h`"",
    "`"$sourceFile`""
)

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $compiler
$psi.Arguments = ($argList -join " ")
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $true

$proc = [System.Diagnostics.Process]::Start($psi)
$proc.WaitForExit()

$stdout = $proc.StandardOutput.ReadToEnd()
$stderr = $proc.StandardError.ReadToEnd()

if ($proc.ExitCode -eq 0) {
    Write-Host "[SUCCESS] Mod syntax verified with 0 errors!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "[FAILED] Mod syntax verification failed with exit code $($proc.ExitCode)" -ForegroundColor Red
    if ($stderr) {
        Write-Host $stderr -ForegroundColor Yellow
    }
    if ($stdout) {
        Write-Host $stdout
    }
    exit $proc.ExitCode
}
