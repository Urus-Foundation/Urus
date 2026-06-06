# URUS test runner — Windows PowerShell
# Walks tests/run (must succeed) and tests/fail (must produce diagnostics),
# invoking the freshly built urusc.exe with --emit-c.
#
# Marker support (first 30 lines of each test file):
#   // harness: skip          — don't run this file (documentation marker)
#   // harness: args <flags>  — extra urusc flags for this file
#
# b020: also self-tests the --max-input-bytes override by compiling a
# known-good file with a 16-byte cap and requiring the refusal message.

param(
    [string]$Binary = "compiler/build/Debug/urusc.exe"
)

# NOT "Stop": urusc reports status lines on stderr, and under Windows
# PowerShell 5.1 `2>&1` wraps native stderr in ErrorRecords which would
# throw under Stop. We gate on $LASTEXITCODE explicitly instead.
$ErrorActionPreference = "Continue"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
Set-Location $ProjectRoot

if (-not (Test-Path $Binary)) {
    Write-Host "FAIL: urusc.exe not found at $Binary" -ForegroundColor Red
    Write-Host "  Build first:" -ForegroundColor Yellow
    Write-Host "    cmake -B compiler/build -S ." -ForegroundColor Yellow
    Write-Host "    cmake --build compiler/build" -ForegroundColor Yellow
    exit 1
}

$pass = 0
$fail = 0
$skip = 0

function Get-HarnessDirectives([string]$Path) {
    $d = @{ Skip = $false; Args = @() }
    Get-Content $Path -TotalCount 30 | ForEach-Object {
        if ($_ -match '^\s*//\s*harness:\s*skip\s*$') { $d.Skip = $true }
        elseif ($_ -match '^\s*//\s*harness:\s*args\s+(.+)$') {
            $d.Args = $Matches[1].Trim() -split '\s+'
        }
    }
    return $d
}

Write-Host "==> tests/run (must succeed)" -ForegroundColor Cyan
Get-ChildItem -Path tests/run -Filter *.urus | ForEach-Object {
    $name = $_.Name
    $d = Get-HarnessDirectives $_.FullName
    if ($d.Skip) {
        Write-Host "  SKIP  $name" -ForegroundColor DarkGray
        $script:skip++
        return
    }
    $out = & $Binary $_.FullName --emit-c @($d.Args) 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  PASS  $name" -ForegroundColor Green
        $script:pass++
    } else {
        Write-Host "  FAIL  $name" -ForegroundColor Red
        Write-Host $out
        $script:fail++
    }
}

Write-Host ""
Write-Host "==> tests/fail (must produce diagnostics)" -ForegroundColor Cyan
Get-ChildItem -Path tests/fail -Filter *.urus | ForEach-Object {
    $name = $_.Name
    $d = Get-HarnessDirectives $_.FullName
    if ($d.Skip) {
        Write-Host "  SKIP  $name" -ForegroundColor DarkGray
        $script:skip++
        return
    }
    & $Binary $_.FullName --emit-c @($d.Args) 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  PASS  $name (correctly rejected)" -ForegroundColor Green
        $script:pass++
    } else {
        Write-Host "  FAIL  $name (should have errored)" -ForegroundColor Red
        $script:fail++
    }
}

Write-Host ""
Write-Host "==> harness self-tests" -ForegroundColor Cyan

# 1. --max-input-bytes: tiny cap must refuse a known-good file.
& $Binary tests/run/01*.urus --emit-c --max-input-bytes 16 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Host "  PASS  --max-input-bytes 16 refuses oversized input" -ForegroundColor Green
    $pass++
} else {
    Write-Host "  FAIL  --max-input-bytes 16 should have refused" -ForegroundColor Red
    $fail++
}

# 2. --max-input-bytes: bad values are rejected.
& $Binary tests/run/01*.urus --emit-c --max-input-bytes 0 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Host "  PASS  --max-input-bytes 0 rejected as invalid" -ForegroundColor Green
    $pass++
} else {
    Write-Host "  FAIL  --max-input-bytes 0 should be invalid" -ForegroundColor Red
    $fail++
}

Write-Host ""
$color = if ($fail -eq 0) { 'Green' } else { 'Red' }
Write-Host "Result: $pass passed, $fail failed, $skip skipped" -ForegroundColor $color
if ($fail -gt 0) { exit 1 }
