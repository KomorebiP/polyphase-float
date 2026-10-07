#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

from __future__ import annotations

import csv
import importlib.util
import json
import subprocess
import sys
import tempfile
from pathlib import Path

MACHINE = "1" * 64
SOFTWARE = "2" * 64
FIELDS = [
    "bits", "operation", "method", "method_workers", "repeat", "order",
    "seconds", "binding", "process_repeat", "verified",
    "machine_fingerprint_sha256", "software_fingerprint_sha256",
]


def rows_for(binding: str) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    order = 0
    for process in range(1, 5):
        for repeat in range(1, 6):
            # Robust rank-5 multiplication win. Portable AUTO is rank 3 here.
            cases = [
                (100000, "multiply", "ppf_rank3", 5, 1.20 + process * 0.001),
                (100000, "multiply", "ppf_rank5", 5, 1.00 + process * 0.001),
                # Stable rank-5 square win, but portable AUTO already selects rank 5.
                (600000, "square", "ppf_rank3", 5, 1.20 + process * 0.001),
                (600000, "square", "ppf_rank5", 5, 1.00 + process * 0.001),
                # Too small to clear the minimum-speedup gate.
                (200000, "multiply", "ppf_rank3", 5, 1.02 + process * 0.001),
                (200000, "multiply", "ppf_rank5", 5, 1.00 + process * 0.001),
            ]
            # Binding disagreement: close prefers rank 5; spread prefers rank 3.
            if binding == "close":
                cases.extend([
                    (100000, "fma", "ppf_rank3", 5, 1.20 + process * 0.001),
                    (100000, "fma", "ppf_rank5", 5, 1.00 + process * 0.001),
                ])
            else:
                cases.extend([
                    (100000, "fma", "ppf_rank3", 5, 1.00 + process * 0.001),
                    (100000, "fma", "ppf_rank5", 5, 1.20 + process * 0.001),
                ])
            for bits, operation, method, workers, seconds in cases:
                order += 1
                rows.append({
                    "bits": bits,
                    "operation": operation,
                    "method": method,
                    "method_workers": workers,
                    "repeat": repeat,
                    "order": order,
                    "seconds": f"{seconds:.9f}",
                    "binding": binding,
                    "process_repeat": process,
                    "verified": "true",
                    "machine_fingerprint_sha256": MACHINE,
                    "software_fingerprint_sha256": SOFTWARE,
                })
    return rows


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS)
        writer.writeheader()
        writer.writerows(rows)


def run_generator(root: Path, directory: Path, suffix: str) -> tuple[Path, Path]:
    header = directory / f"profile-{suffix}.h"
    report = directory / f"profile-{suffix}.json"
    command = [
        sys.executable,
        str(root / "tools" / "generate_product_profile.py"),
        "--input", str(directory / "close.csv"),
        "--input", str(directory / "spread.csv"),
        "--output-header", str(header),
        "--output-report", str(report),
        "--profile-name", "synthetic-profile",
        "--min-processes", "4",
        "--min-bindings", "2",
        "--min-speedup", "1.05",
        "--min-win-fraction", "0.75",
        "--bootstrap-draws", "2000",
    ]
    completed = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, check=False)
    if completed.returncode != 0:
        raise AssertionError(
            f"generator failed ({completed.returncode})\n"
            f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}"
        )
    return header, report


def test_calibration_resume_id(root: Path) -> None:
    module_path = root / "scripts" / "calibrate_product_profile.py"
    spec = importlib.util.spec_from_file_location("ppf_calibrator", module_path)
    if spec is None or spec.loader is None:
        raise AssertionError("could not load calibration module")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    if module.format_cpu_set({0, 1, 2, 4, 6, 7}) != "0-2,4,6-7":
        raise AssertionError("CPU-affinity normalization is unstable")
    existing = {"campaign_id": "persisted-campaign"}
    resumed = module.resolve_campaign_id(None, existing, MACHINE)
    if resumed != "persisted-campaign":
        raise AssertionError(f"resume changed campaign ID: {resumed!r}")
    explicit = module.resolve_campaign_id("explicit-campaign", existing, MACHINE)
    if explicit != "explicit-campaign":
        raise AssertionError(f"explicit campaign ID was ignored: {explicit!r}")


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    test_calibration_resume_id(root)
    with tempfile.TemporaryDirectory(prefix="ppf-profile-test-") as temp:
        directory = Path(temp)
        write_csv(directory / "close.csv", rows_for("close"))
        write_csv(directory / "spread.csv", rows_for("spread"))
        header_a, report_a = run_generator(root, directory, "a")
        header_b, report_b = run_generator(root, directory, "b")
        if header_a.read_bytes() != header_b.read_bytes():
            raise AssertionError("generated headers are not deterministic")
        if report_a.read_bytes() != report_b.read_bytes():
            raise AssertionError("generated reports are not deterministic")
        report = json.loads(report_a.read_text(encoding="utf-8"))
        rules = report["rules"]
        if len(rules) != 1:
            raise AssertionError(f"expected one accepted rule, found {len(rules)}")
        rule = rules[0]
        expected = {
            "operation": "multiply",
            "min_precision": 100000,
            "max_precision": 100000,
            "min_workers": 5,
            "max_workers": 5,
            "algorithm": "rank5",
        }
        for key, value in expected.items():
            if rule.get(key) != value:
                raise AssertionError(f"unexpected rule field {key}: {rule.get(key)!r}")
        text = header_a.read_text(encoding="utf-8")
        for token in (
            'PPF_PRODUCT_PROFILE_NAME "synthetic-profile"',
            "PPF_PRODUCT_PROFILE_RULE_COUNT 1u",
            "PPF_PRODUCT_OP_MULTIPLY",
            "PPF_PRODUCT_TOOM3",
        ):
            if token not in text:
                raise AssertionError(f"generated header lacks {token!r}")
        rejected = {
            (item["bits"], item["operation"]): item
            for item in report["conditions"] if not item["accepted_rule"]
        }
        if "binding campaigns selected different winners" not in \
                rejected[(100000, "fma")]["reason"]:
            raise AssertionError("binding disagreement was not rejected")
        if "matches the portable fallback" not in \
                rejected[(600000, "square")]["reason"]:
            raise AssertionError("portable-equivalent rule was not omitted")
    print("PRODUCT PROFILE GENERATOR TESTS PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
