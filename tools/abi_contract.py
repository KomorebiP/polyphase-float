#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later
from __future__ import annotations
import argparse, hashlib, json, re
from pathlib import Path

COMMENT_RE = re.compile(r'/\*.*?\*/|//[^\n]*', re.S)

def strip_comments(text: str) -> str:
    return COMMENT_RE.sub('', text)

def norm(s: str) -> str:
    return re.sub(r'\s+', ' ', s).strip()

def parse_functions(path: Path):
    lines = path.read_text(encoding='utf-8').splitlines()
    stack=[]; functions=[]; buf=[]; cond=''
    ignored_guards={'POLYPHASE_FLOAT_H','POLYPHASE_FLOAT_MPFR_H','__cplusplus'}
    for raw in lines:
        line=raw.strip()
        if line.startswith('#if '):
            stack.append(line[4:].strip()); continue
        if line.startswith('#ifdef '):
            name=line[7:].strip(); stack.append('' if name in ignored_guards else 'defined('+name+')'); continue
        if line.startswith('#ifndef '):
            name=line[8:].strip(); stack.append('' if name in ignored_guards else '!defined('+name+')'); continue
        if line.startswith('#elif '):
            if stack: stack[-1]=line[6:].strip()
            continue
        if line == '#else':
            if stack: stack[-1]='!('+stack[-1]+')'
            continue
        if line == '#endif':
            if stack: stack.pop()
            continue
        if not buf and re.match(r'^\s*PPF_API\b', raw):
            buf=[raw.strip()]; cond=' && '.join(x for x in stack if x)
        elif buf:
            buf.append(raw.strip())
        if buf and ';' in raw:
            declaration=norm(strip_comments(' '.join(buf)))
            declaration=declaration.replace('PPF_API ', '', 1)
            m=re.search(r'\b(ppf_[A-Za-z0-9_]+)\s*\(', declaration)
            if not m:
                raise RuntimeError(f'cannot parse function declaration: {declaration}')
            functions.append({'name':m.group(1),'declaration':declaration,'condition':cond,'header':path.name})
            buf=[]; cond=''
    return functions

def parse_typedefs(text: str):
    clean=strip_comments(text)
    out={}
    for m in re.finditer(r'typedef\s+([^;{}]+?)\s+(ppf_t|ppf_[A-Za-z0-9_]+_t)\s*;', clean, re.S):
        body=norm(m.group(1))
        name=m.group(2)
        if body.startswith('struct ') or body.startswith('enum '):
            continue
        out[name]=body
    return out

def parse_structs(text: str):
    clean=strip_comments(text)
    out={}
    for m in re.finditer(r'typedef\s+struct\s*\{(.*?)\}\s*(ppf_t|ppf_[A-Za-z0-9_]+_t)\s*;', clean, re.S):
        body=m.group(1)
        fields=[]
        for part in body.split(';'):
            p=norm(part)
            if p:
                fields.append(p+';')
        out[m.group(2)]=fields
    return out

def parse_enums(text: str):
    clean=strip_comments(text)
    out={}
    for m in re.finditer(r'typedef\s+enum\s*\{(.*?)\}\s*(ppf_t|ppf_[A-Za-z0-9_]+_t)\s*;', clean, re.S):
        body=norm(m.group(1))
        items=[norm(x) for x in body.split(',') if norm(x)]
        out[m.group(2)]=items
    idx=0
    for m in re.finditer(r'(?<!typedef\s)enum\s*\{(.*?)\}\s*;', clean, re.S):
        body=norm(m.group(1)); items=[norm(x) for x in body.split(',') if norm(x)]
        if not items: continue
        first=re.match(r'([A-Za-z_][A-Za-z0-9_]*)', items[0])
        key='anonymous:'+(first.group(1) if first else str(idx)); idx+=1
        out[key]=items
    return out

def parse_constants(text: str):
    keep={
        'PPF_PREC_DEFAULT','PPF_EMIN_DEFAULT','PPF_EMAX_DEFAULT','PPF_EMIN_MIN','PPF_EMAX_MAX',
        'PPF_TERNARY_ERROR','PPF_ABI_EPOCH','PPF_ABI_REVISION','PPF_ABI_TAG','PPF_ABI_INFO_VERSION',
        'PPF_SEMANTIC_CONTRACT_VERSION','PPF_SEMANTIC_CONTRACT_SHA256',
        'PPF_ALIASING_CONTRACT_VERSION','PPF_ALIASING_CONTRACT_SHA256'
    }
    out={}
    lines=text.splitlines()
    i=0
    while i<len(lines):
        line=lines[i].rstrip()
        if line.lstrip().startswith('#define '):
            acc=line.strip()
            while acc.endswith('\\') and i+1<len(lines):
                i+=1; acc=acc[:-1]+' '+lines[i].strip()
            m=re.match(r'#define\s+(PPF_[A-Za-z0-9_]+)\s+(.+)', acc)
            if m and m.group(1) in keep:
                out[m.group(1)]=norm(m.group(2))
        i+=1
    return out

def parse_version(text: str):
    vals=[]
    for macro in ('PPF_VERSION_MAJOR','PPF_VERSION_MINOR','PPF_VERSION_PATCHLEVEL'):
        m=re.search(rf'#define\s+{macro}\s+(\d+)',text); vals.append(m.group(1) if m else '0')
    return '.'.join(vals)

