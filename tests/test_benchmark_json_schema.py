#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Validate retained-sample JSON emitted by public benchmark executables."""

from __future__ import annotations

import argparse
import json
import math
import statistics
import subprocess
import sys
from pathlib import Path
from typing import Any


def run_json(command: list[str]) -> dict[str, Any]:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if result.returncode:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
    text = result.stdout.strip()
    try:
        value = json.loads(text)
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"invalid benchmark JSON: {text!r}") from exc
    if not isinstance(value, dict):
        raise RuntimeError("benchmark output is not a JSON object")
    return value


def validate(record: dict[str, Any], expected_method: str, expected_samples: int) -> None:
    if record.get("schema_version") != 2:
        raise RuntimeError(f"unexpected schema version: {record}")
    if record.get("method") != expected_method:
        raise RuntimeError(
            f"unexpected method {record.get('method')!r}; expected {expected_method!r}"
        )
    if record.get("verified") is not True:
        raise RuntimeError("benchmark record is not correctness-gated")
    if record.get("sample_statistic") != "median":
        raise RuntimeError("benchmark record does not identify the median statistic")
    samples = record.get("samples_seconds")
    if not isinstance(samples, list) or len(samples) != expected_samples:
        raise RuntimeError(
            f"expected {expected_samples} retained samples, got {samples!r}"
        )
    values = [float(value) for value in samples]
    if any(not math.isfinite(value) or value <= 0.0 for value in values):
        raise RuntimeError(f"invalid timing sample: {values!r}")
    if int(record.get("sample_count", -1)) != len(values):
        raise RuntimeError("sample_count does not match samples_seconds")
    observed = statistics.median(values)
    reported = float(record["seconds"])
    tolerance = max(1e-12, abs(observed) * 2e-9)
    if abs(observed - reported) > tolerance:
        raise RuntimeError(
            f"reported median {reported} does not match retained samples {observed}"
        )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--speedup", type=Path, required=True)
    parser.add_argument("--constants", type=Path, required=True)
    args = parser.parse_args()

    speedup = run_json(
        [str(args.speedup), "ppf_mul_1t", "10000", "1", "1", "3"]
    )
    validate(speedup, "ppf_mul_1t", 3)

    constants = run_json(
        [str(args.constants), "ppf_pi_cold", "1000", "1", "3"]
    )
    validate(constants, "ppf_pi_cold", 3)
    print("benchmark JSON schema and raw-sample retention: PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
