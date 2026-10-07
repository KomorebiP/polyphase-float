#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Run correctness-gated benchmarks for the completed arithmetic family."""
from __future__ import annotations

import argparse
import csv
import json
import os
import platform
import subprocess
from pathlib import Path


def parse_bits(text: str) -> list[int]:
    values = [int(part.strip()) for part in text.split(",") if part.strip()]
    if not values or any(value < 64 for value in values):
        raise ValueError("precisions must be integers of at least 64 bits")
    return values


def run(cmd: list[str], cwd: Path) -> str:
    completed = subprocess.run(
        cmd, cwd=cwd, check=True, text=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    if completed.stderr:
        print(completed.stderr, end="", file=os.sys.stderr)
    return completed.stdout


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--out", default="results/arithmetic_completion")
    parser.add_argument("--bits", default="1000,10000,100000")
    parser.add_argument("--repetitions", type=int, default=9)
    args = parser.parse_args()

    root = Path(args.root).resolve()
    out = (root / args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    bits_values = parse_bits(args.bits)
    if args.repetitions < 1:
        raise SystemExit("repetitions must be positive")
    executable = root / "bin" / "benchmark_arithmetic_completion"
    if not executable.exists():
        raise SystemExit(f"missing benchmark executable: {executable}")

    rows: list[dict[str, str]] = []
    for bits in bits_values:
        print(f"benchmarking {bits:,} bits...", flush=True)
        output = run([str(executable), str(bits), str(args.repetitions)], root)
        rows.extend(dict(row) for row in csv.DictReader(output.splitlines()))

    fields = [
        "bits", "operation", "inner", "ppf_median_s", "mpfr_median_s",
        "speedup_mpfr_over_ppf",
    ]
    with (out / "benchmark.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)

    environment = {
        "platform": platform.platform(),
        "python": platform.python_version(),
        "bits": bits_values,
        "repetitions": args.repetitions,
        "contract": (
            "Each timed row is checked first for exact ppf_t value equality "
            "and matching ternary direction against MPFR at the same binary "
            "precision and nearest-even rounding."
        ),
    }
    (out / "environment.json").write_text(
        json.dumps(environment, indent=2) + "\n", encoding="utf-8"
    )

    lines = [
        "# Completed arithmetic-family benchmark",
        "",
        "Each row compares one independent Polyphase operation with the corresponding MPFR operation at the same destination precision and nearest-even rounding. The benchmark verifies the numerical result and ternary direction before retaining the timing.",
        "",
        "A ratio above one favors Polyphase.",
        "",
        "| Bits | Operation | Polyphase (s) | MPFR (s) | MPFR / Polyphase |",
        "|---:|---|---:|---:|---:|",
    ]
    for row in rows:
        lines.append(
            f"| {int(row['bits']):,} | `{row['operation']}` | "
            f"{float(row['ppf_median_s']):.9f} | "
            f"{float(row['mpfr_median_s']):.9f} | "
            f"{float(row['speedup_mpfr_over_ppf']):.3f}× |"
        )
    lines.extend([
        "",
        "## Interpretation",
        "",
        "The scalar wrappers are primarily a public-surface completion milestone. Integer and double variants lower into the existing independent binary kernels, while rational variants retain exact GMP-rational arithmetic until one final binary rounding. The extra temporary construction makes several small and middle-precision scalar rows slower than MPFR.",
        "",
        "`mul_z` begins to benefit from the existing large-integer multiplication path at high precision. `fmma` is near parity in the largest development row because the two exact products are fused before the single final rounding.",
        "",
        "`fac_ui` delegates the exact integer factorial to GMP and performs only one final independent floating-point rounding. That specialized path is substantially faster than the installed MPFR factorial implementation in the measured rows. The factorial argument is selected from the target precision so that the exact integer has the same general scale as the destination.",
        "",
        "These measurements are development-machine evidence, not portable crossover claims. The supplied launcher reruns them on the target processor and GMP/MPFR build.",
        "",
    ])
    report = out / "REPORT.md"
    report.write_text("\n".join(lines), encoding="utf-8")
    print(report)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
