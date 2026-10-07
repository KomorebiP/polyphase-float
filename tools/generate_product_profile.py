#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Generate a conservative build-time Polyphase product-planner profile.

A profile is machine- and software-fingerprint specific.  Rules are emitted
only when rank-3 and rank-5 measurements have enough independent process
replicates, agree across the requested binding campaigns, clear a minimum
speedup threshold, and have a bootstrap interval wholly on the winning side
of parity.  The portable planner remains the fallback for every uncovered
condition.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import random
import re
import statistics
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence

OPERATION_ENUM = {
    "multiply": "PPF_PRODUCT_OP_MULTIPLY",
    "square": "PPF_PRODUCT_OP_SQUARE",
    "fma": "PPF_PRODUCT_OP_FMA",
}
ALGORITHM_ENUM = {
    "rank3": "PPF_PRODUCT_KARATSUBA3",
    "rank5": "PPF_PRODUCT_TOOM3",
}
METHOD_ALGORITHM = {
    "ppf_rank3": "rank3",
    "ppf_rank5": "rank5",
}


@dataclass(frozen=True)
class Observation:
    source: str
    source_sha256: str
    machine: str
    software: str
    binding: str
    process_repeat: int
    bits: int
    operation: str
    workers: int
    method: str
    repeat: int
    seconds: float


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def parse_bool(value: str | None) -> bool:
    if value is None or value == "":
        return True
    return value.strip().lower() in {"1", "true", "yes", "on"}


def percentile(values: Sequence[float], probability: float) -> float:
    if not values:
        raise ValueError("percentile of empty values")
    ordered = sorted(values)
    if len(ordered) == 1:
        return float(ordered[0])
    position = (len(ordered) - 1) * probability
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return float(ordered[lower])
    return float(
        ordered[lower] * (upper - position)
        + ordered[upper] * (position - lower)
    )


def bootstrap_median_interval(values: Sequence[float], draws: int,
                              seed: int) -> tuple[float, float]:
    rng = random.Random(seed)
    count = len(values)
    samples = [
        statistics.median(values[rng.randrange(count)] for _ in range(count))
        for _ in range(draws)
    ]
    return percentile(samples, 0.025), percentile(samples, 0.975)


def stable_seed(parts: Iterable[object]) -> int:
    material = "|".join(str(part) for part in parts).encode("utf-8")
    return int.from_bytes(hashlib.sha256(material).digest()[:8], "big")


def portable_algorithm(operation: str, bits: int, workers: int,
                       gmp_numb_bits: int) -> str:
    limbs = (bits + gmp_numb_bits - 1) // gmp_numb_bits
    if operation == "square" and workers >= 5 and limbs >= 8192:
        return "rank5"
    if workers >= 3 and limbs >= 1536:
        return "rank3"
    return "full"


def sanitize_name(value: str) -> str:
    if not re.fullmatch(r"[A-Za-z0-9._-]+", value):
        raise ValueError(
            "profile name may contain only letters, digits, period, "
            "underscore, and hyphen"
        )
    return value


