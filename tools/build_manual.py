#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Build the reader's manual from prose and the governed public API ledger."""
from __future__ import annotations
import argparse
import collections
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import textwrap

ROOT = Path(__file__).resolve().parents[1]
MANUAL = ROOT / "docs/manual"

RETURNS = {
    "ternary": "A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.",
    "void": "No return value.",
    "status": "A status code. Check the operation's success convention; this is not a numerical ternary.",
    "predicate": "A predicate result: zero means false and a nonzero value means true.",
    "comparison": "An ordering result. NaNs and unordered comparisons require the operation's documented handling.",
    "value": "The value described above, with the declared C return type.",
    "borrowed_pointer": "A borrowed pointer. Do not free the returned storage.",
    "pointer": "A pointer with the ownership conditions below.",
    "owned_pointer": "A pointer with the ownership conditions below.",
    "formatted_count": "A formatting count; a negative result reports failure.",
    "count": "The operation-specific count described above, rather than a numerical ternary.",
    "sign": "A sign result for the operation described above.",
    "interval_status": "`PPF_INTERVAL_DECIDED`, `PPF_INTERVAL_UNDECIDED`, or `PPF_INTERVAL_ERROR`. The numerical ternary is a separate output.",
    "packed_ternary": "A packed two-output rounding code. Test for `PPF_TERNARY_ERROR` first. Decode each component as documented; the sign of the packed integer is not a numerical direction.",
}
ATOMICITY = {
    "verified_transactional": "An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.",
    "reviewed_allocation_free": "The valid initialized-storage path needs no project allocation. This says nothing about GMP-internal or application allocations.",
    "verified_initialization_lifecycle": "Initial allocation failure leaves a safe-zero object. A later setter failure leaves an initialized, clearable object. Clear every initialized outcome; there is no old uninitialized numerical value to preserve.",
    "initialization_safe_zero": "On a valid initialization request, project-allocation failure leaves a safe zero-state object. Invalid arguments can be rejected before zeroing; follow the initializer's preconditions and cleanup lifecycle.",
    "verified_prefix_cleanup": "A failed variadic initializer clears the visited prefix. Unvisited objects are untouched.",
}
THREADS = {
    "application_lock_required": "Application locking is required for shared mutable use.",
    "immutable_plan": "Share the plan only under its documented immutable-plan and lifetime rules.",
    "global_synchronized": "The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.",
    "thread_local": "This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.",
}


def signature(declaration: str) -> str:
    declaration = " ".join(declaration.split())
    if len(declaration) <= 76:
        return declaration
    head, tail = declaration.split("(", 1)
    parameters = tail.rsplit(")", 1)[0]
    parts, current, depth = [], "", 0
    for char in parameters:
        if char == "," and depth == 0:
            parts.append(current.strip()); current = ""; continue
        current += char
        if char == "(": depth += 1
        if char == ")": depth -= 1
    if current.strip(): parts.append(current.strip())
    return head + "(\n" + ",\n".join("    " + x for x in parts) + "\n);"


