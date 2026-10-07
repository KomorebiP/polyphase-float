#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Run independent rank-3/rank-5 campaigns and generate a planner profile."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import platform
import random
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Any, Iterable

SCHEMA = 1
EXPECTED_METHODS = {
    "multiply": {"ppf_full_1t", "ppf_rank3", "ppf_rank5", "ppf_window", "ppf_auto", "mpfr_1t"},
    "square": {"ppf_full_1t", "ppf_rank3", "ppf_rank5", "ppf_window", "ppf_auto", "mpfr_1t"},
    "fma": {"ppf_full_1t", "ppf_rank3", "ppf_rank5", "ppf_auto", "mpfr_1t"},
}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def canonical_json(value: object) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":"),
                       ensure_ascii=True) + "\n").encode("utf-8")


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_bytes(canonical_json(value))
    os.replace(temporary, path)


def command_text(command: list[str], *, env: dict[str, str] | None = None,
                 timeout: int = 60, check: bool = True) -> str:
    completed = subprocess.run(
        command, env=env, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, timeout=timeout, check=False,
    )
    if check and completed.returncode != 0:
        raise RuntimeError(
            f"command failed ({completed.returncode}): {' '.join(command)}\n"
            f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}"
        )
    return completed.stdout.strip()


def first_line(command: list[str]) -> str:
    text = command_text(command, check=False)
    return text.splitlines()[0] if text else "unavailable"


def read_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8", errors="replace").strip()
    except OSError:
        return "unavailable"


def format_cpu_set(cpus: Iterable[int]) -> str:
    ordered = sorted(set(int(cpu) for cpu in cpus))
    if not ordered:
        return "unavailable"
    ranges: list[str] = []
    start = previous = ordered[0]
    for cpu in ordered[1:]:
        if cpu == previous + 1:
            previous = cpu
            continue
        ranges.append(str(start) if start == previous else f"{start}-{previous}")
        start = previous = cpu
    ranges.append(str(start) if start == previous else f"{start}-{previous}")
    return ",".join(ranges)


def allowed_cpu_list() -> str:
    try:
        return format_cpu_set(os.sched_getaffinity(0))
    except (AttributeError, OSError):
        text = first_line(["taskset", "-pc", str(os.getpid())])
        if ":" in text:
            text = text.rsplit(":", 1)[1].strip()
        return text or "unavailable"


def collect_environment(binary: Path) -> dict[str, Any]:
    machine = {
        "platform_machine": platform.machine(),
        "platform_system": platform.system(),
        "lscpu": command_text(["lscpu", "-J"], check=False),
        "cpu_model": next(
            (line.split(":", 1)[1].strip()
             for line in read_text(Path("/proc/cpuinfo")).splitlines()
             if line.lower().startswith("model name") and ":" in line),
            "unavailable",
        ),
        "online_cpus": read_text(Path("/sys/devices/system/cpu/online")),
        "allowed_cpus": allowed_cpu_list(),
    }
    software = {
        "platform": platform.platform(),
        "uname": first_line(["uname", "-a"]),
        "cc": first_line([os.environ.get("CC", "cc"), "--version"]),
        "ld": first_line(["ld", "--version"]),
        "python": sys.version.splitlines()[0],
        "gmp": first_line(["pkg-config", "--modversion", "gmp"]),
        "mpfr": first_line(["pkg-config", "--modversion", "mpfr"]),
        "benchmark_binary_sha256": sha256_file(binary),
    }
    return {
        "schema_version": SCHEMA,
        "captured_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "machine": machine,
        "software": software,
        "machine_fingerprint_sha256": hashlib.sha256(canonical_json(machine)).hexdigest(),
        "software_fingerprint_sha256": hashlib.sha256(canonical_json(software)).hexdigest(),
    }


def parse_list(text: str, minimum: int) -> list[int]:
    values = sorted({int(item.strip()) for item in text.split(",") if item.strip()})
    if not values or values[0] < minimum:
        raise ValueError(f"values must be integers >= {minimum}")
    return values


def safe_name(value: str) -> str:
    if not re.fullmatch(r"[A-Za-z0-9._-]+", value):
        raise ValueError("campaign ID contains unsupported characters")
    return value


