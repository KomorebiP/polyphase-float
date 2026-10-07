#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

from __future__ import annotations

import argparse
import csv
import json
import pathlib
import platform
import subprocess
from collections import defaultdict


def run(cmd: list[str]) -> str:
    p = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    if p.returncode != 0:
        raise SystemExit(f"command failed ({p.returncode}): {' '.join(cmd)}\n{p.stdout}\n{p.stderr}")
    return p.stdout


parser = argparse.ArgumentParser()
parser.add_argument("--root", default=".")
parser.add_argument("--bits", default="100000,1000000,5000000")
parser.add_argument("--repetitions", type=int, default=9)
parser.add_argument("--workers", type=int, default=4)
args = parser.parse_args()

root = pathlib.Path(args.root).resolve()
out = root / "results" / "independent_arithmetic"
out.mkdir(parents=True, exist_ok=True)

rows: list[dict[str, object]] = []
for bits in [int(x) for x in args.bits.split(",") if x.strip()]:
    text = run([
        str(root / "bin" / "benchmark_arithmetic"),
        str(bits), str(args.repetitions), str(args.workers),
    ])
    for line in text.splitlines():
        parts = line.strip().split(",")
        if len(parts) != 3 or not parts[0].isdigit():
            continue
        rows.append({
            "bits": int(parts[0]),
            "method": parts[1],
            "median_seconds": float(parts[2]),
            "workers": args.workers if parts[1].endswith("parallel") else 1,
        })
    print(f"completed {bits:,} bits", flush=True)

if not rows:
    raise SystemExit("benchmark returned no rows")

with (out / "raw.csv").open("w", newline="", encoding="utf-8") as f:
    writer = csv.DictWriter(f, fieldnames=list(rows[0]))
    writer.writeheader(); writer.writerows(rows)

index = {(int(r["bits"]), str(r["method"])): float(r["median_seconds"]) for r in rows}
comparisons: list[dict[str, object]] = []
for bits in sorted({int(r["bits"]) for r in rows}):
    specs = [
        ("add_1t", "ppf_add", "mpfr_add"),
        ("multiply_1t", "ppf_mul_1t", "mpfr_mul"),
        ("multiply_parallel_vs_mpfr_1t", "ppf_mul_parallel", "mpfr_mul"),
        ("division_1t", "ppf_div", "mpfr_div"),
        ("sqrt_1t", "ppf_sqrt", "mpfr_sqrt"),
        ("fma_1t", "ppf_fma", "mpfr_fma"),
        ("dot_1t", "ppf_dot_1t", "mpfr_dot"),
        ("dot_parallel_vs_mpfr_1t", "ppf_dot_parallel", "mpfr_dot"),
    ]
    for name, ppf, mpfr in specs:
        if (bits, ppf) not in index or (bits, mpfr) not in index:
            continue
        comparisons.append({
            "bits": bits,
            "comparison": name,
            "speedup": index[(bits, mpfr)] / index[(bits, ppf)],
            "ppf_seconds": index[(bits, ppf)],
            "mpfr_seconds": index[(bits, mpfr)],
        })

with (out / "comparisons.csv").open("w", newline="", encoding="utf-8") as f:
    writer = csv.DictWriter(f, fieldnames=list(comparisons[0]))
    writer.writeheader(); writer.writerows(comparisons)

lines = [
    "# Independent floating arithmetic benchmark",
    "",
    "The Polyphase rows use the independent `ppf_t` representation for the entire timed operation. MPFR is used only as the comparison implementation outside the Polyphase timing. No MPFR import or export occurs inside a timed Polyphase arithmetic call.",
    "",
    f"Measured repetitions per row: {args.repetitions}. Requested Polyphase workers for parallel rows: {args.workers}.",
    "",
    "| Precision | Method | Workers | Median (s) |",
    "|---:|---|---:|---:|",
]
for r in rows:
    lines.append(f"| {int(r['bits']):,} | {r['method']} | {r['workers']} | {float(r['median_seconds']):.9f} |")
lines += [
    "",
    "## Comparisons",
    "",
    "A speedup above one favors the independent Polyphase operation. Parallel rows are practical single-call latency comparisons against one ordinary MPFR call, not equal-core comparisons.",
    "",
    "| Precision | Comparison | Speedup |",
    "|---:|---|---:|",
]
for r in comparisons:
    lines.append(f"| {int(r['bits']):,} | {r['comparison']} | {float(r['speedup']):.3f}x |")
lines += [
    "",
    "## Interpretation",
    "",
    "The benchmark is a development diagnostic, not a universal performance claim. Multiplication and exact product reductions are already near the intended performance regime. Addition, division, and square root currently favor correctness and complete independent semantics over specialized tuning and remain the principal optimization targets.",
]
(out / "REPORT.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
(out / "environment.json").write_text(json.dumps({
    "platform": platform.platform(),
    "python": platform.python_version(),
    "bits": sorted({int(r["bits"]) for r in rows}),
    "repetitions": args.repetitions,
    "parallel_workers": args.workers,
    "timing_contract": "independent ppf_t arithmetic; MPFR outside Polyphase timed region",
}, indent=2) + "\n", encoding="utf-8")
print((out / "REPORT.md").read_text(encoding="utf-8"))
