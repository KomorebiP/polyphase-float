param(
    [string]$Bits = "100,1000,5000",
    [int]$Repetitions = 9,
    [int]$Loops = 1,
    [int]$BootstrapDraws = 20000,
    [switch]$SkipTests,
    [switch]$Fresh
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
Write-Host "Independent correctly rounded logarithms, exponentials, and powers" -ForegroundColor Cyan
Write-Host "[1/3] Installing WSL dependencies..." -ForegroundColor Yellow
$install = "export DEBIAN_FRONTEND=noninteractive; command -v apt-get >/dev/null 2>&1 || { echo 'Ubuntu/Debian WSL is required.' >&2; exit 2; }; apt-get update -qq && apt-get install -y -qq build-essential make cmake pkg-config libgmp-dev libmpfr-dev python3 binutils util-linux"
Invoke-WslChecked -Command $install -AsRoot

Write-Host "[2/3] Rebuilding and validating all 24 transcendental functions..." -ForegroundColor Yellow
$build = "cd $qRoot && find . -type f -exec touch {} + && make clean && make -j`$(nproc) all audit bin/test_transcendental_complete bin/test_transcendental_random bin/test_transcendental_range bin/benchmark_transcendental_method bin/example_transcendentals"
if (-not $SkipTests) { $build += " test" }
Invoke-WslChecked -Command $build

Write-Host "[3/3] Running correctness-gated one-thread comparisons..." -ForegroundColor Yellow
$freshArg = if ($Fresh) { " --fresh" } else { "" }
Invoke-WslChecked -Command "cd $qRoot && python3 scripts/run_transcendental_benchmark.py --root . --out results/transcendentals --bits '$Bits' --repetitions $Repetitions --loops $Loops --bootstrap-draws $BootstrapDraws$freshArg"

$resultRoot = Join-Path $root "results\transcendentals"
Write-Host ""
Write-Host "TRANSCENDENTAL BLOCK COMPLETE" -ForegroundColor Green
Write-Host "Report:  $(Join-Path $resultRoot 'REPORT.md')"
Write-Host "Summary: $(Join-Path $resultRoot 'summary.csv')"
Write-Host "Paired:  $(Join-Path $resultRoot 'paired_comparisons.csv')"
Write-Host "Raw:     $(Join-Path $resultRoot 'raw.csv')"
Write-Host ""
Get-Content -LiteralPath (Join-Path $resultRoot "REPORT.md")
