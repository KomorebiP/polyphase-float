#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later
from __future__ import annotations
import argparse,json,subprocess
from pathlib import Path

def run_probe(path:Path):
    cp=subprocess.run([str(path)],text=True,capture_output=True,check=True)
    return json.loads(cp.stdout)

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--probe',type=Path,required=True); ap.add_argument('--baseline',type=Path,required=True); ap.add_argument('--output',type=Path); ap.add_argument('--skip-model-mismatch',action='store_true'); a=ap.parse_args()
    cur=run_probe(a.probe); base=json.loads(a.baseline.read_text())
    if cur['model']!=base['model']:
        result={'compatible':None,'skipped':True,'reason':'data model differs','baseline_model':base['model'],'current_model':cur['model']}
        print(json.dumps(result,indent=2));
        if a.output: a.output.write_text(json.dumps(result,indent=2,sort_keys=True)+'\n')
        raise SystemExit(77 if a.skip_model_mismatch else 1)
    diffs=[]
    for section in ('types','offsets','values'):
        for key,val in base[section].items():
            if key not in cur[section]: diffs.append({'section':section,'name':key,'change':'missing'})
            elif cur[section][key]!=val: diffs.append({'section':section,'name':key,'before':val,'after':cur[section][key]})
    result={'compatible':not diffs,'skipped':False,'baseline_version':base.get('release_version'),'current_version':cur.get('release_version'),'differences':diffs,'model':cur['model']}
    if a.output: a.output.write_text(json.dumps(result,indent=2,sort_keys=True)+'\n')
    print(json.dumps(result,indent=2,sort_keys=True))
    if diffs: raise SystemExit(1)
if __name__=='__main__': main()
