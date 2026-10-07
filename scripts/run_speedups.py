#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

from __future__ import annotations
import argparse,csv,hashlib,json,math,os,random,statistics,subprocess,time
from pathlib import Path

METHODS=(
'ppf_add legacy_add mpfr_add '
'ppf_mul_1t ppf_mul_inner legacy_mul_inner mpfr_mul_1t '
'ppf_sqr_1t ppf_sqr_inner mpfr_sqr_1t '
'ppf_scale legacy_scale mpfr_scale '
'ppf_div mpfr_div ppf_sqrt mpfr_sqrt ppf_hypot mpfr_hypot '
'ppf_fma mpfr_fma ppf_dot_1t ppf_dot_inner mpfr_dot_1t '
'ppf_mul_outer mpfr_mul_outer ppf_plan_batch mpfr_div_outer').split()
COMPS=[
('direct_add_vs_previous_mpz','ppf_add','legacy_add'),
('direct_product_channels_vs_previous_mpz_channels','ppf_mul_inner','legacy_mul_inner'),
('direct_scaling_vs_previous_mpz_scaling','ppf_scale','legacy_scale'),
('add_vs_mpfr_1t','ppf_add','mpfr_add'),
('multiply_vs_mpfr_1t','ppf_mul_1t','mpfr_mul_1t'),
('multiply_internal_threads_vs_mpfr_1t','ppf_mul_inner','mpfr_mul_1t'),
('square_vs_mpfr_1t','ppf_sqr_1t','mpfr_sqr_1t'),
('square_internal_threads_vs_mpfr_1t','ppf_sqr_inner','mpfr_sqr_1t'),
('scale_vs_mpfr_1t','ppf_scale','mpfr_scale'),
('division_vs_mpfr_1t','ppf_div','mpfr_div'),
('sqrt_vs_mpfr_1t','ppf_sqrt','mpfr_sqrt'),
('hypot_vs_mpfr_1t','ppf_hypot','mpfr_hypot'),
('fma_vs_mpfr_1t','ppf_fma','mpfr_fma'),
('dot_vs_mpfr_1t','ppf_dot_1t','mpfr_dot_1t'),
('dot_internal_threads_vs_mpfr_1t','ppf_dot_inner','mpfr_dot_1t'),
('multiply_equal_outer_workers','ppf_mul_outer','mpfr_mul_outer'),
('planned_division_equal_outer_workers','ppf_plan_batch','mpfr_div_outer')]

def med(x): return float(statistics.median(x))
def mad(x):
 m=med(x); return med([abs(v-m) for v in x])
def pct(x,p):
 y=sorted(x); q=(len(y)-1)*p; a=math.floor(q); b=math.ceil(q)
 return y[a] if a==b else y[a]*(b-q)+y[b]*(q-a)
def ci(x,seed,n=5000):
 r=random.Random(seed); k=len(x); y=[med([x[r.randrange(k)] for _ in range(k)]) for _ in range(n)]
 return pct(y,.025),pct(y,.975)
def signp(w,l):
 n=w+l
 if not n:return 1.0
 k=min(w,l); return min(1.0,2*sum(math.comb(n,i) for i in range(k+1))/(2**n))
def writecsv(path,rows):
 if not rows:path.write_text('');return
 keys=[]
 for r in rows:
  for k in r:
   if k not in keys:keys.append(k)
 with path.open('w',newline='',encoding='utf-8') as f:
  w=csv.DictWriter(f,fieldnames=keys);w.writeheader();w.writerows(rows)
