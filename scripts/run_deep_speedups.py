#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

from __future__ import annotations
import argparse, csv, hashlib, json, math, os, random, statistics, subprocess, time
from pathlib import Path
from typing import Sequence

def median(xs: Sequence[float]) -> float: return float(statistics.median(xs))
def mad(xs: Sequence[float]) -> float:
    m=median(xs); return median([abs(x-m) for x in xs])
def percentile(xs: Sequence[float], p: float) -> float:
    ys=sorted(xs)
    if len(ys)==1:return float(ys[0])
    q=(len(ys)-1)*p; lo=math.floor(q); hi=math.ceil(q)
    return float(ys[lo] if lo==hi else ys[lo]*(hi-q)+ys[hi]*(q-lo))
def bootstrap_ci(xs: Sequence[float], seed: int, draws: int) -> tuple[float,float]:
    rng=random.Random(seed); n=len(xs); vals=[]
    for _ in range(draws): vals.append(median([xs[rng.randrange(n)] for _ in range(n)]))
    return percentile(vals,.025),percentile(vals,.975)
def sign_p(w:int,l:int)->float:
    n=w+l
    if n==0:return 1.0
    k=min(w,l); return min(1.0,2.0*sum(math.comb(n,i) for i in range(k+1))/(2**n))
def write_csv(path:Path, rows:list[dict]):
    path.parent.mkdir(parents=True,exist_ok=True)
    if not rows:path.write_text('');return
    fields=[]
    for r in rows:
        for k in r:
            if k not in fields:fields.append(k)
    with path.open('w',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rows)
def run_json(cmd:list[str], env:dict[str,str])->dict:
    p=subprocess.run(cmd,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=False)
    if p.returncode:
        raise RuntimeError(f"command failed ({p.returncode}): {' '.join(cmd)}\n{p.stdout}\n{p.stderr}")
    for line in reversed([x.strip() for x in p.stdout.splitlines() if x.strip()]):
        try:return json.loads(line)
        except json.JSONDecodeError:pass
    raise RuntimeError(f"no JSON output: {p.stdout}\n{p.stderr}")