def resolve_campaign_id(requested: str | None,
                        existing_config: dict[str, Any] | None,
                        machine_fingerprint: str) -> str:
    if requested:
        return safe_name(requested)
    if existing_config is not None:
        existing = existing_config.get("campaign_id")
        if not isinstance(existing, str) or not existing:
            raise ValueError("existing run_config.json has no valid campaign_id")
        return safe_name(existing)
    return safe_name(
        time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
        + "-planner-" + machine_fingerprint[:8]
    )


def parse_benchmark(text: str, expected_bits: int, workers: int,
                    repetitions: int) -> list[dict[str, str]]:
    lines = [line for line in text.splitlines() if line.strip()]
    index = next((i for i, line in enumerate(lines)
                  if line.startswith("bits,operation,method,")), None)
    if index is None:
        raise RuntimeError(f"benchmark emitted no CSV header:\n{text}")
    rows = list(csv.DictReader(lines[index:]))
    if len(rows) != 17 * repetitions:
        raise RuntimeError(
            f"expected {17 * repetitions} planner rows; received {len(rows)}"
        )
    observed: dict[str, set[str]] = {key: set() for key in EXPECTED_METHODS}
    repeats: dict[tuple[str, str], set[int]] = {}
    for row in rows:
        bits = int(row["bits"])
        operation = row["operation"]
        method = row["method"]
        seconds = float(row["seconds"])
        if bits != expected_bits or operation not in EXPECTED_METHODS or seconds <= 0:
            raise RuntimeError(f"malformed planner row: {row}")
        observed[operation].add(method)
        repeats.setdefault((operation, method), set()).add(int(row["repeat"]))
        if method not in {"ppf_full_1t", "mpfr_1t"} and \
                int(row["method_workers"]) != workers:
            raise RuntimeError(f"worker mismatch in planner row: {row}")
    if observed != EXPECTED_METHODS:
        raise RuntimeError(f"planner method inventory mismatch: {observed}")
    expected_repeats = set(range(1, repetitions + 1))
    if any(values != expected_repeats for values in repeats.values()):
        raise RuntimeError("planner repetitions are incomplete")
    return rows


def record_id(process_repeat: int, binding: str, bits: int, workers: int) -> str:
    return f"p{process_repeat:03d}-{binding}-{bits}-w{workers}"


def write_record(path: Path, rows: list[dict[str, str]], enrichment: dict[str, object]) -> None:
    if not rows:
        raise ValueError("empty record")
    fields = list(rows[0].keys()) + list(enrichment.keys())
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary_name = tempfile.mkstemp(prefix=path.name + ".", suffix=".tmp",
                                          dir=path.parent)
    try:
        with os.fdopen(fd, "w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=fields)
            writer.writeheader()
            for row in rows:
                combined = dict(row)
                combined.update(enrichment)
                writer.writerow(combined)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary_name, path)
    finally:
        if os.path.exists(temporary_name):
            os.unlink(temporary_name)


