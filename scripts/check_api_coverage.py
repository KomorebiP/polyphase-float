#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

import re, subprocess, sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
inv=(root/'reference/mpfr_api_inventory.md').read_text()
hdr=(root/'include/polyphase_float.h').read_text()
all_symbols=set(re.findall(r'`(mpfr_[a-z0-9_]+)`',inv))
types={'mpfr_t','mpfr_ptr','mpfr_srcptr','mpfr_prec_t','mpfr_exp_t','mpfr_rnd_t','mpfr_flags_t','mpfr_float128'}
functions=sorted(all_symbols-types)
missing=[m for m in functions if 'ppf_'+m[5:] not in hdr]
if len(functions)!=349:
    print(f'expected 349 inventory functions, found {len(functions)}',file=sys.stderr);sys.exit(1)
if missing:
    print('missing counterparts:',*missing,sep='\n  ',file=sys.stderr);sys.exit(1)
lib=root/'lib/libpolyphase_float.so'
if lib.exists():
    out=subprocess.check_output(['nm','-D','--defined-only',str(lib)],text=True)
    exports=set(re.findall(r'\b(ppf_[a-z0-9_]+)(?:@@?[A-Za-z0-9_.]+)?$',out,re.M))
    decls=set(re.findall(r'PPF_API\s+[^;]*?\b(ppf_[a-z0-9_]+)\s*\(',hdr,re.S))
    if exports!=decls:
        print(f'header/export mismatch: declarations={len(decls)} exports={len(exports)}',file=sys.stderr)
        print('not exported:',sorted(decls-exports),file=sys.stderr)
        print('not declared:',sorted(exports-decls),file=sys.stderr)
        sys.exit(1)
    print(f'API COVERAGE PASS: 349/349 direct counterparts; {len(exports)}/{len(decls)} exports')
else:
    print('API COVERAGE PASS: 349/349 direct counterparts (library not built)')
