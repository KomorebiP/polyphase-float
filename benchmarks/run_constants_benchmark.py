#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

from __future__ import annotations

import argparse
import csv
import json
import math
import os
from pathlib import Path
import random
import statistics
import subprocess
import time
from typing import Iterable


def median(xs: list[float]) -> float:
    return float(statistics.median(xs))


def mad(xs: list[float]) -> float:
    m = median(xs)
    return median([abs(x - m) for x in xs])


def percentile(xs: list[float], p: float) -> float:
    ys = sorted(xs)
    if not ys:
        return float("nan")
    if len(ys) == 1:
        return ys[0]
    x = (len(ys) - 1) * p
    lo, hi = math.floor(x), math.ceil(x)
    if lo == hi:
        return ys[lo]
    return ys[lo] * (hi - x) + ys[hi] * (x - lo)


def bootstrap_median(xs: list[float], seed: int, draws: int) -> tuple[float, float]:
    rng = random.Random(seed)
    n = len(xs)
    vals = [median([xs[rng.randrange(n)] for _ in range(n)]) for _ in range(draws)]
    return percentile(vals, 0.025), percentile(vals, 0.975)


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
    with path.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        w.writerows(rows)


def invoke(binary: Path, method: str, bits: int, workers: int) -> float:
    p = subprocess.run(
        [str(binary), method, str(bits), str(workers), "1"],
        text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
        env={**os.environ, "LC_ALL": "C", "LANG": "C"},
    )
    if p.returncode != 0:
        raise RuntimeError(f"{method} failed ({p.returncode})\n{p.stdout}\n{p.stderr}")
    payload = json.loads(p.stdout.strip().splitlines()[-1])
    return float(payload["seconds_per_call"])


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--out", default="results/constants")
    ap.add_argument("--bits", default="100000,1000000,5000000")
    ap.add_argument("--workers", type=int, default=4)
    ap.add_argument("--repeats", type=int, default=9)
    ap.add_argument("--bootstrap-draws", type=int, default=20000)
    ap.add_argument("--seed", type=int, default=20260810)
    args = ap.parse_args()

    root = Path(args.root).resolve()
    out = (root / args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    binary = root / "bin/benchmark_constants"
    bits_list = [int(x) for x in args.bits.split(",") if x.strip()]
    workers = max(1, args.workers)
    rng = random.Random(args.seed)

    definitions = {
        "ppf_pi_cold_1t": ("ppf_pi_cold", 1),
        "ppf_pi_cold_parallel": ("ppf_pi_cold", workers),
        "mpfr_pi_cold_1t": ("mpfr_pi_cold", 1),
        "ppf_log2_cold_1t": ("ppf_log2_cold", 1),
        "ppf_log2_cold_parallel": ("ppf_log2_cold", workers),
        "mpfr_log2_cold_1t": ("mpfr_log2_cold", 1),
        "ppf_pi_cached": ("ppf_pi_cached", workers),
        "mpfr_pi_cached": ("mpfr_pi_cached", 1),
        "ppf_log2_cached": ("ppf_log2_cached", workers),
        "mpfr_log2_cached": ("mpfr_log2_cached", 1),
    }

    raw: list[dict] = []
    print("Independent certified-constant benchmark", flush=True)
    print(f"workers={workers}, repetitions={args.repeats}", flush=True)
    for bits in bits_list:
        # One untimed warm-up for every execution model.
        for _, (method, w) in definitions.items():
            invoke(binary, method, bits, w)
        for rep in range(1, args.repeats + 1):
            names = list(definitions)
            rng.shuffle(names)
            for order, name in enumerate(names, 1):
                method, w = definitions[name]
                seconds = invoke(binary, method, bits, w)
                raw.append({
                    "bits": bits,
                    "repeat": rep,
                    "order": order,
                    "method": name,
                    "workers": w,
                    "seconds": seconds,
                })
                print(f"  bits={bits:,} round={rep} {name}: {seconds:.9f}s", flush=True)

    write_csv(out / "raw.csv", raw)
    summary: list[dict] = []
    for bits in bits_list:
        for name in definitions:
            xs = [r["seconds"] for r in raw if r["bits"] == bits and r["method"] == name]
            summary.append({
                "bits": bits,
                "method": name,
                "workers": definitions[name][1],
                "runs": len(xs),
                "median_seconds": median(xs),
                "mad_seconds": mad(xs),
                "min_seconds": min(xs),
                "max_seconds": max(xs),
            })
    write_csv(out / "summary.csv", summary)

    index = {(r["bits"], r["repeat"], r["method"]): r["seconds"] for r in raw}
    comparisons = [
        ("pi_equal_core", "ppf_pi_cold_1t", "mpfr_pi_cold_1t"),
        ("pi_single_call_parallel", "ppf_pi_cold_parallel", "mpfr_pi_cold_1t"),
        ("pi_parallel_scaling", "ppf_pi_cold_parallel", "ppf_pi_cold_1t"),
        ("log2_equal_core", "ppf_log2_cold_1t", "mpfr_log2_cold_1t"),
        ("log2_single_call_parallel", "ppf_log2_cold_parallel", "mpfr_log2_cold_1t"),
        ("log2_parallel_scaling", "ppf_log2_cold_parallel", "ppf_log2_cold_1t"),
        ("pi_cached", "ppf_pi_cached", "mpfr_pi_cached"),
        ("log2_cached", "ppf_log2_cached", "mpfr_log2_cached"),
    ]
    paired: list[dict] = []
    for bits in bits_list:
        for label, favored, other in comparisons:
            ratios: list[float] = []
            wins = losses = ties = 0
            for rep in range(1, args.repeats + 1):
                a = index[(bits, rep, favored)]
                b = index[(bits, rep, other)]
                ratios.append(b / a)
                if a < b:
                    wins += 1
                elif a > b:
                    losses += 1
                else:
                    ties += 1
            lo, hi = bootstrap_median(
                ratios, args.seed ^ bits ^ sum(ord(c) for c in label), args.bootstrap_draws
            )
            paired.append({
                "bits": bits,
                "comparison": label,
                "pairs": len(ratios),
                "paired_median_speedup": median(ratios),
                "bootstrap_95_low": lo,
                "bootstrap_95_high": hi,
                "wins": wins,
                "losses": losses,
                "ties": ties,
                "sign_test_two_sided_p": sign_p(wins, losses),
            })
    write_csv(out / "paired_comparisons.csv", paired)

    smap = {(r["bits"], r["method"]): r for r in summary}
    lines = [
        "# Independent certified constants", "",
        "Every Polyphase result is produced as a `ppf_t` from a certified dyadic interval.",
        "MPFR is linked only by the benchmark executable as the reference implementation.", "",
        "## Median timings", "",
        "| Bits | Method | Workers | Median (s) | MAD (s) |",
        "|---:|---|---:|---:|---:|",
    ]
    for bits in bits_list:
        for name in definitions:
            x = smap[(bits, name)]
            lines.append(
                f"| {bits:,} | {name} | {x['workers']} | "
                f"{x['median_seconds']:.9f} | {x['mad_seconds']:.9f} |"
            )
    lines += ["", "## Paired comparisons", "",
              "| Bits | Comparison | Paired median | Bootstrap 95% | W-L-T | Sign p |",
              "|---:|---|---:|---:|---:|---:|"]
    for x in paired:
        lines.append(
            f"| {x['bits']:,} | {x['comparison']} | "
            f"{x['paired_median_speedup']:.3f}x | "
            f"[{x['bootstrap_95_low']:.3f}, {x['bootstrap_95_high']:.3f}] | "
            f"{x['wins']}-{x['losses']}-{x['ties']} | {x['sign_test_two_sided_p']:.6g} |"
        )
    lines += ["", "A speedup above 1 favors Polyphase in the named comparison.",
              "The `single_call_parallel` rows compare internally threaded Polyphase latency with one ordinary MPFR call; they are not equal-resource batch comparisons.",
              "Cached retrieval is reported separately from cold computation."]
    (out / "REPORT.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    (out / "environment.json").write_text(json.dumps({
        "timestamp_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "bits": bits_list,
        "workers": workers,
        "repetitions": args.repeats,
        "seed": args.seed,
    }, indent=2), encoding="utf-8")
    print("\n" + (out / "REPORT.md").read_text(), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
