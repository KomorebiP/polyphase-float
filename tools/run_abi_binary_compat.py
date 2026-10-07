#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later
from __future__ import annotations
import argparse,json,os,shutil,subprocess
from pathlib import Path

def run(cmd,**kw):
    return subprocess.run([str(x) for x in cmd],check=True,text=True,capture_output=True,**kw)

def build(root:Path, out:Path):
    if out.exists(): shutil.rmtree(out)
    run(['cmake','-S',root,'-B',out,'-DCMAKE_BUILD_TYPE=Release','-DPPF_MPFR_SUPPORT=OFF','-DPPF_OPENMP=OFF','-DPPF_BUILD_EXAMPLES=OFF','-DPPF_BUILD_BENCHMARKS=OFF','-DPPF_WARNINGS_AS_ERRORS=ON'])
    run(['cmake','--build',out,'-j2'])
    return (out/'libpolyphase_float.so').resolve()

def execute(binary:Path, libdir:Path):
    env=os.environ.copy(); env['LD_LIBRARY_PATH']=str(libdir)
    cp=run([binary],env=env)
    return cp.stdout.strip()

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--baseline-source',type=Path,required=True); ap.add_argument('--current-source',type=Path,required=True); ap.add_argument('--work',type=Path,required=True); ap.add_argument('--output',type=Path)
    a=ap.parse_args(); a.work.mkdir(parents=True,exist_ok=True)
    oldlib=build(a.baseline_source.resolve(),a.work/'baseline-build')
    newlib=build(a.current_source.resolve(),a.work/'current-build')
    consumer=a.work/'consumer-0.11'
    run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I',a.baseline_source.resolve()/'include',a.current_source.resolve()/'tests/abi_0_11_consumer.c','-L',oldlib.parent,'-lpolyphase_float','-Wl,-rpath,$ORIGIN','-lgmp','-lm','-pthread','-o',consumer])
    old_out=execute(consumer,oldlib.parent); new_out=execute(consumer,newlib.parent)
    needed=run(['readelf','-d',consumer]).stdout
    result={'schema_version':1,'baseline_output':old_out,'current_output':new_out,'outputs_equal':old_out==new_out,'consumer_needed_soname':'libpolyphase_float.so.0' in needed,'passed':old_out==new_out and 'libpolyphase_float.so.0' in needed}
    if a.output:
        a.output.parent.mkdir(parents=True,exist_ok=True); a.output.write_text(json.dumps(result,indent=2,sort_keys=True)+'\n')
    print(json.dumps(result,indent=2,sort_keys=True))
    if not result['passed']: raise SystemExit(1)
if __name__=='__main__': main()
