#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Run the correctness-gated I/O and formatting benchmark."""
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
    if not values or any(value < 2 for value in values):
        raise ValueError("precisions must be integers of at least 2 bits")
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
    parser.add_argument("--out", default="results/io_formatting")
    parser.add_argument("--bits", default="10000,100000,1000000")
    parser.add_argument("--repetitions", type=int, default=9)
    args = parser.parse_args()

    root = Path(args.root).resolve()
    out = (root / args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    bits_values = parse_bits(args.bits)
    if args.repetitions < 1:
        raise SystemExit("repetitions must be positive")
    executable = root / "bin" / "benchmark_io_formatting"
    if not executable.exists():
        raise SystemExit(f"missing benchmark executable: {executable}")

    rows: list[dict[str, str]] = []
    for bits in bits_values:
        print(f"benchmarking {bits:,} bits...", flush=True)
        output = run([str(executable), str(bits), str(args.repetitions)], root)
        rows.extend(dict(row) for row in csv.DictReader(output.splitlines()))

    fields = [
        "bits", "operation", "ppf_median_s", "mpfr_median_s",
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
        "interchange_note": (
            "PPFIF01 and MPFR FPIF are independently timed functional "
            "round trips; their byte formats are intentionally different."
        ),
    }
    (out / "environment.json").write_text(
        json.dumps(environment, indent=2) + "\n", encoding="utf-8"
    )

    lines = [
        "# I/O and formatting development benchmark",
        "",
        "Each text row compares one Polyphase call with one MPFR call at the same binary precision. The executable checks the generated text or reconstructed numerical value before retaining the timing.",
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
        "`out_str_128_digits` and `asprintf_128_significant` are directly comparable text operations.",
        "",
        "`portable_interchange_roundtrip` compares end-to-end serialization and restoration, but the two libraries use different experimental interchange byte formats. Polyphase adds a canonical header and CRC-32 integrity check, so this row is a functional throughput comparison rather than a byte-format comparison.",
        "",
        "The formatting implementation is correctness-first. It supports mixed standard C, GMP, precision, and independent floating-point arguments, including embedded null characters and exact `snprintf` length semantics.",
        "",
    ])
    report = out / "REPORT.md"
    report.write_text("\n".join(lines), encoding="utf-8")
    print(report)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
