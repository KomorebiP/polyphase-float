#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Run correctness-gated inverse-angle and dense-phase speed comparisons."""
from __future__ import annotations

import argparse
import csv
import json
import os
import platform
import subprocess
from pathlib import Path
from typing import Iterable

FIELDNAMES = [
    "bits", "repetitions", "inner_loops", "operation",
    "ppf_median_s", "mpfr_median_s", "mpfr_over_ppf",
]
FOCUSED = {
    "sin_0.3", "cos_0.3", "tan_0.3", "sinpi_0.3",
    "atan_0.3", "atan2_0.75_-0.25",
    "asin_0.3", "acos_0.3", "asinh_0.3", "atanh_0.3",
}


def parse_csv_ints(text: str, minimum: int = 1) -> list[int]:
    values: list[int] = []
    for raw in text.split(","):
        raw = raw.strip()
        if not raw:
            continue
        value = int(raw)
        if value < minimum:
            raise ValueError(f"values must be at least {minimum}")
        values.append(value)
    if not values:
        raise ValueError("no values supplied")
    return values


def run_checked(cmd: list[str], cwd: Path) -> str:
    completed = subprocess.run(
        cmd, cwd=cwd, check=True, text=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    if completed.stderr:
        print(completed.stderr, end="", file=os.sys.stderr)
    return completed.stdout


def read_cpu_description() -> str:
    try:
        return subprocess.run(
            ["lscpu"], check=True, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
        ).stdout
    except (OSError, subprocess.CalledProcessError):
        return ""


def write_rows(path: Path, rows: Iterable[dict[str, str]]) -> None:
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=FIELDNAMES)
        writer.writeheader()
        writer.writerows(rows)


def format_seconds(value: float) -> str:
    if value < 1e-6:
        return f"{value * 1e9:.2f} ns"
    if value < 1e-3:
        return f"{value * 1e6:.2f} us"
    return f"{value:.6f} s"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--out", default="results/inverse_angle_speedups_local")
    parser.add_argument("--bits", default="20000,50000,100000")
    parser.add_argument("--repetitions", type=int, default=9)
    parser.add_argument("--inner-loops", type=int, default=0,
                        help="0 lets the C benchmark choose by precision")
    args = parser.parse_args()

    root = Path(args.root).resolve()
    out = (root / args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    bits_values = parse_csv_ints(args.bits, minimum=2)
    if args.repetitions < 1:
        raise SystemExit("repetitions must be positive")
    if args.inner_loops < 0:
        raise SystemExit("inner-loops cannot be negative")

    executable = root / "bin" / "benchmark_trig_hyperbolic"
    if not executable.exists():
        raise SystemExit(f"missing benchmark executable: {executable}")

    rows: list[dict[str, str]] = []
    for bits in bits_values:
        print(f"benchmarking {bits:,} bits...", flush=True)
        cmd = [str(executable), str(bits), str(args.repetitions)]
        if args.inner_loops:
            cmd.append(str(args.inner_loops))
        text = run_checked(cmd, root)
        case_rows = [dict(row) for row in csv.DictReader(text.splitlines())]
        if not case_rows:
            raise RuntimeError(f"benchmark returned no rows at {bits} bits")
        for row in case_rows:
            if set(FIELDNAMES) - set(row):
                raise RuntimeError(f"benchmark row has missing fields: {row}")
        rows.extend(case_rows)
        write_rows(out / f"benchmark_{bits}.csv", case_rows)
        write_rows(out / "benchmark.csv", rows)

    focused = [row for row in rows if row["operation"] in FOCUSED]
    write_rows(out / "focused.csv", focused)

    environment = {
        "platform": platform.platform(),
        "python": platform.python_version(),
        "cpu_count": os.cpu_count(),
        "bits": bits_values,
        "repetitions": args.repetitions,
        "inner_loops": args.inner_loops,
        "lscpu": read_cpu_description(),
        "comparison_contract": {
            "precision": "identical binary destination precision",
            "rounding": "nearest, ties to even",
            "correctness_gate": "value and ternary direction checked before timing is retained",
            "workers": "one Polyphase call versus one MPFR call",
        },
    }
    (out / "environment.json").write_text(
        json.dumps(environment, indent=2) + "\n", encoding="utf-8"
    )

    lines = [
        "# Inverse-angle and dense-phase speed benchmark",
        "",
        "Each row compares one independent Polyphase call with one MPFR call at the same binary precision and nearest-even rounding. The benchmark rejects a case before timing if either the value or ternary direction disagrees.",
        "",
        "A ratio above one favors Polyphase. A ratio below one means MPFR is faster.",
        "",
        "| Bits | Operation | Polyphase | MPFR | MPFR / Polyphase |",
        "|---:|---|---:|---:|---:|",
    ]
    for row in focused:
        ppf = float(row["ppf_median_s"])
        mpfr = float(row["mpfr_median_s"])
        lines.append(
            f"| {int(row['bits']):,} | `{row['operation']}` | "
            f"{format_seconds(ppf)} | {format_seconds(mpfr)} | "
            f"{float(row['mpfr_over_ppf']):.3f}x |"
        )

    lines.extend([
        "",
        "## Interpretation",
        "",
        "This branch adds a cubically convergent certified inverse-angle correction, shared exact-ratio atan2 reduction, direct fixed-point tangent enclosures inside the correction loop, and a density-aware sine/cosine block planner. The fast result is accepted only when the interval proves the rounded value and ternary direction; otherwise the library returns to the existing independent certified path.",
        "",
        "The comparison with MPFR remains an equal-precision one-thread operation comparison. The package's same-host before-and-after report separately measures the gain relative to the preceding independent implementation.",
        "",
    ])
    report = out / "REPORT.md"
    report.write_text("\n".join(lines), encoding="utf-8")
    print(report)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
