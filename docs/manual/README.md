# Polyphase Float User Guide and Reference

This is the reader's manual for the C library. It develops checked numerical
programs, explains ownership and rounding, and provides a complete core API
reference. Read the [PDF edition](Polyphase_Float_Manual.pdf), or download the
[HTML edition](Polyphase_Float_Manual.html) and open the saved file in a browser. The chapter files remain available for ordinary text
reading and contributions.

The library remains **beta and work in progress**. For build commands,
validation and release records, use the [documentation index](../README.md).

## Start here

1. [Finding your way](chapters/01_orientation.md)
2. [Build and install](chapters/02_build_install.md)
3. [Objects and contexts](chapters/03_objects_contexts.md)
4. [Rounding and errors](chapters/04_rounding_errors.md)
5. [First programs](chapters/05_first_programs.md)

## Numerical workflows

6. [Arithmetic and range](chapters/06_arithmetic_ranges.md)
7. [Constants and intervals](chapters/07_constants_intervals.md)
8. [Elementary functions](chapters/08_elementary_functions.md)
9. [Special functions](chapters/09_special_functions.md)
10. [Numerical recipes](chapters/10_numerical_recipes.md)

## Application engineering

11. [Text and binary I/O](chapters/11_text_binary_io.md)
12. [Memory and overlap](chapters/12_memory_aliasing.md)
13. [Threads and runtime state](chapters/13_threads_runtime.md)
14. [Integration and performance](chapters/14_integration_performance.md)
15. [Troubleshooting](chapters/15_troubleshooting.md)

## Reference material

- [Numerical limits](numerical_limits.md)
- [Every core function](API_REFERENCE.md)
- [Types and named constants](TYPES_REFERENCE.md)
- [Glossary](glossary.md)
- [Checked example outputs](EXAMPLE_RESULTS.md)
- [Complete example programs](../../examples/manual/)

## Rebuild the book

The library itself does not require a document toolchain. Rebuilding the book
requires Python 3, Pandoc, XeLaTeX, the `fvextra`, `needspace`, and `fancyhdr`
LaTeX packages, and the Bitstream Charter, Nimbus Sans, and DejaVu Sans Mono
fonts. To regenerate the mathematical illustrations, also install NumPy and
Matplotlib.

From the source root:

```sh
python3 tools/build_manual.py --output ../polyphase-manual
```

The command creates PDF, self-contained HTML with native MathML, and assembled
Markdown editions in the chosen directory. It reads the governed API records
and never modifies the production library. Add `--skip-pdf` when only the HTML
and Markdown editions are needed. The source chapter files and complete
programs remain the editable material.

The API page on GitHub uses ordinary Markdown headings so that its
alphabetical index works there. To regenerate that page without building
the book or installing Pandoc:

```sh
python3 tools/build_manual.py --github-reference docs/manual/API_REFERENCE.md
```

The book builder retains its explicit PDF and HTML anchors.

[Documentation](../README.md) · [Project overview](../../README.md)
