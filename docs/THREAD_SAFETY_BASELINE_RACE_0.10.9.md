# Historical 0.10.9 descriptor race

## Purpose

This record preserves the defect that motivated the 0.11.0 descriptor change.
It is a historical reproduction, not a current-release failure.

## Reproduction

Eight POSIX threads repeatedly called `ppf_const_euler` against independent
contexts in a GCC ThreadSanitizer build of the unmodified 0.10.9 source.
ThreadSanitizer exited with status 66 and reported a data race at:

```text
ppf_euler_constant_spec
src/special_common.c:479
```

The 0.10.9 accessor rewrote the fields of a function-local static descriptor on
every call. The values written were identical, but concurrent unsynchronized
writes still violate the C memory model.

## Correction

Version 0.11.0 initializes the Euler and Catalan descriptors, and their cache
objects, inside one `pthread_once` callback. The descriptors are immutable
after that callback returns.

The focused historical probe reports the race on 0.10.9. The same concurrent
first-use and cleanup workload passes in the 0.11.0 ThreadSanitizer inventory.
