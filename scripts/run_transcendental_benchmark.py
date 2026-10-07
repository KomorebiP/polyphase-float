#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import random
import statistics
import subprocess
import time

OPS = ["log", "log1p", "exp", "expm1", "pow", "pow_ui"]


def median(values: list[float]) -> float:
    return float(statistics.median(values))


def mad(values: list[float]) -> float:
    m = median(values)
    return median([abs(x - m) for x in values])


def percentile(values: list[float], q: float) -> float:
    x = sorted(values)
    if not x:
        return float("nan")
    if len(x) == 1:
        return x[0]
    p = q * (len(x) - 1)
    lo, hi = int(math.floor(p)), int(math.ceil(p))
    if lo == hi:
        return x[lo]
    return x[lo] * (hi - p) + x[hi] * (p - lo)


def bootstrap_ci(values: list[float], draws: int, seed: int) -> tuple[float, float]:
    rng = random.Random(seed)
    n = len(values)
    samples = [median([values[rng.randrange(n)] for _ in range(n)]) for _ in range(draws)]
    return percentile(samples, 0.025), percentile(samples, 0.975)


def sign_p(wins: int, losses: int) -> float:
    n = wins + losses
    if n == 0:
        return 1.0
    k = min(wins, losses)
    tail = sum(math.comb(n, i) for i in range(k + 1)) / (2 ** n)
    return min(1.0, 2.0 * tail)


