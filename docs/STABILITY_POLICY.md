# Stability and maintenance policy

## Scope of the commitment

Polyphase Float retains C ABI epoch 1 across compatible 0.x releases from
0.11.0 onward and throughout the 1.x series. The governed platform is Linux
x86-64 ELF with the LP64/GMP64 data model and the supported C calling
convention. Other platforms require their own qualification before this
commitment applies to them.

This policy settles compatibility and maintenance decisions for a future 1.x
release. It does not declare that version 1.0 has passed its numerical,
independent-review, or reproduction requirements. The current version remains
0.20.0; the [roadmap](../ROADMAP.md) records the promotion requirements.

## Public contract

The stable interface consists of the public headers, the epoch-1 ABI record,
the semantic and aliasing contracts, and the documented supported domains and
error behavior. In particular, compatible releases preserve:

- existing function declarations, calling conventions, public structure
  layouts, enum values, and ABI-governed constants on the governed data model;
- ownership, initialization, cleanup, and supported aliasing rules;
- the meanings of rounding modes, successful return values, sticky flags,
  documented numerical rejection, and the explicitly covered allocation
  rollback guarantees;
- the distinction between the GMP-dependent core and the optional MPFR
  adapter, and the documented conditions on compiler-dependent interfaces;
- existing ELF symbols and their version nodes.

The contract is bounded. It does not promise every mathematical operation for
every precision, argument, exponent range, or allocation size. See the
[release scope](COMPLETION_SCOPE_0.20.0.md), public semantic ledger, and
function-specific documentation. Exposed structure fields are governed for
layout compatibility; arbitrary direct mutation is not a supported substitute
for the public functions.

Release-version and generated contract-digest metadata identify the installed
release and may change when their underlying version or contract record
changes. They are not a promise that every diagnostic string is identical
between compatible releases.

A correction to erroneous results, memory safety, or behavior that contradicts
the documented contract is a bug fix. It must have an explicit description and
a regression test. A fix that cannot preserve the governed contract requires
the breaking-change procedure in the [ABI policy](ABI_POLICY.md); it must not
silently change an existing compatible release's interface. New functions and
newly supported domains may be added compatibly, with their own documentation
and evidence.

## Library and package versions

The project version is independent of the ABI epoch and shared-library SONAME.
Compatible 1.x releases retain both `libpolyphase_float.so.0` and
`libpolyphase_float_mpfr.so.0`, ABI epoch 1, and the inherited symbol-version
nodes. A product version of 1.0 therefore does not itself cause a dynamic
linker transition. CMake and Make use the same SONAME decision.

CMake package selection follows these rules:

| Installed version | Minimum-version request | Selection policy |
| --- | --- | --- |
| Compatible 0.x | 0.11.0 or later within 0.x | Accept when the installed version is at least the requested version. |
| Compatible 1.x | 0.11.0 or later within 0.x or 1.x | Accept when the installed version is at least the requested version. |
| Either series | Earlier than 0.11.0 | Reject; this is outside the governed baseline. |
| Either series | A newer major or unavailable newer version | Reject. |

`EXACT` remains CMake's exact-version match; a 1.0.0 package does not satisfy
`0.20.0 EXACT`. Version ranges retain their lower and upper bounds, including
an excluded upper bound. For example, 1.0.0 satisfies `0.11.0...<2.0.0` but
does not satisfy `0.11.0...<1.0.0`. Current 0.x packages retain CMake's
`SameMajorVersion` range rules. The generated version file also retains
CMake's pointer-size suitability check when that information is available.
The governed minimum still applies to consumers that have not enabled a
compiled language. Unversioned `find_package` makes no version request and
does not run CMake's version-file suitability check.

These checks do not certify an otherwise unqualified platform. A future
project major of 2 or higher requires an explicit compatibility-policy
decision. `tests/test_package_version.py` exercises the selection rules with
current and future-provider fixtures; `make package-version-check` runs the
same check when CMake and Python are available.

The PPFIF interchange format is a separate serialized-data contract. Its
version and validation rules do not change merely because the project or ABI
version changes.

## Maintenance and support

For the 1.x series, fixes are supplied as maintainer resources permit through
the latest compatible 1.x release. Users should normally update to that
release. Compatibility is a commitment across the series, not a promise to
backport every fix to every earlier minor or patch release.

There is no guaranteed calendar maintenance window, response-time agreement,
or fixed release schedule. Any planned end of maintenance must be stated
explicitly in the support and security policies, with available migration
guidance. Until a 1.x release is published, the existing pre-1.0 maintenance
scope in [Security policy](../SECURITY.md) continues to apply.

Report reproducible defects using [Support](../SUPPORT.md). Suspected security
issues follow the private-reporting procedure in the security policy. Release
notes must distinguish a documented commitment, locally tested evidence,
mathematical argument, and independent reproduction; none substitutes for
another.