def read_observations(paths: Sequence[Path], allow_missing_fingerprint: bool
                      ) -> tuple[list[Observation], list[dict]]:
    observations: list[Observation] = []
    sources: list[dict] = []
    for path in paths:
        if not path.is_file():
            raise ValueError(f"input does not exist: {path}")
        source_hash = sha256_file(path)
        accepted = skipped = 0
        with path.open(newline="", encoding="utf-8") as stream:
            reader = csv.DictReader(stream)
            required = {
                "bits", "operation", "method", "method_workers",
                "repeat", "seconds",
            }
            missing = required - set(reader.fieldnames or [])
            if missing:
                raise ValueError(
                    f"{path}: missing required columns: {sorted(missing)}"
                )
            for row in reader:
                method = row.get("method", "")
                operation = row.get("operation", "")
                if method not in METHOD_ALGORITHM or operation not in OPERATION_ENUM:
                    skipped += 1
                    continue
                if not parse_bool(row.get("verified")):
                    raise ValueError(f"{path}: contains an unverified planner row")
                machine = (row.get("machine_fingerprint_sha256") or "").strip()
                software = (row.get("software_fingerprint_sha256") or "").strip()
                if not machine or not software:
                    if not allow_missing_fingerprint:
                        raise ValueError(
                            f"{path}: planner row lacks machine/software fingerprint"
                        )
                    machine = machine or "missing-machine-fingerprint"
                    software = software or "missing-software-fingerprint"
                binding = (row.get("binding") or path.parent.name or "unknown").strip()
                process_text = (row.get("process_repeat") or "1").strip()
                try:
                    seconds = float(row["seconds"])
                    observation = Observation(
                        source=str(path.resolve()),
                        source_sha256=source_hash,
                        machine=machine,
                        software=software,
                        binding=binding,
                        process_repeat=int(process_text),
                        bits=int(row["bits"]),
                        operation=operation,
                        workers=int(row["method_workers"]),
                        method=method,
                        repeat=int(row["repeat"]),
                        seconds=seconds,
                    )
                except (TypeError, ValueError) as error:
                    raise ValueError(f"{path}: malformed row: {row}") from error
                if observation.seconds <= 0 or observation.bits <= 0 or \
                        observation.workers <= 0 or observation.process_repeat <= 0:
                    raise ValueError(f"{path}: nonpositive planner field: {row}")
                observations.append(observation)
                accepted += 1
        sources.append({
            "label": f"{path.parent.name}/{path.name}",
            "sha256": source_hash,
            "accepted_rows": accepted,
            "skipped_rows": skipped,
        })
    if not observations:
        raise ValueError("inputs contained no rank-3/rank-5 observations")
    return observations, sources


def analyze_binding(values: list[Observation], *, min_processes: int,
                    min_speedup: float, min_win_fraction: float,
                    draws: int) -> dict:
    by_process_method: dict[tuple[int, str], list[float]] = defaultdict(list)
    for value in values:
        by_process_method[(value.process_repeat, value.method)].append(value.seconds)
    ratios: list[float] = []
    processes: list[int] = []
    for process in sorted({key[0] for key in by_process_method}):
        rank3 = by_process_method.get((process, "ppf_rank3"))
        rank5 = by_process_method.get((process, "ppf_rank5"))
        if not rank3 or not rank5:
            continue
        ratios.append(statistics.median(rank3) / statistics.median(rank5))
        processes.append(process)
    result: dict = {
        "processes": processes,
        "process_count": len(ratios),
        "rank3_over_rank5_process_ratios": ratios,
        "winner": None,
        "reason": "",
    }
    if len(ratios) < min_processes:
        result["reason"] = (
            f"requires {min_processes} independent processes; found {len(ratios)}"
        )
        return result
    median_ratio = float(statistics.median(ratios))
    low, high = bootstrap_median_interval(
        ratios, draws, stable_seed([
            values[0].machine, values[0].software, values[0].binding,
            values[0].bits, values[0].operation, values[0].workers,
        ])
    )
    rank5_wins = sum(value > 1.0 for value in ratios)
    rank3_wins = sum(value < 1.0 for value in ratios)
    required_wins = math.ceil(min_win_fraction * len(ratios))
    result.update({
        "median_rank3_over_rank5": median_ratio,
        "bootstrap_95_low": low,
        "bootstrap_95_high": high,
        "rank5_process_wins": rank5_wins,
        "rank3_process_wins": rank3_wins,
        "required_process_wins": required_wins,
    })
    if median_ratio >= min_speedup and low > 1.0 and rank5_wins >= required_wins:
        result["winner"] = "rank5"
        result["reason"] = "rank 5 cleared speedup, interval, and win-fraction gates"
    elif median_ratio <= 1.0 / min_speedup and high < 1.0 and \
            rank3_wins >= required_wins:
        result["winner"] = "rank3"
        result["reason"] = "rank 3 cleared speedup, interval, and win-fraction gates"
    else:
        result["reason"] = "evidence did not clear all stability gates"
    return result


