#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Exercise the release export audit against real ELF fixture libraries.

The fixtures test symbol and feature contracts only. They are not numerical
implementations and do not replace the production library's C test suites.
"""

from __future__ import annotations

import argparse
import importlib.util
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("release_audit", ROOT / "tools/release_audit.py")
assert SPEC is not None and SPEC.loader is not None
AUDIT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(AUDIT)

FLOAT128 = {"ppf_get_float128", "ppf_set_float128"}
DECIMAL = {
    "ppf_get_decimal64", "ppf_set_decimal64",
    "ppf_get_decimal128", "ppf_set_decimal128",
}
COMPILER = os.environ.get("CC", "cc")


class ReleaseApiAuditTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.declarations = set()
        for relative in ("abi/symbols/core-0.11.txt", "abi/symbols/core-0.12.txt"):
            cls.declarations.update(
                line.strip()
                for line in (ROOT / relative).read_text(encoding="utf-8").splitlines()
                if line.strip() and not line.startswith("#")
            )
        if len(cls.declarations) != 437:
            raise AssertionError("expected the reviewed 437-function symbol inventory")
        if not FLOAT128 | DECIMAL <= cls.declarations:
            raise AssertionError("optional-format inventory is incomplete")

    def audit_fixture(
        self,
        float128: int,
        decimal: int,
        *,
        missing: set[str] | None = None,
        extra: set[str] | None = None,
    ) -> tuple[list[str], int]:
        symbols = self.declarations.copy()
        if float128 != 1:
            symbols.difference_update(FLOAT128)
        if decimal != 1:
            symbols.difference_update(DECIMAL)
        symbols.difference_update(missing or set())
        symbols.update(extra or set())

        probes = {
            "ppf_buildopt_float128_p": float128,
            "ppf_buildopt_decimal_p": decimal,
        }
        lines = []
        for name in sorted(symbols):
            self.assertRegex(name, r"^ppf_[a-z0-9_]+$")
            if name in probes:
                lines.append(f"int {name}(void) {{ return {probes[name]}; }}")
            else:
                lines.append(f"void {name}(void) {{}}")

        with tempfile.TemporaryDirectory(prefix="ppf-release-api-") as temporary:
            directory = Path(temporary)
            source = directory / "fixture.c"
            library = directory / "fixture.so"
            source.write_text("\n".join(lines) + "\n", encoding="utf-8")
            subprocess.run(
                [COMPILER, "-shared", "-fPIC", str(source), "-o", str(library)],
                check=True,
                text=True,
                capture_output=True,
            )
            failures: list[str] = []
            declarations, exports = AUDIT.audit_api(ROOT, library, failures)
        self.assertEqual(declarations, 437)
        self.assertEqual(exports, len(symbols))
        return failures, exports

    def test_all_feature_profiles(self) -> None:
        for float128, decimal in ((0, 0), (0, 1), (1, 0), (1, 1)):
            with self.subTest(float128=float128, decimal=decimal):
                failures, exports = self.audit_fixture(float128, decimal)
                self.assertEqual(failures, [])
                self.assertEqual(exports, 431 + 2 * float128 + 4 * decimal)

    def test_missing_unconditional_export_fails(self) -> None:
        failures, _ = self.audit_fixture(1, 0, missing={"ppf_add"})
        self.assertTrue(any("missing=['ppf_add']" in item for item in failures), failures)

    def test_each_missing_enabled_optional_export_fails(self) -> None:
        for name in sorted(FLOAT128 | DECIMAL):
            with self.subTest(symbol=name):
                failures, _ = self.audit_fixture(1, 1, missing={name})
                self.assertTrue(any(f"missing=['{name}']" in item for item in failures), failures)

    def test_exports_for_disabled_features_fail(self) -> None:
        for name in sorted(FLOAT128 | DECIMAL):
            with self.subTest(symbol=name):
                failures, _ = self.audit_fixture(0, 0, extra={name})
                self.assertTrue(any(f"extra=['{name}']" in item for item in failures), failures)

    def test_unknown_export_fails(self) -> None:
        failures, _ = self.audit_fixture(1, 0, extra={"ppf_unknown_export"})
        self.assertTrue(any("extra=['ppf_unknown_export']" in item for item in failures), failures)

    def test_invalid_feature_probe_fails(self) -> None:
        for float128, decimal in ((-1, 0), (1, 2)):
            with self.subTest(float128=float128, decimal=decimal):
                failures, _ = self.audit_fixture(float128, decimal)
                self.assertTrue(any("invalid feature value" in item for item in failures), failures)

    def test_missing_feature_probe_fails(self) -> None:
        for name in ("ppf_buildopt_float128_p", "ppf_buildopt_decimal_p"):
            with self.subTest(probe=name):
                failures, _ = self.audit_fixture(1, 0, missing={name})
                self.assertTrue(any("optional-format features" in item for item in failures), failures)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default=COMPILER, help="C compiler for ELF fixtures")
    arguments, remaining = parser.parse_known_args()
    COMPILER = arguments.cc
    unittest.main(argv=[__file__, *remaining])
