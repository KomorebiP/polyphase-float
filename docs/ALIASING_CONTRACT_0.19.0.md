# Polyphase Float aliasing contract

Release: `0.19.0`  
Contract version: `1`  
Contract SHA-256: `f8e0428e22a97df6bc6972e609731d5bb8946ba8fc611c0e210cf2cc068f2abe`

## Contract boundary

This contract governs overlap among public Polyphase floating objects and the lifecycle preconditions that can make an apparent in-place call invalid. It does not authorize overlapping raw buffers in advanced direct-storage interfaces unless those interfaces state their own storage rule.

A verified in-place classification means the destination/source positions named by the contract were compared with out-of-place execution, including numerical value, ternary relation, and context flags. The systematic matrix exercises all six public rounding modes. Multi-source functions additionally exercise each destination/source position and the all-sources-identical case.

## Summary

| Item | Count |
|---|---:|
| Public core functions | **437** |
| Unverified aliasing entries | **0** |
| Governed distinct-output functions | **5** |
| Governed same-index batch functions | **1** |
| Source-distinct preconditions | **2** |

## Classification counts

| Classification | Functions |
|---|---:|
| `direct_storage` | 11 |
| `lifecycle_destination` | 17 |
| `mutable_object` | 15 |
| `not_applicable` | 222 |
| `source_must_be_distinct` | 2 |
| `verified_distinct_outputs` | 5 |
| `verified_in_place` | 164 |
| `verified_same_index_overlap` | 1 |

## Governed multi-output rules

| Function | Allowed overlap | Forbidden overlap | Invalid-overlap behavior |
|---|---|---|---|
| `ppf_modf` | either floating output may alias the floating input individually | the two floating outputs must not be the same object | returns the function error sentinel before modifying the shared output object |
| `ppf_grandom` | the optional second output may be NULL | the two floating outputs must not be the same object | returns PPF_TERNARY_ERROR, raises PPF_FLAG_ERANGE, and preserves the shared object |
| `ppf_div_with_plan_many` | outputs[i] may equal numerators[i] for each index | duplicate outputs and cross-index output/input overlap | returns failure and raises PPF_FLAG_ERANGE before starting the batch |
| `ppf_sin_cos` | either floating output may alias the floating input individually | the two floating outputs must not be the same object | returns the function error sentinel before modifying the shared output object |
| `ppf_sinh_cosh` | either floating output may alias the floating input individually | the two floating outputs must not be the same object | returns the function error sentinel before modifying the shared output object |
| `ppf_get_constant_interval` | lower and upper may refer to distinct caller-owned integers | lower and upper must not be the same mpz object | returns failure before modifying either endpoint |

## Reproduction

```sh
python3 tools/aliasing_contract.py verify --root . --contract semantics/aliasing-contract-v1.json
python3 tests/test_aliasing_contract.py --root .
ctest --test-dir build/validation -R 'aliasing_(matrix|contract)' --output-on-failure
make aliasing-check
```

The JSON file is canonical. The CSV is a flattened review surface generated from the same data.
