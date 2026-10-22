param(
    [string]$Bits = "100000,1000000,5000000",
    [int]$Repetitions = 9,
    [int]$Workers = 4,
    [switch]$SkipSanitizers
)
& "$PSScriptRoot\RUN_INDEPENDENT_LIBRARY.ps1" -Bits $Bits -Repetitions $Repetitions -Workers $Workers -SkipSanitizers:$SkipSanitizers
exit $LASTEXITCODE