def api_reference() -> str:
    ledger = json.loads((ROOT / "semantics/contract-v1.json").read_text())
    functions = ledger["functions"]
    assert len(functions) == len({x["name"] for x in functions}) == 437
    grouped = collections.OrderedDict()
    for function in functions:
        grouped.setdefault(function["section"], []).append(function)
    out = ["# Core API reference\n",
        "This reference covers every core function declared in `<polyphase_float.h>`. "
        "The five optional MPFR adapter functions are described separately in the integration chapter. "
        "Entries are arranged by task, then by name. Use PDF search or the alphabetical index to find a symbol.\n",
        "All ordinary arithmetic objects must be initialized and valid. Unless an operation explicitly "
        "changes precision, the destination's precision governs its result. Mutating an object or an explicit "
        "context requires exclusive access. No function transfers ownership unless its entry says so.\n",
        "A recorded allocation contract describes recoverable project allocation, not every allocation made "
        "by GMP, libc, or the operating system. Numerical range rejection and certification exhaustion have "
        "their own output rules. In particular, an error sentinel alone is not a promise of destination preservation.\n",
        "The numerical-limits appendix applies throughout this reference. Absence of a function-specific "
        "limit in an entry is not a claim of unlimited work, a termination proof, or universal correct rounding. "
        "The full source and individual numerical arguments remain the authority for implementation details.\n"]
    for section, entries in grouped.items():
        out.append("## " + section + "\n")
        for f in sorted(entries, key=lambda x: x["name"]):
            name = f["name"]
            purpose = f["purpose"]
            if name == "ppf_get_str":
                purpose = "Format a significand into a supplied or library-allocated buffer and return its radix exponent separately."
            out.extend(["### " + name + " {#" + name + "}\n", purpose + "\n", "```c\n" + signature(f["declaration"]) + "\n```\n"])
            if f["domain"]["classification"] == "function_specific":
                out.append("**Input contract.** " + f["domain"]["summary"].rstrip(".") + ".\n")
            result = f["return_contract"].get("note") or RETURNS[f["return_contract"]["kind"]]
            result = result[:1].upper() + result[1:].rstrip(".") + "."
            result = re.sub(r"(?<!`)\b(PPF_[A-Z0-9_]+)\b(?!`)", r"`\1`", result)
            sentinel = f["return_contract"].get("error_sentinel")
            if sentinel and f["return_contract"]["kind"] not in {"ternary", "interval_status", "formatted_count"}:
                result += " Failure indicator: `" + sentinel + "`."
            out.append("**Result.** " + result + "\n")
            conditions = []
            context = f["context"]
            if name in {"ppf_context_init", "ppf_context_init_custom"}:
                conditions.append("The context destination must be non-null writable unused storage. This call initializes it; do not overwrite a live context.")
            elif context["policy"] == "explicit_required":
                conditions.append("The supplied context must be non-null and initialized.")
            elif context["parameter"] and context["null_uses_thread_default"]:
                conditions.append("A null `ctx` selects the calling thread's default context.")
            if f["rounding"].get("parameter") or f["rounding"]["kind"] in {"fixed_by_operation", "explicit"}:
                conditions.append(f["rounding"]["note"].rstrip(".") + ".")
            if conditions:
                out.append("**Context and rounding.** " + " ".join(conditions) + "\n")
            if f["ownership"]["classification"] != "no_transfer":
                out.append("**Ownership.** " + f["ownership"]["note"].rstrip(".") + ".\n")
            alias = f["aliasing"]
            if alias["classification"] != "not_applicable":
                lines = list(alias.get("allowed_patterns", []))
                lines.extend("Forbidden: " + x for x in alias.get("forbidden_patterns", []))
                out.append("**Overlap.** " + "; ".join(lines).rstrip(".") + ".\n")
            atomicity = f["failure_atomicity"].get("note") or ATOMICITY.get(f["failure_atomicity"]["classification"])
            if f["failure_atomicity"]["classification"] == "verified_transactional" and not f["arguments"]["destinations"] and not f["arguments"]["inout_numbers"] and not f["failure_atomicity"].get("note"):
                atomicity = "The operation has a project-allocation failure contract. Follow its specific output and ownership rules; numerical-destination rollback does not describe arbitrary string, stream, arena, cache, or plan outputs. File I/O side effects are not rolled back."
            if atomicity:
                out.append("**Allocation and failure.** " + atomicity[:1].upper() + atomicity[1:].rstrip(".") + ".\n")
            limit = f["practical_limits"]
            if limit["classification"] == "implementation_bound":
                summary = re.sub(r"(?<!`)\b(PPF_[A-Z0-9_]+)\b(?!`)", r"`\1`", limit["summary"])
                out.append("**Resource policy.** " + summary.rstrip(".") + ". Consult the numerical-limits appendix for the complete cap inventory.\n")
            thread = THREADS.get(f["thread_safety"]["classification"])
            if thread:
                out.append("**Concurrency.** " + thread + "\n")
            if f["availability"]["classification"] != "always":
                out.append("**Availability.** This declaration depends on `" + str(f["availability"]["condition"]) + "`; test the installed header's capability macro.\n")
    out.extend(["# Alphabetical function index\n", "Every entry below links to its core API reference.\n"])
    for letter in sorted({f["name"][4:5] for f in functions}):
        out.append("## " + letter.upper() + "\n")
        names = [f["name"] for f in sorted(functions, key=lambda x: x["name"]) if f["name"][4:5] == letter]
        midpoint = (len(names) + 1) // 2
        left, right = names[:midpoint], names[midpoint:]
        link = lambda name: "[`" + name + "`](#" + name + ")" if name else ""
        out.extend(["| Function | Function |", "|---|---|"])
        for index, name in enumerate(left):
            out.append("| " + link(name) + " | " + link(right[index] if index < len(right) else "") + " |")
        out.append("")
    return "\n".join(out)