def canonical_json(value: object) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":"),
                       ensure_ascii=True) + "\n").encode("utf-8")


def build_profile(observations: list[Observation], sources: list[dict],
                  args: argparse.Namespace) -> dict:
    machines = sorted({value.machine for value in observations})
    software = sorted({value.software for value in observations})
    if len(machines) != 1 or len(software) != 1:
        raise ValueError(
            "one generated profile must contain exactly one machine and one "
            "software fingerprint"
        )
    grouped: dict[tuple[int, str, int, str], list[Observation]] = defaultdict(list)
    for value in observations:
        grouped[(value.bits, value.operation, value.workers, value.binding)].append(value)
    condition_bindings: dict[tuple[int, str, int], dict[str, dict]] = defaultdict(dict)
    for (bits, operation, workers, binding), values in sorted(grouped.items()):
        condition_bindings[(bits, operation, workers)][binding] = analyze_binding(
            values,
            min_processes=args.min_processes,
            min_speedup=args.min_speedup,
            min_win_fraction=args.min_win_fraction,
            draws=args.bootstrap_draws,
        )
    rules: list[dict] = []
    conditions: list[dict] = []
    for (bits, operation, workers), bindings in sorted(condition_bindings.items()):
        winners = [entry.get("winner") for entry in bindings.values()]
        accepted = False
        reason: str
        selected: str | None = None
        if len(bindings) < args.min_bindings:
            reason = f"requires {args.min_bindings} bindings; found {len(bindings)}"
        elif any(winner is None for winner in winners):
            reason = "at least one binding did not clear the stability gates"
        elif len(set(winners)) != 1:
            reason = "binding campaigns selected different winners"
        else:
            selected = winners[0]
            portable = portable_algorithm(operation, bits, workers,
                                          args.gmp_numb_bits)
            if selected == portable:
                reason = "stable winner matches the portable fallback"
            else:
                accepted = True
                reason = "stable cross-binding winner overrides portable fallback"
                limbs = (bits + args.gmp_numb_bits - 1) // args.gmp_numb_bits
                rules.append({
                    "operation": operation,
                    "operation_enum": OPERATION_ENUM[operation],
                    "min_precision": bits,
                    "max_precision": bits,
                    "min_limbs": limbs,
                    "max_limbs": limbs,
                    "min_workers": workers,
                    "max_workers": workers,
                    "algorithm": selected,
                    "algorithm_enum": ALGORITHM_ENUM[selected],
                })
        conditions.append({
            "bits": bits,
            "operation": operation,
            "workers": workers,
            "bindings": bindings,
            "selected_winner": selected,
            "portable_algorithm": portable_algorithm(
                operation, bits, workers, args.gmp_numb_bits
            ),
            "accepted_rule": accepted,
            "reason": reason,
        })
    requested_name = args.profile_name or (
        f"ppf-{machines[0][:12]}-{software[0][:12]}"
    )
    name = sanitize_name(requested_name)
    payload = {
        "schema_version": 1,
        "profile_name": name,
        "machine_fingerprint_sha256": machines[0],
        "software_fingerprint_sha256": software[0],
        "criteria": {
            "minimum_independent_processes_per_binding": args.min_processes,
            "minimum_bindings": args.min_bindings,
            "minimum_speedup": args.min_speedup,
            "minimum_process_win_fraction": args.min_win_fraction,
            "bootstrap_draws": args.bootstrap_draws,
            "confidence": 0.95,
            "gmp_numb_bits": args.gmp_numb_bits,
        },
        "sources": sources,
        "rules": rules,
        "conditions": conditions,
    }
    payload_hash = hashlib.sha256(canonical_json(payload)).hexdigest()
    payload["profile_sha256"] = payload_hash
    return payload


def c_unsigned(value: int) -> str:
    return f"{value}u"


