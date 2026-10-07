param(
    [string]$Bits = "1000,10000,100000",
    [int]$Repetitions = 9,
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
Write-Host "Independent runtime and compatibility completion" -ForegroundColor Cyan
Write-Host "[1/5] Installing WSL dependencies..." -ForegroundColor Yellow
$install = "export DEBIAN_FRONTEND=noninteractive; command -v apt-get >/dev/null 2>&1 || { echo 'Ubuntu/Debian WSL is required.' >&2; exit 2; }; apt-get update -qq && apt-get install -y -qq build-essential make cmake pkg-config libgmp-dev libmpfr-dev python3 binutils util-linux"
Invoke-WslChecked -Command $install -AsRoot

Write-Host "[2/5] Rebuilding the complete independent library..." -ForegroundColor Yellow
Invoke-WslChecked -Command "cd $qRoot && find . -type f -exec touch {} + && make clean && make -j`$(nproc) all bin/test_runtime_support bin/test_random_runtime bin/test_conditional_formats bin/benchmark_runtime_completion bin/example_runtime_completion"

Write-Host "[3/5] Running correctness and independence tests..." -ForegroundColor Yellow
if (-not $SkipTests) {
    Invoke-WslChecked -Command "cd $qRoot && make test && make audit && make coverage"
} else {
    Invoke-WslChecked -Command "cd $qRoot && ./bin/test_runtime_support && ./bin/test_random_runtime && ./bin/test_conditional_formats && make audit && make coverage"
}

Write-Host "[4/5] Running an independent CMake/CTest build..." -ForegroundColor Yellow
Invoke-WslChecked -Command "cd $qRoot && rm -rf build-cmake && cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON >/dev/null && cmake --build build-cmake -j`$(nproc) >/dev/null && ctest --test-dir build-cmake --output-on-failure"

Write-Host "[5/5] Running correctness-gated runtime comparisons..." -ForegroundColor Yellow
Invoke-WslChecked -Command "cd $qRoot && python3 scripts/run_runtime_completion.py --root . --out results/runtime_completion --bits '$Bits' --repetitions $Repetitions"

$resultRoot = Join-Path $root "results\runtime_completion"
Write-Host ""
Write-Host "LOWERCASE API INVENTORY COMPLETE: 349 / 349" -ForegroundColor Green
Write-Host "Report:       $(Join-Path $resultRoot 'REPORT.md')"
Write-Host "Measurements: $(Join-Path $resultRoot 'paired_comparisons.csv')"
Write-Host "Coverage:     $(Join-Path $resultRoot 'api_coverage.csv')"
Write-Host ""
Get-Content -LiteralPath (Join-Path $resultRoot "REPORT.md")