def write_csv(path: Path, rows: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if not rows:
        path.write_text("", encoding="utf-8")
        return
    fields: list[str] = []
    for row in rows:
        for key in row:
            if key not in fields:
                fields.append(key)
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def invoke(binary: Path, method: str, bits: int, loops: int, env: dict[str, str]) -> dict:
    result = subprocess.run(
        [str(binary), method, str(bits), str(loops)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
        env=env,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"{method} failed with {result.returncode}\n{result.stdout}\n{result.stderr}"
        )
    for line in reversed(result.stdout.splitlines()):
        try:
            payload = json.loads(line)
        except json.JSONDecodeError:
            continue
        if isinstance(payload, dict):
            return payload
    raise RuntimeError(f"No JSON output from {method}\n{result.stdout}\n{result.stderr}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--out", default="results/transcendentals")
    parser.add_argument("--bits", default="100,1000,5000")
    parser.add_argument("--repetitions", type=int, default=9)
    parser.add_argument("--loops", type=int, default=1)
    parser.add_argument("--bootstrap-draws", type=int, default=20000)
    parser.add_argument("--seed", type=int, default=20260811)
    parser.add_argument("--fresh", action="store_true")
    args = parser.parse_args()

    root = Path(args.root).resolve()
    out = (root / args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    raw_path = out / "raw.csv"
    if args.fresh and raw_path.exists():
        raw_path.unlink()

    binary = root / "bin" / "benchmark_transcendental_method"
    bits_values = [int(x) for x in args.bits.split(",") if x.strip()]
    repetitions = max(3, args.repetitions)
    loops = max(1, args.loops)
    methods = [f"{family}_{op}" for op in OPS for family in ("ppf", "mpfr")]
    rng = random.Random(args.seed)
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = str(root / "lib") + (
        ":" + env["LD_LIBRARY_PATH"] if env.get("LD_LIBRARY_PATH") else ""
    )
    env["OMP_NUM_THREADS"] = "1"

    raw: list[dict] = []
    if raw_path.exists():
        with raw_path.open(newline="", encoding="utf-8") as stream:
            for row in csv.DictReader(stream):
                try:
                    parsed = {
                        "bits": int(row["bits"]),
                        "repeat": int(row["repeat"]),
                        "order": int(row["order"]),
                        "method": row["method"],
                        "seconds_per_call": float(row["seconds_per_call"]),
                        "loops": int(row["loops"]),
                        "ternary": int(row["ternary"]),
                    }
                except (KeyError, ValueError):
                    continue
                if parsed["bits"] in bits_values and parsed["method"] in methods:
                    raw.append(parsed)
    completed = {(r["bits"], r["repeat"], r["method"]) for r in raw}

    print("Correctness-gated independent transcendental benchmark", flush=True)
    print(f"bits={bits_values}, repetitions={repetitions}, loops={loops}", flush=True)

    for bits in bits_values:
        warm = methods[:]
        rng.shuffle(warm)
        for method in warm:
            invoke(binary, method, bits, 1, env)

    for bits in bits_values:
        print(f"\n{bits:,} bits", flush=True)
        for repeat in range(1, repetitions + 1):
            order = methods[:]
            rng.shuffle(order)
            for position, method in enumerate(order, 1):
                if (bits, repeat, method) in completed:
                    continue
                payload = invoke(binary, method, bits, loops, env)
                row = {
                    "bits": bits,
                    "repeat": repeat,
                    "order": position,
                    "method": method,
                    "seconds_per_call": float(payload["seconds_per_call"]),
                    "loops": int(payload["loops"]),
                    "ternary": int(payload["ternary"]),
                }
                raw.append(row)
                completed.add((bits, repeat, method))
                raw.sort(key=lambda r: (r["bits"], r["repeat"], r["order"]))
                write_csv(raw_path, raw)
                print(
                    f"  round={repeat} {method}: {row['seconds_per_call']:.9f}s",
                    flush=True,
                )
    write_csv(raw_path, raw)

    summary: list[dict] = []
    for bits in bits_values:
        for method in methods:
            values = [
                r["seconds_per_call"]
                for r in raw
                if r["bits"] == bits and r["method"] == method
            ]
            summary.append(
                {
                    "bits": bits,
                    "method": method,
                    "runs": len(values),
                    "median_seconds": median(values),
                    "mad_seconds": mad(values),
                    "min_seconds": min(values),
                    "max_seconds": max(values),
                }
            )
    write_csv(out / "summary.csv", summary)

    lookup = {
        (r["bits"], r["repeat"], r["method"]): r["seconds_per_call"]
        for r in raw
    }
    paired: list[dict] = []
    for bits in bits_values:
        for op in OPS:
            ratios: list[float] = []
            wins = losses = ties = 0
            for repeat in range(1, repetitions + 1):
                ppf_seconds = lookup[(bits, repeat, f"ppf_{op}")]
                mpfr_seconds = lookup[(bits, repeat, f"mpfr_{op}")]
                ratios.append(mpfr_seconds / ppf_seconds)
                if ppf_seconds < mpfr_seconds:
                    wins += 1
                elif ppf_seconds > mpfr_seconds:
                    losses += 1
                else:
                    ties += 1
            digest = int.from_bytes(hashlib.sha256(op.encode()).digest()[:8], "big")
            low, high = bootstrap_ci(
                ratios, args.bootstrap_draws, args.seed ^ bits ^ digest
            )
            paired.append(
                {
                    "bits": bits,
                    "operation": op,
                    "pairs": len(ratios),
                    "paired_median_speedup": median(ratios),
                    "bootstrap_95_low": low,
                    "bootstrap_95_high": high,
                    "wins": wins,
                    "losses": losses,
                    "ties": ties,
                    "sign_test_two_sided_p": sign_p(wins, losses),
                }
            )
    write_csv(out / "paired_comparisons.csv", paired)

    smap = {(r["bits"], r["method"]): r for r in summary}
    pmap = {(r["bits"], r["operation"]): r for r in paired}
    lines = [
        "# Independent log, exponential, and power benchmark",
        "",
        "Every timed implementation is checked against MPFR before its row is retained.",
        "Both libraries use one worker, the same destination precision, and nearest-even rounding.",
        "A speedup above 1 favors Polyphase.",
        "",
        "| Bits | Operation | Polyphase median | MPFR median | Paired speedup | Bootstrap 95% | W-L-T |",
        "|---:|---|---:|---:|---:|---:|---:|",
    ]
    for bits in bits_values:
        for op in OPS:
            ppf_row = smap[(bits, f"ppf_{op}")]
            mpfr_row = smap[(bits, f"mpfr_{op}")]
            comparison = pmap[(bits, op)]
            lines.append(
                f"| {bits:,} | {op} | {ppf_row['median_seconds']:.9f} s | "
                f"{mpfr_row['median_seconds']:.9f} s | "
                f"{comparison['paired_median_speedup']:.3f}x | "
                f"[{comparison['bootstrap_95_low']:.3f}, "
                f"{comparison['bootstrap_95_high']:.3f}] | "
                f"{comparison['wins']}-{comparison['losses']}-{comparison['ties']} |"
            )
    lines += [
        "",
        "## Interpretation",
        "",
        "This is the first certified implementation of the complete function family, not a performance release.",
        "The interval algorithms are intentionally conservative and currently slower than MPFR, especially for exponential and general power calls.",
        "Input construction, process startup, and the untimed correctness gate are excluded from the timed region.",
    ]
    (out / "REPORT.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    (out / "environment.json").write_text(
        json.dumps(
            {
                "timestamp_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                "bits": bits_values,
                "repetitions": repetitions,
                "loops": loops,
                "seed": args.seed,
                "contract": "one worker; same destination precision; nearest-even; correctness-gated against MPFR",
                "uname": subprocess.run(
                    ["uname", "-a"], text=True, stdout=subprocess.PIPE
                ).stdout.strip(),
                "compiler": subprocess.run(
                    ["cc", "--version"], text=True, stdout=subprocess.PIPE
                ).stdout.splitlines()[0],
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    print("\n" + (out / "REPORT.md").read_text(encoding="utf-8"), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
