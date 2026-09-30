param(
    [string]$SourcePath = "$PSScriptRoot\dynamic-island-jedmamosto-fork.wh.cpp"
)

$ErrorActionPreference = "Continue"

if (-not (Test-Path $SourcePath)) {
    Write-Error "Source file does not exist: $SourcePath"
    exit 1
}

$sourceFile = (Resolve-Path $SourcePath).Path
$sourceDir = Split-Path -Parent $sourceFile
$batFile = Join-Path $sourceDir "verify_build.bat"

Write-Host "Verifying mod : $([System.IO.Path]::GetFileNameWithoutExtension($sourceFile))" -ForegroundColor Cyan
Write-Host "Source file   : $sourceFile" -ForegroundColor Gray
Write-Host "Include dir   : $sourceDir" -ForegroundColor Gray

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$proc = Start-Process -FilePath "cmd.exe" -ArgumentList @("/c", "`"$batFile`"") -Wait -NoNewWindow -PassThru
$stopwatch.Stop()

$exitCode = $proc.ExitCode
$elapsedMs = $stopwatch.ElapsedMilliseconds

if ($exitCode -eq 0) {
    Write-Host "[SUCCESS] Mod syntax verified with 0 errors in ${elapsedMs}ms (PCH-Accelerated)!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "[FAILED] Mod syntax verification failed with exit code $exitCode in ${elapsedMs}ms" -ForegroundColor Red
    exit $exitCode
}