def type_reference() -> str:
    contract = json.loads((ROOT / "abi/contract-v1.json").read_text())
    out = ["# Types and named constants\n",
        "Include the installed public header to obtain these declarations. The listings "
        "below are reference material. The installed header is the authoritative declaration source. These definitions "
        "summarize the governed public types. Public layout compatibility does not make "
        "direct field mutation an ordinary application interface. Use lifecycle and setter "
        "functions unless you are implementing the documented direct-storage protocol.\n",
        "## Scalar types\n", "```c"]
    for name, definition in sorted(contract["typedefs"].items()):
        guard = "PPF_HAVE_FLOAT128" if name == "ppf_float128_t" else "PPF_HAVE_DECIMAL_FLOAT" if name in {"ppf_decimal64_t", "ppf_decimal128_t"} else None
        if guard:
            out.extend(["#if " + guard, "__extension__ typedef " + definition + " " + name + ";", "#endif"])
        else:
            out.append("typedef " + definition + " " + name + ";")
    out.extend(["```\n", "The optional decimal and 128-bit binary types exist only when their installed-header "
        "capability conditions are satisfied. They do not make those C extensions mandatory "
        "for the independent core.\n", "## Enumerations\n"])
    for name, members in contract["enums"].items():
        label = name.removeprefix("anonymous:")
        start = "enum {" if name.startswith("anonymous:") else "typedef enum {"
        end = "};" if name.startswith("anonymous:") else "} " + name + ";"
        out.extend(["### " + label + "\n", "```c\n" + start + "\n" + ",\n".join("    " + x for x in members) + "\n" + end + "\n```\n"])
    out.extend(["## Precision, range, and ABI constants\n",
        "Version and digest strings identify a build or generated contract. They are not "
        "mathematical parameters and do not attest to the bytes of a loaded binary.\n", "```c"])
    for name, definition in sorted(contract["constants"].items()):
        if "SHA256" not in name and not name.startswith("PPF_VERSION"):
            out.append("#define " + name + " " + str(definition))
    out.extend(["```\n", "## Public layout records\n",
        "The allocator function-pointer types and advanced custom-storage callbacks are "
        "declared in `<polyphase_float.h>`. Keep callback signatures exact. The records below "
        "describe ABI shape, not a license to replace initialized objects with raw structure copies.\n"])
    for name, fields in contract["structs"].items():
        out.extend(["### " + name + "\n", "```c\ntypedef struct {\n" + "\n".join("    " + x for x in fields) + "\n} " + name + ";\n```\n"])
    return "\n".join(out)


def complete_examples() -> str:
    directory = ROOT / "examples/manual"
    manifest = json.loads((directory / "manifest.json").read_text())
    out = ["# Complete example programs\n", "These are the complete source files supplied with the manual. "
        "Build them through the manual example targets or the checked-example runner. "
        "They include their own initialization, status checks, output, and cleanup. "
        "Each program's expected output is recorded in the example manifest and the example-results guide.\n"]
    for path in sorted(directory.glob("*.c")):
        out.extend(["## " + path.name + " {#example-" + path.stem + "}\n", "```c\n" + path.read_text().rstrip() + "\n```\n"])
    return "\n".join(out)


