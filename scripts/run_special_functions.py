#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Run the correctness-gated special-function benchmark and write a local report."""
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
    for item in text.split(","):
        item = item.strip()
        if not item:
            continue
        value = int(item)
        if value < 2:
            raise ValueError("every precision must be at least 2 bits")
        values.append(value)
    if not values:
        raise ValueError("no precisions supplied")
    return values


def run(cmd: list[str], cwd: Path) -> str:
    completed = subprocess.run(
        cmd,
        cwd=cwd,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if completed.stderr:
        print(completed.stderr, end="", file=os.sys.stderr)
    return completed.stdout


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--out", default="results/special_functions")
    parser.add_argument("--bits", default="64,128,256")
    parser.add_argument("--repetitions", type=int, default=5)
    args = parser.parse_args()

    root = Path(args.root).resolve()
    out = (root / args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    bits_values = parse_bits(args.bits)
    if args.repetitions < 1:
        raise SystemExit("repetitions must be positive")

    executable = root / "bin" / "benchmark_special_functions"
    if not executable.exists():
        raise SystemExit(f"missing benchmark executable: {executable}")

    rows: list[dict[str, str]] = []
    for bits in bits_values:
        print(f"benchmarking {bits:,} bits...", flush=True)
        text = run([str(executable), str(bits), str(args.repetitions)], root)
        for row in csv.DictReader(text.splitlines()):
            rows.append(dict(row))

    csv_path = out / "benchmark.csv"
    fieldnames = [
        "bits",
        "operation",
        "ppf_median_s",
        "mpfr_median_s",
        "speedup_mpfr_over_ppf",
    ]
    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    environment = {
        "platform": platform.platform(),
        "python": platform.python_version(),
        "bits": bits_values,
        "repetitions": args.repetitions,
    }
    (out / "environment.json").write_text(
        json.dumps(environment, indent=2) + "\n", encoding="utf-8"
    )

    lines = [
        "# Special-function development benchmark",
        "",
        "Each row compares one independent Polyphase call with one MPFR call at the same binary precision and nearest-even rounding. The benchmark checks the returned value and ternary direction before recording the timing.",
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
    lines.extend(
        [
            "",
            "## Interpretation",
            "",
            "This is a correctness-first reference implementation. The present special-function algorithms use directed interval arithmetic and conservative series, recurrences, reflection formulas, and precision escalation. The benchmark is included to identify the next speed targets; it is not presented as a performance win over MPFR.",
            "",
            "The explicit speed-return backlog is recorded in `SPEED_RETURN_BACKLOG.md`. It includes both this special-function layer and the previously deferred inverse-angle and dense trigonometric work.",
            "",
        ]
    )
    report_path = out / "REPORT.md"
    report_path.write_text("\n".join(lines), encoding="utf-8")
    print(report_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
