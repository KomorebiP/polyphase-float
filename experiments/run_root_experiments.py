#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

from __future__ import annotations
import argparse, csv, pathlib, re, subprocess, tempfile

p=argparse.ArgumentParser()
p.add_argument('--root',default='.')
p.add_argument('--bits',default='100000,1000000,5000000')
p.add_argument('--repetitions',type=int,default=9)
a=p.parse_args(); root=pathlib.Path(a.root).resolve(); src=root/'experiments'; out=root/'results'/'failed_speed_experiments'; out.mkdir(parents=True,exist_ok=True)
rows=[]
with tempfile.TemporaryDirectory(prefix='ppf-root-exp-') as td:
    td=pathlib.Path(td)
    programs={
      'root_vs_rootrem':src/'benchmark_root_vs_rootrem.c',
      'sqrt_candidate':src/'benchmark_sqrt_candidate.c',
      'recsqrt_direct':src/'benchmark_recsqrt_direct.c',
    }
    for name,path in programs.items():
        exe=td/name
        subprocess.run(['gcc','-O3','-march=native','-std=c11',str(path),'-lgmp','-lm','-o',str(exe)],check=True)
        for bits in [int(x) for x in a.bits.split(',') if x.strip()]:
            if name=='root_vs_rootrem':
                for degree in (3,7):
                    text=subprocess.check_output([str(exe),str(bits),str(degree),str(a.repetitions)],text=True).strip()
                    m=re.search(r'rootrem=([0-9.]+) root=([0-9.]+) speed=([0-9.]+)',text); assert m,text
                    rows.append({'bits':bits,'experiment':name,'degree':degree,'baseline_seconds':float(m.group(1)),'candidate_seconds':float(m.group(2)),'baseline_over_candidate':float(m.group(3)),'retained':1})
            elif name=='sqrt_candidate':
                text=subprocess.check_output([str(exe),str(bits),str(a.repetitions)],text=True).strip()
                m=re.search(r'root=([0-9.]+) cand=([0-9.]+) speed=([0-9.]+)',text); assert m,text
                rows.append({'bits':bits,'experiment':name,'degree':2,'baseline_seconds':float(m.group(1)),'candidate_seconds':float(m.group(2)),'baseline_over_candidate':float(m.group(3)),'retained':0})
            else:
                text=subprocess.check_output([str(exe),str(bits),str(a.repetitions)],text=True).strip()
                m=re.search(r'qonly=([0-9.]+) direct=([0-9.]+) speed=([0-9.]+)',text); assert m,text
                rows.append({'bits':bits,'experiment':name,'degree':-2,'baseline_seconds':float(m.group(1)),'candidate_seconds':float(m.group(2)),'baseline_over_candidate':float(m.group(3)),'retained':0})
with (out/'results.csv').open('w',newline='',encoding='utf-8') as f:
    w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
lines=['# Root speed experiments','',f'Medians of {a.repetitions} warmed executions on the development host. A ratio above one means the candidate is faster than its baseline.','', '| Bits | Experiment | Degree | Baseline (s) | Candidate (s) | Baseline/candidate | Retained |','|---:|---|---:|---:|---:|---:|---:|']
for r in rows: lines.append(f"| {r['bits']:,} | {r['experiment']} | {r['degree']} | {r['baseline_seconds']:.9f} | {r['candidate_seconds']:.9f} | {r['baseline_over_candidate']:.3f}x | {r['retained']} |")
(out/'REPORT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
print((out/'REPORT.md').read_text())
