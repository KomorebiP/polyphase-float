#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

import argparse, csv, json, os, random, statistics, subprocess, sys
from pathlib import Path

PAIRS = [
    ("urandomb", "ppf_urandomb", "mpfr_urandomb"),
    ("urandom", "ppf_urandom", "mpfr_urandom"),
    ("can_round", "ppf_can_round", "mpfr_can_round"),
]

def run(exe, method, bits, loops):
    cp = subprocess.run([str(exe), method, str(bits), str(loops)],
                        check=True, capture_output=True, text=True)
    return json.loads(cp.stdout.strip().splitlines()[-1])

def med(xs): return statistics.median(xs)
def mad(xs):
    m=med(xs); return med([abs(x-m) for x in xs])

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--root',default='.')
    ap.add_argument('--out',default='results/runtime_completion')
    ap.add_argument('--bits',default='1000,10000,100000')
    ap.add_argument('--repetitions',type=int,default=9)
    args=ap.parse_args()
    root=Path(args.root).resolve(); out=(root/args.out); out.mkdir(parents=True,exist_ok=True)
    exe=root/'bin/benchmark_runtime_completion'
    bits_list=[int(x) for x in args.bits.split(',') if x.strip()]
    raw=[]; rng=random.Random(0x52554e54)
    for bits in bits_list:
        loops=max(8,min(2000,2_000_000//bits))
        for label,p,m in PAIRS:
            for rep in range(args.repetitions):
                order=[p,m]; rng.shuffle(order)
                vals={}
                for method in order:
                    row=run(exe,method,bits,loops)
                    row.update(operation=label,repetition=rep)
                    raw.append(row); vals[method]=row['seconds_per_call']
    with (out/'raw.csv').open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=['operation','bits','repetition','method','loops','seconds','seconds_per_call'])
        w.writeheader(); w.writerows(raw)
    summary=[]; paired=[]
    for bits in bits_list:
      for label,p,m in PAIRS:
        for method in [p,m]:
          xs=[r['seconds_per_call'] for r in raw if r['bits']==bits and r['operation']==label and r['method']==method]
          summary.append(dict(bits=bits,operation=label,method=method,median_seconds=med(xs),mad_seconds=mad(xs)))
        ratios=[]
        for rep in range(args.repetitions):
          pr=next(r for r in raw if r['bits']==bits and r['operation']==label and r['method']==p and r['repetition']==rep)
          mr=next(r for r in raw if r['bits']==bits and r['operation']==label and r['method']==m and r['repetition']==rep)
          ratios.append(mr['seconds_per_call']/pr['seconds_per_call'])
        paired.append(dict(bits=bits,operation=label,paired_median_speedup=med(ratios),wins=sum(x>1 for x in ratios),losses=sum(x<1 for x in ratios),ties=sum(x==1 for x in ratios)))
    with (out/'summary.csv').open('w',newline='') as f:
      w=csv.DictWriter(f,fieldnames=summary[0].keys());w.writeheader();w.writerows(summary)
    with (out/'paired_comparisons.csv').open('w',newline='') as f:
      w=csv.DictWriter(f,fieldnames=paired[0].keys());w.writeheader();w.writerows(paired)
    lines=['# Runtime Completion Benchmark','',f'Medians of {args.repetitions} randomized-order repetitions. A speedup above 1 favors Polyphase.','', '| Bits | Operation | Paired speedup | W-L-T |','|---:|---|---:|---:|']
    for r in paired:
      lines.append(f"| {r['bits']:,} | `{r['operation']}` | {r['paired_median_speedup']:.3f}× | {r['wins']}-{r['losses']}-{r['ties']} |")
    lines += ['', 'The random functions use the same GMP random-state type. `urandomb` and `urandom` are value/state differential-checked before timing. The normal and exponential generators are correctness-tested separately because they intentionally use independent exact-sampling algorithms rather than MPFR bitstream compatibility.']
    (out/'REPORT.md').write_text('\n'.join(lines)+'\n')
    (out/'environment.json').write_text(json.dumps({'python':sys.version,'platform':sys.platform,'cpu_count':os.cpu_count(),'bits':bits_list,'repetitions':args.repetitions},indent=2)+'\n')
    print((out/'REPORT.md').read_text())
if __name__=='__main__': main()
