#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Run the correctness-gated trigonometric/hyperbolic benchmark and write reports."""
from __future__ import annotations

import argparse
import csv
import json
import os
import platform
import subprocess
from pathlib import Path


def parse_bits(text: str) -> list[int]:
    values: list[int] = []
    for part in text.split(","):
        part = part.strip()
        if not part:
            continue
        value = int(part)
        if value < 2:
            raise ValueError("every precision must be at least 2 bits")
        values.append(value)
    if not values:
        raise ValueError("no precisions supplied")
    return values


def run(cmd: list[str], cwd: Path) -> str:
    completed = subprocess.run(cmd, cwd=cwd, check=True, text=True,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if completed.stderr:
        print(completed.stderr, end="", file=os.sys.stderr)
    return completed.stdout


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--out", default="results/trig_hyperbolic")
    parser.add_argument("--bits", default="100,1000,5000")
    parser.add_argument("--repetitions", type=int, default=5)
    parser.add_argument("--inner-loops", type=int, default=0,
                        help="0 lets the C benchmark select a precision-dependent value")
    args = parser.parse_args()

    root = Path(args.root).resolve()
    out = (root / args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    bits_values = parse_bits(args.bits)
    if args.repetitions < 1 or args.inner_loops < 0:
        raise SystemExit("repetitions must be positive and inner-loops nonnegative")

    executable = root / "bin" / "benchmark_trig_hyperbolic"
    if not executable.exists():
        raise SystemExit(f"missing benchmark executable: {executable}")

    rows: list[dict[str, str]] = []
    for bits in bits_values:
        cmd = [str(executable), str(bits), str(args.repetitions)]
        if args.inner_loops:
            cmd.append(str(args.inner_loops))
        print(f"benchmarking {bits:,} bits...", flush=True)
        text = run(cmd, root)
        reader = csv.DictReader(text.splitlines())
        for row in reader:
            rows.append(dict(row))

    raw_path = out / "benchmark.csv"
    fieldnames = [
        "bits", "repetitions", "inner_loops", "operation",
        "ppf_median_s", "mpfr_median_s", "mpfr_over_ppf",
    ]
    with raw_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    environment = {
        "platform": platform.platform(),
        "python": platform.python_version(),
        "bits": bits_values,
        "repetitions": args.repetitions,
        "inner_loops": args.inner_loops,
    }
    (out / "environment.json").write_text(json.dumps(environment, indent=2) + "\n",
                                           encoding="utf-8")

    lines = [
        "# Trigonometric and hyperbolic development benchmark",
        "",
        "Each row compares one independent Polyphase call with one MPFR call at the same binary precision and nearest-even rounding. Every result is checked for exact value and ternary-direction agreement before timing.",
        "",
        "A ratio above one favors Polyphase.",
        "",
        "| Bits | Operation | Polyphase (s) | MPFR (s) | MPFR / Polyphase |",
        "|---:|---|---:|---:|---:|",
    ]
    for row in rows:
        lines.append(
            f"| {int(row['bits']):,} | `{row['operation']}` | "
            f"{float(row['ppf_median_s']):.9f} | {float(row['mpfr_median_s']):.9f} | "
            f"{float(row['mpfr_over_ppf']):.3f}× |"
        )
    lines.extend([
        "",
        "## Interpretation",
        "",
        "This is a correctness-first baseline. The present implementation uses rigorous interval argument reduction and conservative Taylor or algebraic reconstruction. It establishes the independent 35-function surface and identifies the performance work still required; it is not presented as a speed win over MPFR.",
        "",
    ])
    report_path = out / "REPORT.md"
    report_path.write_text("\n".join(lines), encoding="utf-8")
    print(report_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
