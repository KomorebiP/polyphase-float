#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

from __future__ import annotations
import argparse, json, subprocess
from pathlib import Path

CONDITIONAL_CORE = {
    'ppf_get_float128','ppf_set_float128',
    'ppf_get_decimal64','ppf_set_decimal64',
    'ppf_get_decimal128','ppf_set_decimal128',
}

def load_symbols(path: Path) -> list[str]:
    return [x.strip() for x in path.read_text().splitlines() if x.strip() and not x.startswith('#')]

def dynamic_symbols(library: Path):
    cp=subprocess.run(['nm','-D','--defined-only',str(library)],text=True,capture_output=True,check=True)
    result={}; non_ppf=[]
    for line in cp.stdout.splitlines():
        parts=line.split()
        if not parts: continue
        raw=parts[-1]
        if '@@' in raw: base,ver=raw.split('@@',1)
        elif '@' in raw: base,ver=raw.split('@',1)
        else: base,ver=raw,''
        if base.startswith('ppf_'):
            result[base]=ver
        elif base and not base.startswith('_') and not base.startswith('PPF_'):
            non_ppf.append(base)
    return result,sorted(set(non_ppf))

def check_one(library:Path, groups:list[tuple[Path,str]], conditional:set[str], require_versions:bool):
    actual, non_ppf=dynamic_symbols(library)
    expected={};
    for path,version in groups:
        for name in load_symbols(path): expected[name]=version
    missing=sorted(name for name in expected if name not in actual and name not in conditional)
    unexpected=sorted(name for name in actual if name not in expected)
    version_errors=[]
    if require_versions:
        for name,version in expected.items():
            if name in actual and actual[name] != version:
                version_errors.append({'name':name,'expected':version,'actual':actual[name]})
    return {
      'library':str(library),'actual_count':len(actual),'expected_count':len(expected),
      'missing':missing,'conditional_missing':sorted(name for name in conditional if name in expected and name not in actual),
      'unexpected':unexpected,'version_errors':version_errors,'non_ppf_exports':non_ppf,
      'passed':not missing and not unexpected and not version_errors and not non_ppf,
    }

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--root',type=Path,default=Path('.'))
    ap.add_argument('--core-library',type=Path,required=True)
    ap.add_argument('--adapter-library',type=Path)
    ap.add_argument('--require-versions',action='store_true')
    ap.add_argument('--output',type=Path)
    a=ap.parse_args(); root=a.root.resolve()
    reports=[]
    reports.append(check_one(a.core_library,[
      (root/'abi/symbols/core-0.11.txt','PPF_0.11'),
      (root/'abi/symbols/core-0.12.txt','PPF_0.12')],CONDITIONAL_CORE,a.require_versions))
    if a.adapter_library:
      reports.append(check_one(a.adapter_library,[
        (root/'abi/symbols/adapter-0.11.txt','PPF_MPFR_0.11')],set(),a.require_versions))
    result={'schema_version':1,'reports':reports,'passed':all(x['passed'] for x in reports)}
    if a.output:
      a.output.parent.mkdir(parents=True,exist_ok=True); a.output.write_text(json.dumps(result,indent=2,sort_keys=True)+'\n')
    print(json.dumps(result,indent=2,sort_keys=True))
    if not result['passed']: raise SystemExit(1)
if __name__=='__main__': main()
