POLYPHASE INDEPENDENT FLOATING-POINT SPEEDUPS

Run from PowerShell after extraction:

  powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPEEDUPS.ps1

Default benchmark:
  100,000, 1,000,000, and 5,000,000 bits
  9 randomized outer repetitions
  5 inner samples per measured method
  automatically detected physical-core count
  exact result verification against MPFR after timing and before each row is retained

Outputs:
  results\speedups\REPORT.md
  results\speedups\summary.csv
  results\speedups\paired_comparisons.csv
  results\speedups\raw.csv

See SPEEDUP_REPORT.md and FAILED_SPEED_EXPERIMENTS.md for the retained changes and tested boundaries.