def main()->int:
    ap=argparse.ArgumentParser()
    ap.add_argument('--root',default='.')
    ap.add_argument('--out',default='results/deep_speedups')
    ap.add_argument('--bits',default='100000,1000000,5000000')
    ap.add_argument('--repetitions',type=int,default=9)
    ap.add_argument('--workers',type=int,default=3)
    ap.add_argument('--jobs',type=int,default=3)
    ap.add_argument('--inner-repetitions',type=int,default=1)
    ap.add_argument('--bootstrap-draws',type=int,default=20000)
    ap.add_argument('--seed',type=int,default=20260811)
    ap.add_argument('--fresh',action='store_true')
    args=ap.parse_args()
    root=Path(args.root).resolve(); out=(root/args.out).resolve(); out.mkdir(parents=True,exist_ok=True)
    if args.fresh:
        for x in out.iterdir():
            if x.is_file():x.unlink()
    bits_list=[int(x) for x in args.bits.split(',') if x.strip()]
    reps=max(3,args.repetitions); workers=max(1,args.workers); jobs=max(1,args.jobs)
    env=os.environ.copy();env.update({'LC_ALL':'C','LANG':'C','OMP_PROC_BIND':'close','OMP_PLACES':'cores','OMP_DYNAMIC':'FALSE'})
    single=root/'bin/benchmark_hotpath_method'; matched=root/'bin/benchmark_recsqrt_matched'
    if not single.exists() or not matched.exists():raise RuntimeError('benchmark executables are missing; rebuild first')
    specs=[
      ('get_str','ppf_1t','ppf_get_str','mpfr_1t','mpfr_get_str','single'),
      ('rec_sqrt','ppf_1t','ppf_rec_sqrt','mpfr_1t','mpfr_rec_sqrt','single'),
      ('cbrt','ppf_1t','ppf_cbrt','mpfr_1t','mpfr_cbrt','single'),
      ('root7','ppf_1t','ppf_root7','mpfr_1t','mpfr_root7','single'),
      ('rec_sqrt_latency','ppf_3t','ppf_rec_sqrt_3t','mpfr_1t','mpfr_rec_sqrt','single'),
      ('rec_sqrt_matched','ppf_outer','ppf_outer','mpfr_outer','mpfr_outer','matched'),
    ]
    rng=random.Random(args.seed)
    raw_path=out/'raw.csv'
    rows=[]
    if raw_path.exists() and not args.fresh:
        with raw_path.open(newline='',encoding='utf-8') as f:
            rows=list(csv.DictReader(f))
        for r in rows:
            r['bits']=int(r['bits']); r['repeat']=int(r['repeat']); r['order']=int(r['order'])
            r['workers']=int(r['workers']); r['jobs']=int(r['jobs']); r['seconds']=float(r['seconds'])
    completed={(r['bits'],r['operation'],r['repeat'],r['method']) for r in rows}
    for bits in bits_list:
        for opname,alabel,amethod,blabel,bmethod,kind in specs:
            for repeat in range(1,reps+1):
                order=[(alabel,amethod),(blabel,bmethod)];rng.shuffle(order)
                for pos,(label,method) in enumerate(order,1):
                    key=(bits,opname,repeat,label)
                    if key in completed:
                        continue
                    if kind=='single':
                        payload=run_json([str(single),method,str(bits),str(max(1,args.inner_repetitions))],env)
                        seconds=float(payload['seconds_per_call'])
                    else:
                        payload=run_json([str(matched),method,str(bits),str(workers),str(jobs),str(max(1,args.inner_repetitions))],env)
                        seconds=float(payload['seconds_per_batch'])
                    rows.append({'bits':bits,'operation':opname,'repeat':repeat,'order':pos,'method':label,
                                 'workers':3 if label=='ppf_3t' else (workers if 'outer' in label else 1),
                                 'jobs':jobs if kind=='matched' else 1,'seconds':seconds})
                    completed.add(key)
                    write_csv(raw_path,rows)
                    print(f"bits={bits:,} operation={opname} round={repeat} {label}: {seconds:.9f}s",flush=True)
    write_csv(raw_path,rows)
    summaries=[]
    for key in sorted({(r['bits'],r['operation'],r['method']) for r in rows}):
        b,o,m=key; g=[r for r in rows if (r['bits'],r['operation'],r['method'])==key]; vals=[r['seconds'] for r in g]
        summaries.append({'bits':b,'operation':o,'method':m,'workers':g[0]['workers'],'jobs':g[0]['jobs'],'runs':len(vals),
                          'median_seconds':median(vals),'mad_seconds':mad(vals),'min_seconds':min(vals),'max_seconds':max(vals)})
    write_csv(out/'summary.csv',summaries)
    comparisons=[]
    for b,o in sorted({(r['bits'],r['operation']) for r in rows}):
        methods=sorted({r['method'] for r in rows if r['bits']==b and r['operation']==o})
        first=next(m for m in methods if m.startswith('ppf'))
        second=next(m for m in methods if m.startswith('mpfr'))
        ratios=[];w=l=t=0
        for rr in range(1,reps+1):
            a=next(r['seconds'] for r in rows if r['bits']==b and r['operation']==o and r['repeat']==rr and r['method']==first)
            bb=next(r['seconds'] for r in rows if r['bits']==b and r['operation']==o and r['repeat']==rr and r['method']==second)
            ratios.append(bb/a)
            if a<bb:w+=1
            elif a>bb:l+=1
            else:t+=1
        seed=args.seed^b^int.from_bytes(hashlib.sha256(o.encode()).digest()[:8],'big')
        lo,hi=bootstrap_ci(ratios,seed,args.bootstrap_draws)
        comparisons.append({'bits':b,'operation':o,'comparison':f'{first}_vs_{second}','pairs':reps,
                            'median_paired_speedup':median(ratios),'bootstrap_95_low':lo,'bootstrap_95_high':hi,
                            'wins':w,'losses':l,'ties':t,'sign_test_two_sided_p':sign_p(w,l)})
    write_csv(out/'paired_comparisons.csv',comparisons)
    idx={(r['bits'],r['operation'],r['method']):r for r in summaries}
    lines=['# Deep hot-path speedups','',
      'All timed results are checked against MPFR before the benchmark process exits. A speedup above 1 favors Polyphase.','',
      '## One-thread and single-call results','',
      '| Bits | Operation | Polyphase method | Polyphase median | MPFR median | Paired speedup | 95% interval | W-L-T |',
      '|---:|---|---|---:|---:|---:|---:|---:|']
    cmap={(r['bits'],r['operation']):r for r in comparisons}
    labels={'get_str':'128-digit output','rec_sqrt':'reciprocal square root','cbrt':'cube root','root7':'seventh root','rec_sqrt_latency':'reciprocal square root latency'}
    for b in bits_list:
        for o in ('get_str','rec_sqrt','cbrt','root7','rec_sqrt_latency'):
            c=cmap[(b,o)]; pm=next(m for (bb,oo,m) in idx if bb==b and oo==o and m.startswith('ppf')); mm=next(m for (bb,oo,m) in idx if bb==b and oo==o and m.startswith('mpfr'))
            p=idx[(b,o,pm)];q=idx[(b,o,mm)]
            lines.append(f"| {b:,} | {labels[o]} | {pm} | {p['median_seconds']:.6f} s | {q['median_seconds']:.6f} s | {c['median_paired_speedup']:.3f}x | [{c['bootstrap_95_low']:.3f}, {c['bootstrap_95_high']:.3f}] | {c['wins']}-{c['losses']}-{c['ties']} |")
    lines += ['', '## Equal-worker batch throughput', '',
      f'Both methods receive {workers} outer workers and {jobs} independent reciprocal-square-root jobs.', '',
      '| Bits | Polyphase batch | MPFR batch | Paired speedup | 95% interval | W-L-T |','|---:|---:|---:|---:|---:|---:|']
    for b in bits_list:
        c=cmap[(b,'rec_sqrt_matched')]; p=idx[(b,'rec_sqrt_matched','ppf_outer')]; q=idx[(b,'rec_sqrt_matched','mpfr_outer')]
        lines.append(f"| {b:,} | {p['median_seconds']:.6f} s | {q['median_seconds']:.6f} s | {c['median_paired_speedup']:.3f}x | [{c['bootstrap_95_low']:.3f}, {c['bootstrap_95_high']:.3f}] | {c['wins']}-{c['losses']}-{c['ties']} |")
    lines += ['', 'The three-thread row is single-call latency against one ordinary MPFR call. The matched batch row is the equal-worker throughput comparison.']
    (out/'REPORT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    environment={'timestamp_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),'bits':bits_list,'repetitions':reps,'workers':workers,'jobs':jobs,'bootstrap_draws':args.bootstrap_draws,'seed':args.seed,
                 'uname':subprocess.run(['uname','-a'],text=True,capture_output=True).stdout.strip(),
                 'gcc':subprocess.run(['gcc','--version'],text=True,capture_output=True).stdout.splitlines()[0]}
    (out/'environment.json').write_text(json.dumps(environment,indent=2)+'\n')
    print('\n'+(out/'REPORT.md').read_text(),flush=True)
    return 0
if __name__=='__main__':raise SystemExit(main())
