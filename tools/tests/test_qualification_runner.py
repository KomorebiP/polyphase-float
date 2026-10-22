#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Regression checks for qualification metadata parsing."""

import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("qualification", ROOT / "tools/qualify_release.py")
assert SPEC is not None and SPEC.loader is not None
QUALIFICATION = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(QUALIFICATION)


class CacheParsingTests(unittest.TestCase):
    def test_blank_lines_and_documentation_do_not_enter_keys(self):
        text = "\n//MPFR official include directory\nMPFR_INCLUDE_DIR:PATH=/usr/include\n\n//GMP library\nGMP_LIBRARY:FILEPATH=/usr/lib/libgmp.so\n"
        self.assertEqual(QUALIFICATION.parse_cmake_cache(text), {
            "MPFR_INCLUDE_DIR": "/usr/include", "GMP_LIBRARY": "/usr/lib/libgmp.so"})

    def test_values_keep_equals_and_colons(self):
        self.assertEqual(QUALIFICATION.parse_cmake_cache(
            "# header\nVALUE:STRING=a=b:c\nEMPTY:STRING=\n"),
            {"VALUE": "a=b:c", "EMPTY": ""})

    def test_absent_header_is_not_invented(self):
        self.assertNotIn("MPFR_INCLUDE_DIR", QUALIFICATION.parse_cmake_cache(
            "// No MPFR development header detected\nMPFR_LIBRARY:FILEPATH=/lib/libmpfr.so.6\n"))


if __name__ == "__main__":
    unittest.main()