def combine_records(records: Iterable[Path], destination: Path) -> int:
    all_rows: list[dict[str, str]] = []
    fields: list[str] = []
    for path in sorted(records):
        with path.open(newline="", encoding="utf-8") as stream:
            rows = list(csv.DictReader(stream))
        for row in rows:
            for field in row:
                if field not in fields:
                    fields.append(field)
            all_rows.append(row)
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(destination.suffix + ".tmp")
    with temporary.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(all_rows)
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, destination)
    return len(all_rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default=".")
    parser.add_argument("--binary", default="build/planner-calibration/benchmark_product_planner")
    parser.add_argument("--output", default="results/product_profile_calibration")
    parser.add_argument("--bits", default="1000000,5000000,10000000")
    parser.add_argument("--workers", default="3,4,5,6,8,9,10")
    parser.add_argument("--bindings", default="close,spread")
    parser.add_argument("--process-repetitions", type=int, default=4)
    parser.add_argument("--repetitions", type=int, default=9)
    parser.add_argument("--inner-repetitions", type=int, default=1)
    parser.add_argument("--seed", type=int, default=20260814)
    parser.add_argument("--timeout-seconds", type=int, default=1800)
    parser.add_argument("--campaign-id")
    parser.add_argument("--profile-name")
    parser.add_argument("--fresh", action="store_true")
    args = parser.parse_args()

    root = Path(args.root).resolve()
    binary = (root / args.binary).resolve()
    output_argument = Path(args.output)
    if output_argument.is_absolute() or ".." in output_argument.parts:
        parser.error("--output must be a relative path inside --root")
    output = (root / output_argument).resolve()
    if root != output and root not in output.parents:
        parser.error("--output resolves outside --root")
    try:
        bits_list = parse_list(args.bits, 128)
        workers_list = parse_list(args.workers, 1)
        bindings = [item.strip() for item in args.bindings.split(",") if item.strip()]
        if len(set(bindings)) != len(bindings) or not bindings:
            raise ValueError("bindings must be unique and nonempty")
        if any(binding not in {"close", "spread"} for binding in bindings):
            raise ValueError("bindings must be close and/or spread")
        if args.process_repetitions < 2 or args.repetitions < 3:
            raise ValueError("requires at least two processes and three repetitions")
        if not binary.is_file():
            raise ValueError(f"planner benchmark binary is missing: {binary}")
    except ValueError as error:
        parser.error(str(error))

    if args.fresh and output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True, exist_ok=True)
    environment = collect_environment(binary)
    environment_path = output / "environment.json"
    if environment_path.exists():
        existing_environment = json.loads(environment_path.read_text(encoding="utf-8"))
        for key in ("machine_fingerprint_sha256", "software_fingerprint_sha256"):
            if existing_environment.get(key) != environment.get(key):
                raise RuntimeError(
                    f"environment fingerprint changed for resumed campaign: {key}"
                )
        environment = existing_environment
    else:
        write_json(environment_path, environment)

    config_path = output / "run_config.json"
    existing_config: dict[str, Any] | None = None
    if config_path.exists():
        loaded_config = json.loads(config_path.read_text(encoding="utf-8"))
        if not isinstance(loaded_config, dict):
            raise RuntimeError("run_config.json must contain a JSON object")
        existing_config = loaded_config
    try:
        campaign_id = resolve_campaign_id(
            args.campaign_id, existing_config,
            environment["machine_fingerprint_sha256"],
        )
    except ValueError as error:
        parser.error(str(error))
    config = {
        "schema_version": SCHEMA,
        "campaign_id": campaign_id,
        "bits": bits_list,
        "workers": workers_list,
        "bindings": bindings,
        "process_repetitions": args.process_repetitions,
        "repetitions": args.repetitions,
        "inner_repetitions": args.inner_repetitions,
        "seed": args.seed,
        "binary_sha256": sha256_file(binary),
        "machine_fingerprint_sha256": environment["machine_fingerprint_sha256"],
        "software_fingerprint_sha256": environment["software_fingerprint_sha256"],
    }
    if existing_config is not None:
        if existing_config != config:
            raise RuntimeError(
                "resume configuration differs from run_config.json; use --fresh "
                "or a different output directory"
            )
    else:
        write_json(config_path, config)
    config_hash = sha256_file(config_path)

    conditions = [(bits, workers) for bits in bits_list for workers in workers_list]
    schedule_rows: list[dict[str, Any]] = []
    base_env = os.environ.copy()
    for process_repeat in range(1, args.process_repetitions + 1):
        rng = random.Random(args.seed ^ process_repeat ^ 0x5A17D00D)
        order = list(conditions)
        rng.shuffle(order)
        binding_order = list(bindings)
        if len(binding_order) == 2 and process_repeat % 2 == 0:
            binding_order.reverse()
        schedule_rows.append({
            "process_repeat": process_repeat,
            "binding_order": binding_order,
            "condition_order": [f"{bits}:w{workers}" for bits, workers in order],
        })
        for binding_position, binding in enumerate(binding_order, 1):
            env = base_env.copy()
            env.update({
                "LC_ALL": "C",
                "LANG": "C",
                "OMP_DYNAMIC": "FALSE",
                "OMP_PROC_BIND": binding,
                "OMP_PLACES": "cores",
            })
            for execution_order, (bits, workers) in enumerate(order, 1):
                rid = record_id(process_repeat, binding, bits, workers)
                record_path = output / "records" / binding / f"{rid}.csv"
                if record_path.exists():
                    print(f"SKIP {rid}", flush=True)
                    continue
                command = [str(binary), str(bits), str(args.repetitions),
                           str(workers), str(args.inner_repetitions)]
                print(f"RUN {rid}", flush=True)
                text = command_text(command, env=env, timeout=args.timeout_seconds)
                rows = parse_benchmark(text, bits, workers, args.repetitions)
                enrichment = {
                    "campaign_id": campaign_id,
                    "record_id": rid,
                    "binding": binding,
                    "binding_position": binding_position,
                    "process_repeat": process_repeat,
                    "execution_order": execution_order,
                    "verified": "true",
                    "run_config_sha256": config_hash,
                    "machine_fingerprint_sha256": environment["machine_fingerprint_sha256"],
                    "software_fingerprint_sha256": environment["software_fingerprint_sha256"],
                }
                write_record(record_path, rows, enrichment)

    write_json(output / "execution_schedule.json", schedule_rows)
    combined: dict[str, int] = {}
    rank_inputs: list[Path] = []
    for binding in bindings:
        rank_path = output / binding / "rank_raw.csv"
        count = combine_records(
            (output / "records" / binding).glob("*.csv"), rank_path
        )
        combined[binding] = count
        rank_inputs.append(rank_path)

    profile_header = output / "generated" / "ppf_product_profile_build.h"
    profile_report = output / "generated" / "product_profile_report.json"
    generator = root / "tools" / "generate_product_profile.py"
    profile_name = args.profile_name or (
        "ppf-" + environment["machine_fingerprint_sha256"][:12]
        + "-" + environment["software_fingerprint_sha256"][:12]
    )
    generator_command = [
        sys.executable, str(generator),
        "--output-header", str(profile_header),
        "--output-report", str(profile_report),
        "--profile-name", profile_name,
        "--min-processes", str(args.process_repetitions),
        "--min-bindings", str(len(bindings)),
    ]
    for rank_input in rank_inputs:
        generator_command.extend(["--input", str(rank_input)])
    generator_result = command_text(generator_command, timeout=300)
    profile = json.loads(profile_report.read_text(encoding="utf-8"))

    report_lines = [
        "# Polyphase product-planner calibration",
        "",
        f"Campaign: `{campaign_id}`",
        "",
        f"Machine fingerprint: `{environment['machine_fingerprint_sha256']}`",
        "",
        f"Software fingerprint: `{environment['software_fingerprint_sha256']}`",
        "",
        f"Independent process rounds: **{args.process_repetitions}**",
        "",
        f"Bindings: **{', '.join(bindings)}**",
        "",
        f"Accepted profile rules: **{len(profile['rules'])}**",
        "",
        "The portable planner remains the fallback for every condition not "
        "covered by an accepted rule.",
        "",
        "## Accepted rules",
        "",
        "| Bits | Operation | Workers | Portable | Selected |",
        "|---:|---|---:|---|---|",
    ]
    if profile["rules"]:
        condition_index = {
            (item["bits"], item["operation"], item["workers"]): item
            for item in profile["conditions"]
        }
        for rule in profile["rules"]:
            condition = condition_index[(rule["min_precision"], rule["operation"],
                                         rule["min_workers"])]
            report_lines.append(
                f"| {rule['min_precision']:,} | {rule['operation']} | "
                f"{rule['min_workers']} | {condition['portable_algorithm']} | "
                f"{rule['algorithm']} |"
            )
    else:
        report_lines.append("| — | — | — | — | No override met all gates |")
    report_lines.extend([
        "",
        "## Evidence inventory",
        "",
        f"- Close/spread raw rows: `{combined}`",
        f"- Profile SHA-256: `{profile['profile_sha256']}`",
        f"- Generator result: `{generator_result}`",
        "- Every benchmark invocation performed its built-in MPFR value and "
        "ternary-direction checks before rows were retained.",
        "",
    ])
    (output / "CALIBRATION_REPORT.md").write_text(
        "\n".join(report_lines), encoding="utf-8"
    )
    result = {
        "campaign_id": campaign_id,
        "profile_header": str(profile_header),
        "profile_report": str(profile_report),
        "profile_sha256": profile["profile_sha256"],
        "accepted_rules": len(profile["rules"]),
        "output": str(output),
    }
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