def generate(root: Path):
    headers=[root/'include/polyphase_float.h',root/'include/polyphase_float_mpfr.h']
    functions=[]
    for h in headers: functions.extend(parse_functions(h))
    functions=sorted(functions,key=lambda x:x['name'])
    if len({f['name'] for f in functions})!=len(functions):
        raise RuntimeError('duplicate public function names')
    core=headers[0].read_text(encoding='utf-8')
    contract={
        'schema_version':1,
        'library':'Polyphase Float',
        'release_version':parse_version(core),
        'abi_epoch':int(re.search(r'#define\s+PPF_ABI_EPOCH\s+(\d+)',core).group(1)) if 'PPF_ABI_EPOCH' in core else None,
        'abi_revision':int(re.search(r'#define\s+PPF_ABI_REVISION\s+(\d+)',core).group(1)) if 'PPF_ABI_REVISION' in core else None,
        'functions':functions,
        'typedefs':parse_typedefs(core),
        'structs':parse_structs(core),
        'enums':parse_enums(core),
        'constants':parse_constants(core),
    }
    contract['contract_sha256']=contract_hash(contract)
    return contract

def canonical_payload(data):
    d=json.loads(json.dumps(data))
    d.pop('contract_sha256',None)
    d.pop('release_version',None)
    return json.dumps(d,sort_keys=True,separators=(',',':'),ensure_ascii=False).encode()

def contract_hash(data): return hashlib.sha256(canonical_payload(data)).hexdigest()

def compare(base,cur):
    metadata_constants={'PPF_SEMANTIC_CONTRACT_SHA256','PPF_ALIASING_CONTRACT_SHA256'}
    report={'schema_version':1,'baseline_version':base.get('release_version'),'current_version':cur.get('release_version'),'breaking':[],'compatible_additions':[],'compatible_metadata_changes':[],'unchanged':[]}
    for key in ('functions','typedefs','structs','enums','constants'):
        b=base.get(key,{}); c=cur.get(key,{})
        if isinstance(b,list): b={x['name']:x for x in b}; c={x['name']:x for x in c}
        for name,val in sorted(b.items()):
            if name not in c: report['breaking'].append({'category':key,'name':name,'change':'removed'})
            elif c[name]!=val:
                change={'category':key,'name':name,'change':'changed','before':val,'after':c[name]}
                if key=='constants' and name in metadata_constants:
                    report['compatible_metadata_changes'].append(change)
                else:
                    report['breaking'].append(change)
            else: report['unchanged'].append({'category':key,'name':name})
        for name,val in sorted(c.items()):
            if name not in b: report['compatible_additions'].append({'category':key,'name':name,'value':val})
    report['compatible']=not report['breaking']
    return report

def main():
    ap=argparse.ArgumentParser(); sub=ap.add_subparsers(dest='cmd',required=True)
    g=sub.add_parser('generate'); g.add_argument('--root',type=Path,default=Path('.')); g.add_argument('--output',type=Path,required=True)
    v=sub.add_parser('verify'); v.add_argument('--root',type=Path,default=Path('.')); v.add_argument('--contract',type=Path,required=True)
    c=sub.add_parser('compare'); c.add_argument('--baseline',type=Path,required=True); c.add_argument('--current',type=Path,required=True); c.add_argument('--json-output',type=Path); c.add_argument('--markdown-output',type=Path)
    h=sub.add_parser('hash'); h.add_argument('--contract',type=Path,required=True)
    a=ap.parse_args()
    if a.cmd=='generate':
        data=generate(a.root.resolve()); a.output.parent.mkdir(parents=True,exist_ok=True); a.output.write_text(json.dumps(data,indent=2,sort_keys=True)+'\n'); print(data['contract_sha256'])
    elif a.cmd=='verify':
        expected=json.loads(a.contract.read_text()); actual=generate(a.root.resolve())
        if expected!=actual:
            print(json.dumps(compare(expected,actual),indent=2)); raise SystemExit(1)
        print(f"ABI contract verified: {actual['contract_sha256']}")
    elif a.cmd=='compare':
        base=json.loads(a.baseline.read_text()); cur=json.loads(a.current.read_text()); r=compare(base,cur)
        if a.json_output: a.json_output.write_text(json.dumps(r,indent=2,sort_keys=True)+'\n')
        if a.markdown_output:
            lines=['# ABI contract comparison','',f"Baseline: `{r['baseline_version']}`",f"Current: `{r['current_version']}`",'',f"Compatible: **{'yes' if r['compatible'] else 'no'}**",'',f"Breaking changes: **{len(r['breaking'])}**",f"Compatible additions: **{len(r['compatible_additions'])}**",f"Compatible metadata updates: **{len(r['compatible_metadata_changes'])}**",f"Unchanged entries: **{len(r['unchanged'])}**",'']
            if r['breaking']:
                lines+=['## Breaking changes','']+[f"- `{x['category']}` `{x['name']}`: {x['change']}" for x in r['breaking']]+['']
            if r['compatible_additions']:
                lines+=['## Compatible additions','']+[f"- `{x['category']}` `{x['name']}`" for x in r['compatible_additions']]+['']
            if r['compatible_metadata_changes']:
                lines+=['## Compatible metadata updates','']+[f"- `{x['category']}` `{x['name']}`" for x in r['compatible_metadata_changes']]+['']
            a.markdown_output.write_text('\n'.join(lines)+'\n')
        print(json.dumps({'compatible':r['compatible'],'breaking':len(r['breaking']),'additions':len(r['compatible_additions']),'metadata_updates':len(r['compatible_metadata_changes']),'unchanged':len(r['unchanged'])}))
        if not r['compatible']: raise SystemExit(1)
    else:
        data=json.loads(a.contract.read_text()); print(contract_hash(data))
if __name__=='__main__': main()
