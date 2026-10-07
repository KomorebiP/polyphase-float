#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

from __future__ import annotations
import argparse, csv, json, pathlib, platform, subprocess

p = argparse.ArgumentParser()
p.add_argument('--root', default='.')
p.add_argument('--bits', default='100000,1000000,5000000')
p.add_argument('--repetitions', type=int, default=11)
a = p.parse_args()
root = pathlib.Path(a.root).resolve()
out = root / 'results' / 'utilities_remainder'
out.mkdir(parents=True, exist_ok=True)
rows = []
for bits in [int(x) for x in a.bits.split(',') if x.strip()]:
    cp = subprocess.run(
        [str(root/'bin/benchmark_utilities_remainder'), str(bits), str(a.repetitions)],
        text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
    row = json.loads(cp.stdout.strip().splitlines()[-1])
    rows.append(row)
    print(json.dumps(row), flush=True)
with (out/'raw.jsonl').open('w', encoding='utf-8') as f:
    for row in rows: f.write(json.dumps(row) + '\n')
with (out/'benchmark.csv').open('w', newline='', encoding='utf-8') as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0]))
    w.writeheader(); w.writerows(rows)
ops = [
    ('roundeven', 'round to integer, ties even'),
    ('frac', 'fractional part'),
    ('modf', 'integer and fractional parts'),
    ('fmod', 'truncated-quotient remainder'),
    ('remainder', 'nearest-even quotient remainder'),
    ('cmp', 'comparison'),
]
lines = [
    '# Independent utility and remainder benchmark', '',
    'Each row is the median of alternating-order runs after one warmup. '
    'A ratio above 1 favors Polyphase. MPFR is used only by the benchmark oracle.', '',
    '| Bits | Operation | Polyphase (s) | MPFR (s) | MPFR/Polyphase |',
    '|---:|---|---:|---:|---:|',
]
for row in rows:
    for key, label in ops:
        pv = row[f'ppf_{key}_seconds']; mv = row[f'mpfr_{key}_seconds']
        lines.append(f"| {row['bits']:,} | {label} | {pv:.9f} | {mv:.9f} | {mv/pv:.3f}x |")
(out/'REPORT.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
(out/'environment.json').write_text(json.dumps({
    'platform': platform.platform(),
    'python': platform.python_version(),
    'bits': [r['bits'] for r in rows],
    'repetitions': a.repetitions,
}, indent=2) + '\n', encoding='utf-8')
