param(
    [string]$Bits = "100000,1000000,5000000",
    [int]$Repetitions = 9,
    [int]$Workers = 0,
    [int]$Jobs = 0,
    [int]$InnerRepetitions = 1,
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
if ($LASTEXITCODE -ne 0) { throw "Ubuntu WSL is unavailable." }

Write-Host ""
Write-Host "Polyphase independent floating-point deep speed benchmark" -ForegroundColor Cyan

Write-Host ""
Write-Host "[1/4] Installing WSL dependencies..." -ForegroundColor Yellow
$install = "export DEBIAN_FRONTEND=noninteractive; command -v apt-get >/dev/null 2>&1 || { echo 'Ubuntu/Debian WSL is required.' >&2; exit 2; }; apt-get update -qq && apt-get install -y -qq build-essential make cmake pkg-config libgmp-dev libmpfr-dev python3 time util-linux binutils"
Invoke-WslChecked -Command $install -AsRoot

$physicalText = & wsl.exe -e bash -lc "lscpu -p=CORE,SOCKET | grep -v '^#' | sort -u | wc -l" | Select-Object -First 1
$physical = 1
if (-not [int]::TryParse(([string]$physicalText).Trim(), [ref]$physical)) { $physical = 1 }
if ($Workers -le 0) { $Workers = $physical }
$Workers = [Math]::Max(1, [Math]::Min($Workers, $physical))
if ($Jobs -le 0) { $Jobs = $Workers }
$Jobs = [Math]::Max(1, $Jobs)

Write-Host ""
Write-Host "[2/4] Rebuilding for this CPU..." -ForegroundColor Yellow
Invoke-WslChecked -Command "cd $qRoot && find . -type f -exec touch {} + && make clean all bin/benchmark_hotpath_method bin/benchmark_recsqrt_matched"

Write-Host ""
Write-Host "[3/4] Running correctness and independence checks..." -ForegroundColor Yellow
if (-not $SkipTests) {
    Invoke-WslChecked -Command "cd $qRoot && make test && make audit"
} else {
    Invoke-WslChecked -Command "cd $qRoot && make audit"
}

Write-Host ""
Write-Host "[4/4] Running paired speed measurements..." -ForegroundColor Yellow
$freshArg = if ($Fresh) { " --fresh" } else { "" }
Invoke-WslChecked -Command "cd $qRoot && python3 scripts/run_deep_speedups.py --root . --out results/deep_speedups --bits '$Bits' --repetitions $Repetitions --workers $Workers --jobs $Jobs --inner-repetitions $InnerRepetitions --bootstrap-draws $BootstrapDraws$freshArg"

$resultRoot = Join-Path $root "results\deep_speedups"
Write-Host ""
Write-Host "DEEP SPEED BENCHMARK COMPLETE" -ForegroundColor Green
Write-Host "Physical cores used for matched batches: $Workers"
Write-Host "Independent jobs per matched batch: $Jobs"
Write-Host "Report:  $(Join-Path $resultRoot 'REPORT.md')"
Write-Host "Summary: $(Join-Path $resultRoot 'summary.csv')"
Write-Host "Paired:  $(Join-Path $resultRoot 'paired_comparisons.csv')"
Write-Host "Raw:     $(Join-Path $resultRoot 'raw.csv')"
Write-Host ""
Get-Content -LiteralPath (Join-Path $resultRoot 'REPORT.md')