def write_header(path: Path, profile: dict) -> None:
    rules = profile["rules"]
    lines = [
        "/*",
        " * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors",
        " * SPDX-License-Identifier: LGPL-3.0-or-later",
        " *",
        " * Generated by tools/generate_product_profile.py. Do not edit by hand.",
        " */",
        "",
        "#ifndef PPF_PRODUCT_PROFILE_BUILD_H",
        "#define PPF_PRODUCT_PROFILE_BUILD_H",
        "",
        f'#define PPF_PRODUCT_PROFILE_NAME "{profile["profile_name"]}"',
        f'#define PPF_PRODUCT_PROFILE_SHA256 "{profile["profile_sha256"]}"',
        f"#define PPF_PRODUCT_PROFILE_RULE_COUNT {len(rules)}u",
        "",
    ]
    if rules:
        lines.append(
            "static const ppf_product_profile_rule_t "
            "ppf_product_profile_rules[PPF_PRODUCT_PROFILE_RULE_COUNT] = {"
        )
        for rule in rules:
            lines.extend([
                "    {",
                f"        {rule['operation_enum']},",
                f"        {c_unsigned(rule['min_precision'])},",
                f"        {c_unsigned(rule['max_precision'])},",
                f"        {c_unsigned(rule['min_limbs'])},",
                f"        {c_unsigned(rule['max_limbs'])},",
                f"        {c_unsigned(rule['min_workers'])},",
                f"        {c_unsigned(rule['max_workers'])},",
                f"        {rule['algorithm_enum']}",
                "    },",
            ])
        lines.append("};")
    else:
        lines.extend([
            "static const ppf_product_profile_rule_t ppf_product_profile_rules[1] = {",
            "    {0, 0, 0, 0, 0, 0, 0, PPF_PRODUCT_AUTO}",
            "};",
        ])
    lines.extend(["", "#endif", ""])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", action="append", required=True,
                        help="rank_raw.csv; pass once per binding campaign")
    parser.add_argument("--output-header", required=True)
    parser.add_argument("--output-report", required=True)
    parser.add_argument("--profile-name")
    parser.add_argument("--min-processes", type=int, default=4)
    parser.add_argument("--min-bindings", type=int, default=2)
    parser.add_argument("--min-speedup", type=float, default=1.05)
    parser.add_argument("--min-win-fraction", type=float, default=0.75)
    parser.add_argument("--bootstrap-draws", type=int, default=20000)
    parser.add_argument("--gmp-numb-bits", type=int, default=64)
    parser.add_argument("--allow-missing-fingerprint", action="store_true")
    args = parser.parse_args()
    if args.min_processes < 2:
        parser.error("--min-processes must be at least 2")
    if args.min_bindings < 1:
        parser.error("--min-bindings must be at least 1")
    if not (1.0 < args.min_speedup < 2.0):
        parser.error("--min-speedup must be between 1 and 2")
    if not (0.5 < args.min_win_fraction <= 1.0):
        parser.error("--min-win-fraction must be in (0.5, 1]")
    if args.bootstrap_draws < 1000:
        parser.error("--bootstrap-draws must be at least 1000")
    if args.gmp_numb_bits < 8:
        parser.error("--gmp-numb-bits is implausibly small")
    try:
        input_paths = [Path(value).resolve() for value in args.input]
        observations, sources = read_observations(
            input_paths, args.allow_missing_fingerprint
        )
        profile = build_profile(observations, sources, args)
        output_header = Path(args.output_header).resolve()
        output_report = Path(args.output_report).resolve()
        write_header(output_header, profile)
        output_report.parent.mkdir(parents=True, exist_ok=True)
        output_report.write_bytes(canonical_json(profile))
    except ValueError as error:
        parser.error(str(error))
    print(json.dumps({
        "profile_name": profile["profile_name"],
        "profile_sha256": profile["profile_sha256"],
        "rule_count": len(profile["rules"]),
        "header": str(output_header),
        "report": str(output_report),
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
