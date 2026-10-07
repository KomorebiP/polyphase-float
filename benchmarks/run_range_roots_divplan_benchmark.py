#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

from __future__ import annotations
import argparse, csv, json, pathlib, platform, subprocess

p=argparse.ArgumentParser()
p.add_argument('--root',default='.')
p.add_argument('--bits',default='100000,1000000,5000000')
p.add_argument('--repetitions',type=int,default=5)
p.add_argument('--workers',type=int,default=4)
a=p.parse_args();root=pathlib.Path(a.root).resolve();out=root/'results'/'range_roots_division';out.mkdir(parents=True,exist_ok=True)
rows=[]
for bits in [int(x) for x in a.bits.split(',') if x.strip()]:
    q=subprocess.run([str(root/'bin'/'benchmark_range_roots_divplan'),str(bits),str(a.repetitions),str(a.workers)],text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    if q.returncode: raise SystemExit(f'benchmark failed {bits}:\n{q.stdout}\n{q.stderr}')
    for line in q.stdout.splitlines():
        z=line.strip().split(',')
        if len(z)==3 and z[0].isdigit(): rows.append({'bits':int(z[0]),'method':z[1],'median_seconds':float(z[2])})
    print(f'completed {bits:,} bits',flush=True)
with (out/'raw.csv').open('w',newline='',encoding='utf-8') as f:
    w=csv.DictWriter(f,fieldnames=['bits','method','median_seconds']);w.writeheader();w.writerows(rows)
idx={(r['bits'],r['method']):r['median_seconds'] for r in rows}
comps=[]
for bits in sorted({r['bits'] for r in rows}):
    for name,x,y in [
        ('cbrt_1t','ppf_cbrt','mpfr_cbrt'),('root7_1t','ppf_root7','mpfr_root7'),('rec_sqrt_1t','ppf_rec_sqrt','mpfr_rec_sqrt'),('hypot_1t','ppf_hypot','mpfr_hypot'),('direct_division_1t','ppf_div_direct','mpfr_div'),('planned_division_1t','ppf_div_planned','mpfr_div'),('planned_batch_equal_workers','ppf_plan_batch_matched','mpfr_batch_matched')]:
        if (bits,x) in idx and (bits,y) in idx: comps.append({'bits':bits,'comparison':name,'speedup':idx[(bits,y)]/idx[(bits,x)],'ppf_seconds':idx[(bits,x)],'mpfr_seconds':idx[(bits,y)]})
with (out/'comparisons.csv').open('w',newline='',encoding='utf-8') as f:
    w=csv.DictWriter(f,fieldnames=list(comps[0]));w.writeheader();w.writerows(comps)
lines=['# Range, roots, and reusable-division benchmark','',f'Medians of {a.repetitions} warmed runs. Matched batch rows give both implementations {a.workers} outer workers.','', '| Precision | Method | Median (s) |','|---:|---|---:|']
for r in rows:lines.append(f"| {r['bits']:,} | {r['method']} | {r['median_seconds']:.9f} |")
lines+=['','## Comparisons','','A speedup above one favors Polyphase. Single-operation rows are one-thread comparisons. The batch row is equal-worker throughput.','','| Precision | Comparison | Speedup |','|---:|---|---:|']
for r in comps:lines.append(f"| {r['bits']:,} | {r['comparison']} | {r['speedup']:.3f}x |")
(out/'REPORT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
(out/'environment.json').write_text(json.dumps({'platform':platform.platform(),'bits':sorted({r['bits'] for r in rows}),'repetitions':a.repetitions,'workers':a.workers},indent=2)+'\n')
print((out/'REPORT.md').read_text())
