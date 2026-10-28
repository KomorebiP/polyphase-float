# Polyphase Float documentation

Polyphase Float is **beta and work in progress**. Start with the manual for
an introduction, or choose a guide for the task you want to complete.

## Read and use the library

| Task | Start here |
|---|---|
| Learn the library | [Manual and chapter list](manual/README.md) |
| Read the book as a PDF | [User Guide and Reference](manual/Polyphase_Float_Manual.pdf) |
| Read offline | [HTML edition](manual/Polyphase_Float_Manual.html), then use GitHub's download button and open the saved file in a browser |
| Build and install | [Building](BUILDING.md) |
| Find a function | [Core API reference](manual/API_REFERENCE.md#alphabetical-function-index) |
| Run complete examples | [Example programs](../examples/manual/), [recorded outputs](manual/EXAMPLE_RESULTS.md) |
| Check input limits and failure behavior | [Release scope](COMPLETION_SCOPE_0.20.0.md), [numerical limits](manual/numerical_limits.md) |
| Understand memory and threads | [Aliasing](ALIASING_CONTRACT_0.20.0.md), [thread safety](THREAD_SAFETY.md) |
| Integrate the installed library | [CMake setup](BUILDING.md#downstream-cmake-use), [ABI policy](ABI_POLICY.md) |

The supported build baseline is Linux x86-64 with LP64 and 64-bit GMP limbs.
The [installation chapter](manual/chapters/02_build_install.md) explains the
Linux shell setup for Windows readers using WSL.

## Check results and prepare a release

| Task | Start here |
|---|---|
| Choose and run tests | [Testing](TESTING.md) |
| Reproduce the Linux validation matrix | [Qualification](QUALIFICATION.md) |
| Assess a reported result | [Claims and evidence](CLAIMS_AND_EVIDENCE.md) |
| Prepare source archives | [Release process](RELEASE_PROCESS.md) |
| Publish reviewed artifacts | [GitHub release procedure](GITHUB_RELEASE.md) |
| Find the remaining work | [1.0 readiness](ONE_POINT_ZERO_READINESS.md), [roadmap](../ROADMAP.md) |
| Read earlier records | [History index](HISTORY_INDEX.md), [release notes](../RELEASE_NOTES.md) |

The manual explains how to use the library. Validation records describe the
source, environment and inputs checked by a particular run. Consult both when
deciding whether a result applies to your application.

[Project overview](../README.md) · [Support](../SUPPORT.md) · [Contributing](../CONTRIBUTING.md)