def assemble() -> str:
    chapters = sorted((MANUAL / "chapters").glob("[0-9][0-9]_*.md"))
    if len(chapters) != 15:
        raise RuntimeError("Expected all 15 authored chapters, found " + str(len(chapters)))
    front = (MANUAL / "front_matter.md").read_text()
    parts = [front]
    for path in chapters:
        text = path.read_text()
        text = re.sub(r"^# ([^\n]+)$", lambda m: "# " + m[1] + " {#chapter-" + path.name[:2] + "}", text, count=1, flags=re.M)
        def local_link(match):
            label, target = match[1], match[2]
            if target.startswith(("#", "http:", "https:", "mailto:")):
                return match[0]
            filepart, _, fragment = target.partition("#")
            resolved = (path.parent / filepart).resolve()
            if resolved.parent == ROOT / "examples/manual" and resolved.suffix == ".c":
                return "[" + label + "](#example-" + resolved.stem + ")"
            if resolved.parent == MANUAL / "chapters" and re.match(r"[0-9][0-9]_", resolved.name):
                return "[" + label + "](#" + (fragment or "chapter-" + resolved.name[:2]) + ")"
            if resolved.name == "API_REFERENCE.md" and fragment:
                return "[" + label + "](#" + fragment + ")"
            relative = resolved.relative_to(ROOT).as_posix()
            reference = relative + ("#" + fragment if fragment else "")
            return label if label.strip("`") == reference else label + " (`" + reference + "`)"
        text = re.sub(r"\[([^\]\n]+)\]\(([^\s)]+)\)", local_link, text)
        illustration = {
            "03_objects_contexts.md": ("precision_digits", "Binary precision gives approximately p log10(2) decimal digits of information capacity. This is not an accuracy guarantee for a computed expression."),
            "06_arithmetic_ranges.md": ("rounding_neighbors", "At three-bit precision, the positive midpoint 11/8 rounds to 1.5 under nearest, ties to even. Directed modes select the appropriate neighbor."),
            "07_constants_intervals.md": ("interval_certification", "A useful interval must certify the rounded value, ternary direction, and range state. Crossing a rounding boundary leaves the result undecided."),
        }.get(path.name)
        if illustration:
            name, caption = illustration
            heading, remainder = text.split("\n", 1)
            text = heading + "\n\n![" + caption + "](docs/manual/figures/" + name + ".svg)\n\n" + remainder
        parts.append(text)
    parts.extend([(MANUAL / "numerical_limits.md").read_text(), api_reference(), type_reference(), complete_examples(), (MANUAL / "glossary.md").read_text()])
    return "\n\n".join(parts) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--skip-pdf", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ)
    environment.setdefault("SOURCE_DATE_EPOCH", "1792627200")
    environment.setdefault("FORCE_SOURCE_DATE", "1")
    text = assemble()
    source = output / "Polyphase_Float_Manual.md"
    source.write_text(text)
    common = ["pandoc", str(source), "--from=markdown+tex_math_single_backslash", "--standalone", "--toc", "--toc-depth=1", "--number-sections", "--resource-path=" + str(ROOT) + ":" + str(MANUAL), "--metadata=title:Polyphase Float", "--metadata=subtitle:User Guide and Reference", "--metadata=author:Komorebi P.", "--metadata=date:"]
    subprocess.run([*common, "--embed-resources", "--mathml", "--css=" + str(MANUAL / "manual.css"), "--output=" + str(output / "Polyphase_Float_Manual.html")], env=environment, check=True)
    if not args.skip_pdf:
        pdf_source = output / "manual_print.md"
        pdf_source.write_text(re.sub(r"(docs/manual/figures/[A-Za-z_]+)\.svg", r"\1.pdf", text))
        pdf_common = [pdf_source.as_posix() if x == str(source) else x for x in common]
        subprocess.run([*pdf_common, "--pdf-engine=xelatex", "--lua-filter=" + str(MANUAL / "manual_layout.lua"), "--top-level-division=chapter", "--variable=documentclass:book", "--variable=classoption:oneside,openany", "--variable=fontsize:11pt", "--variable=geometry:paperwidth=7.5in,paperheight=10in,top=0.85in,bottom=0.85in,left=0.85in,right=0.8in", "--variable=mainfont:Bitstream Charter", "--variable=sansfont:Nimbus Sans", "--variable=monofont:DejaVu Sans Mono", "--variable=monofontoptions:Scale=0.78", "--variable=colorlinks:true", "--variable=linkcolor:ManualTeal", "--variable=urlcolor:ManualTeal", "--highlight-style=tango", "--include-in-header=" + str(MANUAL / "manual_header.tex"), "--output=" + str(output / "Polyphase_Float_Manual.pdf")], env=environment, check=True)
        pdf_source.unlink()
    record = {"source_sha256": hashlib.sha256(text.encode()).hexdigest(), "core_functions": 437, "authored_chapters": 15, "files": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in output.iterdir() if p.is_file() and p.name.startswith("Polyphase_Float_Manual.")}}
    (output / "MANUAL_BUILD.json").write_text(json.dumps(record, indent=2, sort_keys=True) + "\n")
    print(json.dumps(record, indent=2))


if __name__ == "__main__":
    main()
