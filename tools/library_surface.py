#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Identify the enabled conditional C surface of a runnable core library."""

from __future__ import annotations

import ctypes
from pathlib import Path


CONDITIONAL_GROUPS = {
    "float128": ("ppf_buildopt_float128_p", {
        "ppf_get_float128", "ppf_set_float128",
    }),
    "decimal": ("ppf_buildopt_decimal_p", {
        "ppf_get_decimal64", "ppf_set_decimal64",
        "ppf_get_decimal128", "ppf_set_decimal128",
    }),
}


def enabled_core_declarations(library: Path, declarations: set[str]
                              ) -> tuple[set[str], dict[str, bool]]:
    """Keep every unconditional and every enabled conditional declaration.

    The capability getters are unconditional public functions. Querying the
    built library avoids assuming that the audit host's compiler has the same
    binary128 or decimal support as the compiler that built the library.
    A cross-target binary that cannot run here cannot provide this evidence.
    """
    loaded = ctypes.CDLL(str(library.resolve()))
    expected = set(declarations)
    features: dict[str, bool] = {}
    for name, (getter_name, symbols) in CONDITIONAL_GROUPS.items():
        getter = getattr(loaded, getter_name)
        getter.argtypes = []
        getter.restype = ctypes.c_int
        value = getter()
        if value not in (0, 1):
            raise RuntimeError(f"{getter_name} returned invalid capability {value}")
        features[name] = bool(value)
        if not value:
            expected.difference_update(symbols)
    return expected, features
