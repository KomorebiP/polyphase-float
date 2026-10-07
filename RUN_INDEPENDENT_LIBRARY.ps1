param(
    [string]$Bits = "100000,1000000,5000000",
    [int]$Repetitions = 9,
    [int]$Workers = 4,
    [switch]$SkipSanitizers
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Convert-ToWslPath {
    param([Parameter(Mandatory = $true)][string]$WindowsPath)
    $full = [System.IO.Path]::GetFullPath($WindowsPath)
    if ($full -match '^([A-Za-z]):\\(.*)$') {
        $drive = $Matches[1].ToLowerInvariant()
        $tail = $Matches[2].Replace('\', '/')
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
if ($LASTEXITCODE -ne 0) { throw "WSL is unavailable." }

Write-Host ""
Write-Host "Polyphase independent floating arithmetic" -ForegroundColor Cyan

Write-Host "[1/10] Installing WSL dependencies..." -ForegroundColor Yellow
$install = "export DEBIAN_FRONTEND=noninteractive; command -v apt-get >/dev/null 2>&1 || { echo 'Ubuntu/Debian WSL is required.' >&2; exit 2; }; apt-get update -qq && apt-get install -y -qq build-essential make cmake pkg-config libgmp-dev libmpfr-dev python3 binutils"
Invoke-WslChecked -Command $install -AsRoot

Write-Host "[2/10] Building and running the independent test suite..." -ForegroundColor Yellow
Invoke-WslChecked -Command "cd $qRoot && find . -type f -exec touch {} + && make clean && make -j`$(nproc) all test audit examples bin/benchmark_object bin/benchmark_arithmetic bin/benchmark_assignment_conversion bin/benchmark_utilities_remainder bin/benchmark_range_roots_divplan bin/benchmark_constants"

Write-Host "[3/10] Running an independent CMake build..." -ForegroundColor Yellow
Invoke-WslChecked -Command "cd $qRoot && rm -rf cmake-build && cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release && cmake --build cmake-build -j`$(nproc) && ctest --test-dir cmake-build --output-on-failure"

Write-Host "[4/10] Validating pkg-config and staged installation..." -ForegroundColor Yellow
Invoke-WslChecked -Command "cd $qRoot && pkg-config --validate ./polyphase-float.pc && rm -rf stage && make DESTDIR=`$PWD/stage install"

if (-not $SkipSanitizers) {
    Write-Host "[5/10] Running AddressSanitizer and UndefinedBehaviorSanitizer..." -ForegroundColor Yellow
    Invoke-WslChecked -Command "cd $qRoot && make sanitize && make -j`$(nproc) all test audit examples bin/benchmark_object bin/benchmark_arithmetic bin/benchmark_assignment_conversion bin/benchmark_utilities_remainder bin/benchmark_range_roots_divplan bin/benchmark_constants"
} else {
    Write-Host "[5/10] Sanitizers skipped by request." -ForegroundColor DarkYellow
}

Write-Host "[6/10] Running object and independent-arithmetic benchmarks..." -ForegroundColor Yellow
Invoke-WslChecked -Command "cd $qRoot && python3 benchmarks/run_benchmark.py --root . --bits '$Bits' --repetitions $Repetitions && python3 benchmarks/run_arithmetic_benchmark.py --root . --bits '$Bits' --repetitions $Repetitions --workers $Workers"

Write-Host "[7/10] Benchmarking initialization, assignment, and conversion..." -ForegroundColor Yellow
Invoke-WslChecked -Command "cd $qRoot && python3 benchmarks/run_assignment_conversion_benchmark.py --root . --bits '$Bits' --repetitions $Repetitions"


Write-Host "[8/10] Benchmarking comparison, integer utilities, and remainders..." -ForegroundColor Yellow
Invoke-WslChecked -Command "cd $qRoot && python3 benchmarks/run_utilities_remainder_benchmark.py --root . --bits '$Bits' --repetitions $Repetitions"

Write-Host "[9/10] Benchmarking range control, roots, and reusable division..." -ForegroundColor Yellow
Invoke-WslChecked -Command "cd $qRoot && python3 benchmarks/run_range_roots_divplan_benchmark.py --root . --bits '$Bits' --repetitions $Repetitions --workers $Workers"

Write-Host "[10/10] Benchmarking independently certified constants..." -ForegroundColor Yellow
Invoke-WslChecked -Command "cd $qRoot && python3 benchmarks/run_constants_benchmark.py --root . --out results/constants --bits '$Bits' --workers $Workers --repeats $Repetitions --bootstrap-draws 20000"

$resultRoot = Join-Path $root "results"
Write-Host ""
Write-Host "INDEPENDENT FLOATING ARITHMETIC VALIDATION COMPLETE" -ForegroundColor Green
Write-Host "Object report:     $(Join-Path $resultRoot 'REPORT.md')"
Write-Host "Arithmetic report: $(Join-Path $resultRoot 'independent_arithmetic\REPORT.md')"
Write-Host "Assignment report: $(Join-Path $resultRoot 'assignment_conversion\REPORT.md')"
Write-Host "Utilities report:  $(Join-Path $resultRoot 'utilities_remainder\REPORT.md')"
Write-Host "Roots/division:    $(Join-Path $resultRoot 'range_roots_division\REPORT.md')"
Write-Host "Constants:         $(Join-Path $resultRoot 'constants\REPORT.md')"
Write-Host ""
Get-Content -LiteralPath (Join-Path $resultRoot "independent_arithmetic\REPORT.md")
