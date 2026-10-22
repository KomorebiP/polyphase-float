param(
    [string]$Bits = "64,128,256",
    [int]$Repetitions = 5,
    [switch]$SkipTests
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Convert-ToWslPath {
    param([Parameter(Mandatory = $true)][string]$WindowsPath)
    $full = [System.IO.Path]::GetFullPath($WindowsPath)
    if ($full -match '^([A-Za-z]):\\(.*)$') {
        $drive = $Matches[1].ToLowerInvariant()
        $tail = $Matches[2].Replace('\\', '/')
        return "/mnt/$drive/$tail"
    }
    $translated = & wsl.exe -e wslpath -a -u -- "$full" 2>$null | Select-Object -First 1
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace([string]$translated)) {
        throw "WSL could not translate: $full"
    }
    return ([string]$translated).Trim()
}

function Invoke-WslChecked {
    param([Parameter(Mandatory = $true)][string]$Command, [switch]$AsRoot)
    if ($AsRoot) { & wsl.exe -u root -e bash -lc $Command }
    else { & wsl.exe -e bash -lc $Command }
    if ($LASTEXITCODE -ne 0) { throw "WSL command failed with exit code $LASTEXITCODE." }
}

$root = (Resolve-Path $PSScriptRoot).Path
$wslRoot = Convert-ToWslPath $root
if ($wslRoot.Contains("'")) { throw "The package path cannot contain an apostrophe." }
$qRoot = "'$wslRoot'"

& wsl.exe -e bash -lc "printf WSL_READY" 2>$null | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Ubuntu WSL is unavailable." }

Write-Host ""
Write-Host "Independent correctly rounded special functions and constants" -ForegroundColor Cyan
Write-Host "[1/4] Installing WSL dependencies..." -ForegroundColor Yellow
$install = "export DEBIAN_FRONTEND=noninteractive; command -v apt-get >/dev/null 2>&1 || { echo 'Ubuntu/Debian WSL is required.' >&2; exit 2; }; apt-get update -qq && apt-get install -y -qq build-essential make cmake pkg-config libgmp-dev libmpfr-dev python3 binutils util-linux"
Invoke-WslChecked -Command $install -AsRoot

Write-Host "[2/4] Rebuilding the independent library..." -ForegroundColor Yellow
Invoke-WslChecked -Command "cd $qRoot && find . -type f -exec touch {} + && make clean && make -j`$(nproc) all bin/benchmark_special_functions bin/example_special_functions"

Write-Host "[3/4] Running differential, cache, alias, range, and independence tests..." -ForegroundColor Yellow
if (-not $SkipTests) {
    Invoke-WslChecked -Command "cd $qRoot && make test && make audit"
} else {
    Invoke-WslChecked -Command "cd $qRoot && make audit"
}

Write-Host "[4/4] Running correctness-gated development comparisons..." -ForegroundColor Yellow
Invoke-WslChecked -Command "cd $qRoot && python3 scripts/run_special_functions.py --root . --out results/special_functions --bits '$Bits' --repetitions $Repetitions"

$resultRoot = Join-Path $root "results\special_functions"
Write-Host ""
Write-Host "SPECIAL-FUNCTION BLOCK COMPLETE" -ForegroundColor Green
Write-Host "Report:       $(Join-Path $resultRoot 'REPORT.md')"
Write-Host "Measurements: $(Join-Path $resultRoot 'benchmark.csv')"
Write-Host "Environment:  $(Join-Path $resultRoot 'environment.json')"
Write-Host ""
Get-Content -LiteralPath (Join-Path $resultRoot "REPORT.md")
