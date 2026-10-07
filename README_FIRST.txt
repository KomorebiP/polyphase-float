POLYPHASE FLOAT 0.18.0 — PUBLIC PRE-1.0 SOURCE RELEASE

Start with README.md.
Repository: https://github.com/KomorebiP/polyphase-float
Publication procedure: docs/GITHUB_RELEASE.md

Portable independent build:

  cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DPPF_OPENMP=AUTO -DPPF_MPFR_SUPPORT=OFF
  cmake --build build/release -j

Guaranteed serial build:

  cmake -S . -B build/serial -DCMAKE_BUILD_TYPE=Release -DPPF_OPENMP=OFF -DPPF_MPFR_SUPPORT=OFF
  cmake --build build/serial -j

Generated public semantic and aliasing contracts:

  python3 tools/semantic_contract.py verify --root . --contract semantics/contract-v1.json
  python3 tests/test_semantic_contract.py --root .
  python3 tools/aliasing_contract.py verify --root . --contract semantics/aliasing-contract-v1.json
  python3 tests/test_aliasing_contract.py --root .
  cmake --build build/release --target semantic-contract-audit

Allocation-atomicity matrices:

  ctest --test-dir build/release -R '^(constant|transcendental_special|arithmetic_root)_allocation_failure$' --output-on-failure

ThreadSanitizer lifecycle validation:

  cmake --preset tsan
  cmake --build --preset tsan -j
  ctest --preset tsan

Full differential validation when MPFR development files are installed:

  cmake -S . -B build/validation -DCMAKE_BUILD_TYPE=RelWithDebInfo -DPPF_OPENMP=AUTO -DPPF_MPFR_SUPPORT=ON -DPPF_BUILD_BENCHMARKS=ON -DPPF_WARNINGS_AS_ERRORS=ON
  cmake --build build/validation -j
  ctest --test-dir build/validation --output-on-failure -j1

License: LGPL-3.0-or-later
Current inventory status: 349/349 direct or conditional counterparts
Public exported functions: 437
Public semantic ledger: 437/437 indexed and directly test-referenced; 191 entries retain explicit gaps
Allocation-atomicity gaps: 81
Verified transactional public functions: 213
Aliasing ledger: 437/437 classified; 0 unverified aliasing entries
Public ABI: governed epoch 1 on Linux ELF LP64/GMP64
