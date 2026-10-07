#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

from __future__ import annotations
import pathlib
import subprocess
import sys

path = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "lib/libpolyphase_float.so")
if not path.exists():
    raise SystemExit(f"missing library: {path}")
p = subprocess.run(["nm", "-D", "--undefined-only", str(path)], text=True,
                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
forbidden = []
for line in p.stdout.splitlines():
    symbol = line.split()[-1] if line.split() else ""
    if symbol.startswith("mpfr_"):
        forbidden.append(symbol)
if forbidden:
    print("INDEPENDENCE AUDIT FAILED")
    for symbol in forbidden:
        print(symbol)
    raise SystemExit(1)
print("INDEPENDENCE AUDIT: PASS (object library imports no mpfr_* symbols)")
