#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Generate exact, explanatory vector figures for the reader's manual."""
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

OUTPUT = Path(__file__).resolve().parents[1] / "docs/manual/figures"
OUTPUT.mkdir(parents=True, exist_ok=True)
TEAL, GOLD, GRAY = "#234b5a", "#b4792c", "#69777c"
plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 10, "axes.spines.top": False,
    "axes.spines.right": False, "axes.labelcolor": TEAL, "text.color": TEAL,
    "axes.titleweight": "bold", "savefig.facecolor": "white", "svg.fonttype": "none"})


def save(fig, name):
    fig.savefig(OUTPUT / (name + ".svg"), bbox_inches="tight")
    fig.savefig(OUTPUT / (name + ".pdf"), bbox_inches="tight")
    plt.close(fig)


fig, ax = plt.subplots(figsize=(7.2, 3.0), layout="constrained")
points = [1, 1.25, 1.5, 1.75, 2]
ax.axhline(0, color=GRAY, lw=1)
ax.scatter(points, np.zeros(5), s=65, color=TEAL, zorder=3)
ax.scatter([1.375], [0], marker="D", s=90, color=GOLD, zorder=4)
ax.annotate("exact input 11/8 = 1.375", (1.375, 0), (1.375, .65), ha="center", arrowprops={"arrowstyle": "-", "color": GOLD})
ax.annotate("toward zero / down\n1.25", (1.25, 0), (1.12, -.7), ha="center", arrowprops={"arrowstyle": "->", "color": TEAL})
ax.annotate("nearest, ties to even / up / away\n1.5", (1.5, 0), (1.68, -.7), ha="center", arrowprops={"arrowstyle": "->", "color": TEAL})
ax.set(ylim=(-1.05, 1), xlim=(.94, 2.06), yticks=[], xticks=points, title="Three-bit binary rounding near one")
ax.spines[["left", "bottom"]].set_visible(False)
save(fig, "rounding_neighbors")

fig, ax = plt.subplots(figsize=(7.2, 3.6), layout="constrained")
bits = np.arange(2, 257)
ax.plot(bits, bits * np.log10(2), lw=2, color=TEAL)
for p in [24, 53, 113, 256]:
    y = p * np.log10(2)
    ax.scatter([p], [y], color=GOLD, s=35, zorder=4)
    ax.annotate(f"{p} bits\nabout {y:.1f} digits", (p, y), xytext=(-8, 13 if p < 200 else -36), textcoords="offset points", ha="right" if p > 200 else "left", fontsize=9, bbox={"facecolor": "white", "edgecolor": "none", "alpha": .9, "pad": 1.8})
ax.set(xlim=(0, 270), ylim=(0, 85), xlabel="Binary precision (bits)", ylabel="Decimal information capacity", title="Bits and approximate decimal significant digits")
ax.grid(alpha=.15)
save(fig, "precision_digits")

fig, axes = plt.subplots(2, 1, figsize=(7.2, 4.7), sharex=True, layout="constrained")
for ax, lo, hi, title in [(axes[0], 1.39, 1.40, "A narrower interval certifies one result and direction"), (axes[1], 1.37, 1.39, "An interval crossing the rounding boundary needs refinement")]:
    ax.axvspan(1.24, 1.375, color="#edf1f3")
    ax.axvspan(1.375, 1.51, color="#dfeaec")
    ax.axvline(1.375, color=GOLD, linestyle="--", lw=1.5)
    ax.plot([lo, hi], [0, 0], color=TEAL, lw=7, solid_capstyle="butt")
    ax.scatter([lo, hi], [0, 0], color=TEAL, s=35)
    ax.set(ylim=(-.6, .6), yticks=[], title=title)
    ax.spines[["left", "bottom"]].set_visible(False)
axes[0].text(1.255, .36, "rounds to 1.25", fontsize=9)
axes[0].text(1.43, .36, "rounds to 1.5", fontsize=9)
axes[1].set(xlim=(1.24, 1.51), xticks=[1.25, 1.375, 1.5], xlabel="Exact value enclosed by the certificate")
save(fig, "interval_certification")
print("Generated three explanatory figures as SVG and PDF.")