def cpus():
 allowed=sorted(os.sched_getaffinity(0)) if hasattr(os,'sched_getaffinity') else list(range(os.cpu_count() or 1))
 try:t=subprocess.check_output(['lscpu','-p=CPU,CORE,SOCKET'],text=True)
 except Exception:return allowed
 out=[];seen=set();aset=set(allowed)
 for line in t.splitlines():
  if not line or line[0]=='#':continue
  cpu,core,sock=map(int,line.split(',')[:3])
  if cpu in aset and (sock,core) not in seen:seen.add((sock,core));out.append(cpu)
 return out or allowed

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--root',default='.');ap.add_argument('--out',default='results/speedups')
 ap.add_argument('--bits',default='100000,1000000,5000000');ap.add_argument('--workers',type=int,default=0);ap.add_argument('--jobs',type=int,default=0)
 ap.add_argument('--repeats',type=int,default=9);ap.add_argument('--inner-repeats',type=int,default=5);ap.add_argument('--bootstrap-draws',type=int,default=20000);ap.add_argument('--cooldown',type=float,default=.05);ap.add_argument('--seed',type=int,default=20260810)
 a=ap.parse_args();root=Path(a.root).resolve();out=(root/a.out).resolve();out.mkdir(parents=True,exist_ok=True);binary=root/'bin/benchmark_speedup_method'
 bits=[int(x) for x in a.bits.split(',') if x];pc=cpus();workers=len(pc) if a.workers <= 0 else min(max(1,a.workers),len(pc));jobs=workers if a.jobs <= 0 else max(1,a.jobs);aff=','.join(map(str,pc[:workers]))
 env=os.environ.copy();env.update(LC_ALL='C',LANG='C',OMP_PROC_BIND='close',OMP_PLACES='cores');rng=random.Random(a.seed);raw=[]
 raw_path=out/'raw.csv'
 if raw_path.exists():
  with raw_path.open(newline='',encoding='utf-8') as f:
   for row in csv.DictReader(f):
    row['bits']=int(row['bits']);row['workers']=int(row['workers']);row['jobs']=int(row['jobs']);row['seconds']=float(row['seconds']);row['verified']=str(row['verified']).lower()=='true';row['repeat']=int(row['repeat']);row['order']=int(row['order']);raw.append(row)
 done={(r['bits'],r['repeat'],r['method']) for r in raw}
 print(f'Physical cores: {pc}; workers={workers}; jobs={jobs}; resumed={len(raw)} rows',flush=True)
 for bit in bits:
  print(f'\n{bit:,} bits',flush=True)
  for rep in range(1,a.repeats+1):
   order=list(METHODS);rng.shuffle(order)
   for pos,m in enumerate(order,1):
    if (bit,rep,m) in done: continue
    cmd=['taskset','-c',aff,str(binary),m,str(bit),str(workers),str(jobs),str(a.inner_repeats)]
    p=subprocess.run(cmd,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    if p.returncode:raise RuntimeError(f'{m} failed\n{p.stdout}\n{p.stderr}')
    r=json.loads(p.stdout.strip().splitlines()[-1])
    if not bool(r.get("verified", False)):
     raise RuntimeError(f"{m} produced an unverified result: {r}")
    r.update(repeat=rep,order=pos);raw.append(r);done.add((bit,rep,m))
    print(f'  round={rep} {m}: {r["seconds"]:.9f}s verified',flush=True)
    if a.cooldown:time.sleep(a.cooldown)
   writecsv(out/'raw.csv',raw);(out/'raw.json').write_text(json.dumps(raw,indent=2))
 summary=[]
 for bit in bits:
  for m in METHODS:
   x=[r['seconds'] for r in raw if r['bits']==bit and r['method']==m]
   summary.append(dict(bits=bit,method=m,workers=workers,jobs=jobs,runs=len(x),median_seconds=med(x),mad_seconds=mad(x),min_seconds=min(x),max_seconds=max(x)))
 writecsv(out/'summary.csv',summary);lookup={(r['bits'],r['repeat'],r['method']):r['seconds'] for r in raw};paired=[]
 for bit in bits:
  for label,first,second in COMPS:
   x=[];w=l=t=0
   for rep in range(1,a.repeats+1):
    aa=lookup[bit,rep,first];bb=lookup[bit,rep,second];x.append(bb/aa)
    if aa<bb:w+=1
    elif aa>bb:l+=1
    else:t+=1
   lo,hi=ci(x,a.seed^bit^int.from_bytes(hashlib.sha256(label.encode()).digest()[:8],'big'),a.bootstrap_draws)
   paired.append(dict(bits=bit,comparison=label,first_method=first,second_method=second,paired_median_speedup=med(x),bootstrap_95_low=lo,bootstrap_95_high=hi,wins=w,losses=l,ties=t,sign_test_two_sided_p=signp(w,l)))
 writecsv(out/'paired_comparisons.csv',paired);idx={(r['bits'],r['method']):r for r in summary}
 show=['ppf_add','mpfr_add','ppf_mul_1t','ppf_mul_inner','mpfr_mul_1t','ppf_sqr_1t','ppf_sqr_inner','mpfr_sqr_1t','ppf_scale','mpfr_scale','ppf_div','mpfr_div','ppf_sqrt','mpfr_sqrt','ppf_hypot','mpfr_hypot','ppf_fma','mpfr_fma','ppf_dot_1t','ppf_dot_inner','mpfr_dot_1t','ppf_mul_outer','mpfr_mul_outer','ppf_plan_batch','mpfr_div_outer']
 lines=['# Independent floating-point speed refinement','','All Polyphase rows use the independent `ppf_t` representation. MPFR is only the comparison and correctness oracle.','','## Median timings','','| Bits | Method | Median (s) | MAD (s) |','|---:|---|---:|---:|']
 for bit in bits:
  for m in show:
   r=idx[bit,m];lines.append(f'| {bit:,} | {m} | {r["median_seconds"]:.9f} | {r["mad_seconds"]:.9f} |')
 lines+=['','## Paired comparisons','','A speedup above 1 favors the first method named.','','| Bits | Comparison | Paired median | Bootstrap 95% | W-L-T | Sign p |','|---:|---|---:|---:|---:|---:|']
 for r in paired:lines.append(f'| {r["bits"]:,} | {r["comparison"]} | {r["paired_median_speedup"]:.3f}x | [{r["bootstrap_95_low"]:.3f}, {r["bootstrap_95_high"]:.3f}] | {r["wins"]}-{r["losses"]}-{r["ties"]} | {r["sign_test_two_sided_p"]:.6g} |')
 lines+=['','## Contracts','','`ppf_mul_inner` and `ppf_sqr_inner` are single-call latency comparisons against ordinary one-thread MPFR.','`ppf_mul_outer` versus `mpfr_mul_outer`, and `ppf_plan_batch` versus `mpfr_div_outer`, give both methods the same outer worker count.','The `legacy_*` methods reproduce the earlier GMP-integer detours as implementation ablations.','']
 report='\n'.join(lines);(out/'REPORT.md').write_text(report);(out/'environment.json').write_text(json.dumps(dict(timestamp_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),bits=bits,workers=workers,jobs=jobs,repeats=a.repeats,inner_repeats=a.inner_repeats,bootstrap_draws=a.bootstrap_draws,physical_cpu_list=pc,uname=subprocess.check_output(['uname','-a'],text=True).strip(),compiler=subprocess.check_output(['gcc','--version'],text=True).splitlines()[0]),indent=2));print('\n'+report)
 return 0
if __name__=='__main__':raise SystemExit(main())
