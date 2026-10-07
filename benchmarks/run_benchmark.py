#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

from __future__ import annotations
import argparse, csv, json, pathlib, platform, subprocess

p = argparse.ArgumentParser()
p.add_argument("--root", default=".")
p.add_argument("--bits", default="100000,1000000,5000000,10000000")
p.add_argument("--repetitions", type=int, default=31)
a = p.parse_args()
root = pathlib.Path(a.root).resolve()
out = root / "results"
out.mkdir(exist_ok=True)
rows = []
for bits in [int(x) for x in a.bits.split(",") if x.strip()]:
    proc = subprocess.run([str(root / "bin/benchmark_object"), str(bits),
                           str(a.repetitions)], text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          check=True)
    row = json.loads(proc.stdout.strip().splitlines()[-1])
    rows.append(row)
    print(json.dumps(row), flush=True)
with (out / "object_benchmark.jsonl").open("w", encoding="utf-8") as f:
    for row in rows: f.write(json.dumps(row) + "\n")
with (out / "object_benchmark.csv").open("w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0]))
    w.writeheader(); w.writerows(rows)
lines = [
    "# Floating object benchmark", "",
    "The copy rows measure one exact same-precision value copy. The boundary row imports and exports through MPFR and is included to show the cost that future native arithmetic avoids.", "",
    "| Precision (bits) | Polyphase copy (s) | MPFR copy (s) | Copy ratio | MPFR boundary roundtrip (s) |",
    "|---:|---:|---:|---:|---:|",
]
for row in rows:
    ratio = row["mpfr_copy_seconds"] / row["ppf_copy_seconds"]
    lines.append(f'| {row["bits"]:,} | {row["ppf_copy_seconds"]:.9f} | {row["mpfr_copy_seconds"]:.9f} | {ratio:.3f}x | {row["mpfr_boundary_roundtrip_seconds"]:.9f} |')
(out / "REPORT.md").write_text("\n".join(lines)+"\n", encoding="utf-8")
(out / "environment.json").write_text(json.dumps({
    "platform": platform.platform(),
    "python": platform.python_version(),
    "bits": [r["bits"] for r in rows],
    "repetitions": a.repetitions,
}, indent=2)+"\n", encoding="utf-8")
