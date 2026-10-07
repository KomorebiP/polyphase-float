# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

CC ?= gcc
AR ?= ar
NATIVE ?= 0
PPF_OPENMP ?= AUTO
PPF_ELF_SYMBOL_VERSIONING ?= AUTO
PPF_OPENMP_MODE := $(shell printf '%s' '$(PPF_OPENMP)' | tr '[:lower:]' '[:upper:]')
PPF_ELF_SYMBOL_VERSIONING_MODE := $(shell printf '%s' '$(PPF_ELF_SYMBOL_VERSIONING)' | tr '[:lower:]' '[:upper:]')
ifneq ($(filter $(PPF_OPENMP_MODE),AUTO ON OFF),$(PPF_OPENMP_MODE))
$(error PPF_OPENMP must be AUTO, ON, or OFF)
endif
ifneq ($(filter $(PPF_ELF_SYMBOL_VERSIONING_MODE),AUTO ON OFF),$(PPF_ELF_SYMBOL_VERSIONING_MODE))
$(error PPF_ELF_SYMBOL_VERSIONING must be AUTO, ON, or OFF)
endif
PPF_UNAME_S := $(shell uname -s 2>/dev/null || printf unknown)
PPF_ELF_SYMBOL_VERSIONING_ENABLED := 0
ifeq ($(PPF_UNAME_S),Linux)
ifneq ($(PPF_ELF_SYMBOL_VERSIONING_MODE),OFF)
PPF_ELF_SYMBOL_VERSIONING_ENABLED := 1
endif
else ifeq ($(PPF_ELF_SYMBOL_VERSIONING_MODE),ON)
$(error PPF_ELF_SYMBOL_VERSIONING=ON is currently supported only on Linux)
endif
CORE_VERSION_SCRIPT_FLAG :=
ADAPTER_VERSION_SCRIPT_FLAG :=
ifeq ($(PPF_ELF_SYMBOL_VERSIONING_ENABLED),1)
CORE_VERSION_SCRIPT_FLAG := -Wl,--version-script=abi/polyphase_float.map
ADAPTER_VERSION_SCRIPT_FLAG := -Wl,--version-script=abi/polyphase_float_mpfr.map
endif
PPF_OPENMP_PROBE := $(shell tmp=$$(mktemp /tmp/ppf-openmp-probe.XXXXXX); \
  if printf '%s\n' '#include <omp.h>' 'int main(void){return omp_get_max_threads()<1;}' | \
       $(CC) -x c -std=c11 -fopenmp -o $$tmp - >/dev/null 2>&1 && \
       $$tmp >/dev/null 2>&1; then printf 1; else printf 0; fi; rm -f $$tmp)
PPF_OPENMP_ENABLED := 0
ifneq ($(PPF_OPENMP_MODE),OFF)
ifeq ($(PPF_OPENMP_PROBE),1)
PPF_OPENMP_ENABLED := 1
else ifeq ($(PPF_OPENMP_MODE),ON)
$(error PPF_OPENMP=ON, but $(CC) has no usable OpenMP support)
endif
endif
OPENMP_CFLAGS :=
OPENMP_LDFLAGS :=
ifeq ($(PPF_OPENMP_ENABLED),1)
OPENMP_CFLAGS := -fopenmp
OPENMP_LDFLAGS := -fopenmp
endif
NATIVE_CFLAGS :=
ifeq ($(NATIVE),1)
NATIVE_CFLAGS := -march=native
endif
CFLAGS ?= -O3 $(NATIVE_CFLAGS) -std=c11 -Wall -Wextra -Wpedantic -fPIC -fvisibility=hidden
override CFLAGS += $(OPENMP_CFLAGS)
PPF_PRODUCT_PROFILE_HEADER ?= src/ppf_product_profile_build.h
PPF_PRODUCT_PROFILE_GENERATED := build/generated/ppf_product_profile_build.h
CPPFLAGS ?=
CPPFLAGS += -Ibuild/generated -Iinclude -Isrc
LDFLAGS ?= -pthread
override LDFLAGS += $(OPENMP_LDFLAGS)
GMP_LIBS ?= -lgmp -lm
MPFR_CFLAGS := $(shell pkg-config --cflags mpfr 2>/dev/null)
MPFR_LIBS := $(shell pkg-config --libs mpfr 2>/dev/null)
ifeq ($(strip $(MPFR_LIBS)),)
MPFR_LIBS := -Wl,-l:libmpfr.so.6 -lgmp
MPFR_CPPFLAGS := -DPPF_USE_MINIMAL_MPFR_ABI -Icompat
endif

# Compile, link, and execute the same optional-format probes required by the
# official MPFR header. A format family is enabled only when the compiler type,
# declarations, library symbols, and MPFR build option all agree.
define ppf_run_mpfr_probe
$(shell tmp=$$(mktemp /tmp/ppf-mpfr-probe.XXXXXX); \
  if $(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) \
       -O2 -std=c11 $(1) $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) \
       -o $$tmp >/dev/null 2>&1 && $$tmp >/dev/null 2>&1; then \
    printf 1; \
  else \
    printf 0; \
  fi; \
  rm -f $$tmp)
endef

PPF_MPFR_HAVE_FLOAT128_API := $(call ppf_run_mpfr_probe,tools/probes/mpfr_float128.c)
PPF_MPFR_HAVE_DECIMAL_API := $(call ppf_run_mpfr_probe,tools/probes/mpfr_decimal.c)
MPFR_OPTIONAL_FORMAT_CPPFLAGS := \
  -DPPF_MPFR_HAVE_FLOAT128_API=$(PPF_MPFR_HAVE_FLOAT128_API) \
  -DPPF_MPFR_HAVE_DECIMAL_API=$(PPF_MPFR_HAVE_DECIMAL_API)

CORE_SOURCES := src/abi.c src/context.c src/result_transaction.c src/arithmetic_root_transaction.c src/transcendental_special_transaction.c src/object.c src/round.c src/limb_round.c src/toom3.c src/window_product.c src/product_planner.c src/arithmetic.c src/arithmetic_variants.c src/assignment.c src/io.c src/format.c src/utilities.c src/remainder.c src/range_flags.c src/roots.c src/divisor_plan.c src/constants.c src/constant_pi.c src/constant_log2.c src/transcendental_fast.c src/transcendental.c src/trig_fast.c src/inverse_angle_fast.c src/trig_hyperbolic.c src/special_common.c src/special_gamma.c src/special_misc.c src/special_bessel.c src/runtime_support.c src/random.c src/conditional_formats.c
CORE_OBJECTS := $(patsubst src/%.c,build/%.o,$(CORE_SOURCES))
ADAPTER_OBJECT := build/mpfr_adapter.o
CORE_SHARED := lib/libpolyphase_float.so
CORE_STATIC := lib/libpolyphase_float.a
ADAPTER_SHARED := lib/libpolyphase_float_mpfr.so
ADAPTER_STATIC := lib/libpolyphase_float_mpfr.a

.PHONY: all core clean test sanitize tsan benchmark package abi-check semantic-check aliasing-check
.NOTPARALLEL: sanitize tsan
all: $(CORE_SHARED) $(CORE_STATIC) $(ADAPTER_SHARED) $(ADAPTER_STATIC)

core: $(CORE_SHARED) $(CORE_STATIC)

build lib bin build/generated:
	mkdir -p $@

$(PPF_PRODUCT_PROFILE_GENERATED): $(PPF_PRODUCT_PROFILE_HEADER) | build/generated
	cp $< $@

build/%.o: src/%.c include/polyphase_float.h src/ppf_internal.h src/transactional_arithmetic_root_names.h src/transactional_impl_prototypes.h src/constant_internal.h $(PPF_PRODUCT_PROFILE_GENERATED) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -DPPF_BUILD_SHARED -DPPF_BUILDING_LIBRARY -c $< -o $@

build/constant_pi.o: src/polyphase_chudnovsky_core.c

$(ADAPTER_OBJECT): adapter/mpfr_adapter.c include/polyphase_float_mpfr.h include/polyphase_float.h compat/mpfr_minimal.h | build
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) $(CFLAGS) -DPPF_BUILD_SHARED -DPPF_BUILDING_LIBRARY -c $< -o $@

$(CORE_SHARED): $(CORE_OBJECTS) abi/polyphase_float.map | lib
	$(CC) -shared -Wl,-soname,libpolyphase_float.so.0 $(CORE_VERSION_SCRIPT_FLAG) -o $@ $(CORE_OBJECTS) $(LDFLAGS) $(GMP_LIBS)
	ln -sf libpolyphase_float.so lib/libpolyphase_float.so.0

$(CORE_STATIC): $(CORE_OBJECTS) | lib
	$(AR) rcs $@ $^

$(ADAPTER_SHARED): $(ADAPTER_OBJECT) $(CORE_SHARED) abi/polyphase_float_mpfr.map | lib
	$(CC) -shared -Wl,-soname,libpolyphase_float_mpfr.so.0 $(ADAPTER_VERSION_SCRIPT_FLAG) -o $@ $(ADAPTER_OBJECT) -Llib -lpolyphase_float $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS)
	ln -sf libpolyphase_float_mpfr.so lib/libpolyphase_float_mpfr.so.0

$(ADAPTER_STATIC): $(ADAPTER_OBJECT) | lib
	$(AR) rcs $@ $^

bin/test_object: tests/test_object.c $(CORE_SHARED) | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(GMP_LIBS) -o $@

bin/test_allocation_failure: tests/test_allocation_failure.c $(CORE_SHARED) | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(GMP_LIBS) -o $@

bin/test_io_allocation_failure: tests/test_io_allocation_failure.c $(CORE_SHARED) | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(GMP_LIBS) -o $@

bin/test_constant_allocation_failure: tests/test_constant_allocation_failure.c $(CORE_SHARED) | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(GMP_LIBS) -o $@

bin/test_transcendental_special_allocation_failure: tests/test_transcendental_special_allocation_failure.c $(CORE_SHARED) | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(GMP_LIBS) -o $@

bin/test_arithmetic_root_allocation_failure: tests/test_arithmetic_root_allocation_failure.c $(CORE_SHARED) | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(GMP_LIBS) -o $@

bin/test_thread_safety: tests/test_thread_safety.c $(CORE_SHARED) | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(GMP_LIBS) -o $@

bin/test_format_errno: tests/test_format_errno.c $(CORE_SHARED) | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(GMP_LIBS) -o $@

bin/test_product_profile: tests/test_product_profile.c $(CORE_SHARED) | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(GMP_LIBS) -o $@

bin/test_abi_runtime: tests/test_abi_runtime.c $(CORE_SHARED) | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(GMP_LIBS) -o $@

bin/test_semantic_surface: tests/test_semantic_surface.c $(CORE_SHARED) | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(GMP_LIBS) -lm -o $@

bin/test_aliasing_matrix: tests/test_aliasing_matrix.c $(CORE_SHARED) | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(GMP_LIBS) -lm -o $@

bin/abi_layout_probe: tools/abi_layout_probe.c include/polyphase_float.h | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -o $@

bin/test_mpfr_adapter: tests/test_mpfr_adapter.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/test_mpfr_type_model: tests/test_mpfr_type_model.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/test_exhaustive_small: tests/test_exhaustive_small.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/test_utilities_remainder: tests/test_utilities_remainder.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/test_assignment_conversion: tests/test_assignment_conversion.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/test_arithmetic_range: tests/test_arithmetic_range.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/test_arithmetic_special: tests/test_arithmetic_special.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/test_arithmetic: tests/test_arithmetic.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/test_arithmetic_variants: tests/test_arithmetic_variants.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/benchmark_arithmetic_completion: benchmarks/benchmark_arithmetic_completion.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O3 -march=native -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/test_runtime_support: tests/test_runtime_support.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/test_random_runtime: tests/test_random_runtime.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/test_conditional_formats: tests/test_conditional_formats.c $(ADAPTER_SHARED) | bin
	@echo "MPFR optional formats: binary128=$(PPF_MPFR_HAVE_FLOAT128_API) decimal=$(PPF_MPFR_HAVE_DECIMAL_API)"
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) $(MPFR_OPTIONAL_FORMAT_CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/test_utilities: tests/test_utilities.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@


bin/test_io: tests/test_io.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/test_format: tests/test_format.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/test_range_flags: tests/test_range_flags.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/test_roots: tests/test_roots.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/test_divisor_plan: tests/test_divisor_plan.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/test_constants: tests/test_constants.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/test_transcendental_complete: tests/test_transcendental_complete.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/test_transcendental_random: tests/test_transcendental_random.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/test_transcendental_range: tests/test_transcendental_range.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/test_transcendental_large: tests/test_transcendental_large.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

TRIG_TESTS := trig_hyperbolic trig_hyperbolic_special trig_hyperbolic_random trig_hyperbolic_large trig_hyperbolic_alias
SPECIAL_TESTS := special_functions special_random special_values special_faithful special_constants special_alias special_large
.SECONDARY: $(addprefix bin/test_,$(TRIG_TESTS)) $(addprefix bin/test_,$(SPECIAL_TESTS))

bin/test_trig_%: tests/test_trig_%.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/test_special_%: tests/test_special_%.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/benchmark_trig_hyperbolic: benchmarks/benchmark_trig_hyperbolic.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O3 -march=native -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/benchmark_special_functions: benchmarks/benchmark_special_functions.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O3 -march=native -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/benchmark_transcendental: benchmarks/benchmark_transcendental.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O3 -march=native -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/benchmark_transcendental_method: benchmarks/benchmark_transcendental_method.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O3 -march=native -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/benchmark_constants: benchmarks/benchmark_constants.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O3 -march=native -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/benchmark_range_roots_divplan: benchmarks/benchmark_range_roots_divplan.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O3 -march=native -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/benchmark_arithmetic: benchmarks/benchmark_arithmetic.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O3 -march=native -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/benchmark_assignment_conversion: benchmarks/benchmark_assignment_conversion.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O3 -march=native -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/benchmark_utilities_remainder: benchmarks/benchmark_utilities_remainder.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O3 -march=native -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/benchmark_io_formatting: benchmarks/benchmark_io_formatting.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O3 -march=native -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/benchmark_runtime_completion: benchmarks/benchmark_runtime_completion.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O3 -march=native -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/benchmark_object: benchmarks/benchmark_object.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O3 -march=native -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

test: bin/test_object bin/test_allocation_failure bin/test_io_allocation_failure bin/test_constant_allocation_failure bin/test_transcendental_special_allocation_failure bin/test_arithmetic_root_allocation_failure bin/test_thread_safety bin/test_format_errno bin/test_product_profile bin/test_abi_runtime bin/test_semantic_surface bin/test_aliasing_matrix bin/test_mpfr_adapter bin/test_mpfr_type_model bin/test_exhaustive_small bin/test_arithmetic bin/test_arithmetic_variants bin/test_arithmetic_special bin/test_arithmetic_range bin/test_assignment_conversion bin/test_io bin/test_format bin/test_utilities bin/test_utilities_remainder bin/test_range_flags bin/test_roots bin/test_divisor_plan bin/test_constants bin/test_product_planner bin/test_transcendental_complete bin/test_transcendental_random bin/test_transcendental_range bin/test_transcendental_large $(addprefix bin/test_,$(TRIG_TESTS)) $(addprefix bin/test_,$(SPECIAL_TESTS)) bin/test_runtime_support bin/test_random_runtime bin/test_conditional_formats
	./bin/test_object
	./bin/test_allocation_failure
	./bin/test_io_allocation_failure
	./bin/test_constant_allocation_failure
	./bin/test_transcendental_special_allocation_failure
	./bin/test_arithmetic_root_allocation_failure
	./bin/test_thread_safety
	./bin/test_format_errno
	./bin/test_product_profile
	./bin/test_abi_runtime
	./bin/test_semantic_surface
	./bin/test_aliasing_matrix
	./bin/test_mpfr_adapter
	./bin/test_mpfr_type_model
	./bin/test_exhaustive_small
	./bin/test_arithmetic
	./bin/test_arithmetic_variants
	./bin/test_arithmetic_special
	./bin/test_arithmetic_range
	./bin/test_assignment_conversion
	./bin/test_io
	./bin/test_format
	./bin/test_utilities_remainder
	./bin/test_utilities
	./bin/test_range_flags
	./bin/test_roots
	./bin/test_divisor_plan
	./bin/test_constants
	./bin/test_product_planner
	./bin/test_transcendental_complete
	./bin/test_transcendental_random
	./bin/test_transcendental_range
	./bin/test_transcendental_large
	./bin/test_trig_hyperbolic
	./bin/test_trig_hyperbolic_special
	./bin/test_trig_hyperbolic_random
	./bin/test_trig_hyperbolic_large
	./bin/test_trig_hyperbolic_alias
	./bin/test_special_functions
	./bin/test_special_random
	./bin/test_special_values
	./bin/test_special_faithful
	./bin/test_special_constants
	./bin/test_special_alias
	./bin/test_special_large
	./bin/test_runtime_support
	./bin/test_random_runtime
	./bin/test_conditional_formats

semantic-check: aliasing-check
	python3 tools/semantic_contract.py verify --root . --contract semantics/contract-v1.json
	python3 tests/test_semantic_contract.py --root .

aliasing-check: bin/test_aliasing_matrix
	./bin/test_aliasing_matrix
	python3 tools/aliasing_contract.py verify --root . --contract semantics/aliasing-contract-v1.json
	python3 tests/test_aliasing_contract.py --root .

abi-check: $(CORE_SHARED) $(ADAPTER_SHARED) bin/test_abi_runtime bin/abi_layout_probe
	./bin/test_abi_runtime
	python3 tools/abi_contract.py verify --root . --contract abi/contract-v1.json
	python3 tools/abi_contract.py compare --baseline abi/baselines/0.11.0-contract.json --current abi/contract-v1.json
	python3 tools/abi_contract.py compare --baseline abi/baselines/0.17.0-contract.json --current abi/contract-v1.json
	python3 tools/abi_layout_compare.py --probe ./bin/abi_layout_probe --baseline abi/baselines/0.11.0-linux-x86_64-lp64-gmp64-layout.json --skip-model-mismatch
	python3 tools/abi_exports.py --root . --core-library $(CORE_SHARED) --adapter-library $(ADAPTER_SHARED) $(if $(filter 1,$(PPF_ELF_SYMBOL_VERSIONING_ENABLED)),--require-versions,)

benchmark: bin/benchmark_object bin/benchmark_arithmetic bin/benchmark_assignment_conversion bin/benchmark_io_formatting bin/benchmark_utilities_remainder bin/benchmark_range_roots_divplan bin/benchmark_speedup_method bin/benchmark_constants bin/benchmark_product_planner bin/benchmark_transcendental bin/benchmark_trig_hyperbolic bin/benchmark_special_functions
	./bin/benchmark_object 1000000 21
	./bin/benchmark_arithmetic 1000000 9 4
	./bin/benchmark_arithmetic_completion 10000 9
	./bin/benchmark_assignment_conversion 1000000 9
	./bin/benchmark_io_formatting 1000000 9
	./bin/benchmark_utilities_remainder 1000000 9
	./bin/benchmark_range_roots_divplan 1000000 5 4
	./bin/benchmark_constants ppf_pi_cold 1000000 4 3
	./bin/benchmark_product_planner 1000000 3 5 1
	./bin/benchmark_transcendental 1000 3
	./bin/benchmark_trig_hyperbolic 1000 3
	./bin/benchmark_special_functions 128 3

sanitize:
	$(MAKE) clean
	$(MAKE) -j1 CFLAGS='-O1 -g -std=c11 -Wall -Wextra -Wpedantic -fPIC -fvisibility=hidden $(OPENMP_CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-pthread $(OPENMP_LDFLAGS) -fsanitize=address,undefined' all bin/test_object bin/test_allocation_failure bin/test_io_allocation_failure bin/test_constant_allocation_failure bin/test_transcendental_special_allocation_failure bin/test_arithmetic_root_allocation_failure bin/test_thread_safety bin/test_format_errno bin/test_product_profile bin/test_aliasing_matrix bin/test_mpfr_adapter bin/test_mpfr_type_model bin/test_exhaustive_small bin/test_arithmetic bin/test_arithmetic_variants bin/test_arithmetic_special bin/test_arithmetic_range bin/test_assignment_conversion bin/test_io bin/test_format bin/test_utilities_remainder bin/test_utilities bin/test_range_flags bin/test_roots bin/test_divisor_plan bin/test_constants bin/test_product_planner bin/test_transcendental_complete bin/test_transcendental_random bin/test_transcendental_range bin/test_transcendental_large $(addprefix bin/test_,$(TRIG_TESTS)) $(addprefix bin/test_,$(SPECIAL_TESTS)) bin/test_runtime_support bin/test_random_runtime bin/test_conditional_formats
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_object
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_allocation_failure
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_io_allocation_failure
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_constant_allocation_failure
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_transcendental_special_allocation_failure
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_arithmetic_root_allocation_failure
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_thread_safety
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_format_errno
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_product_profile
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_semantic_surface
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_aliasing_matrix
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_mpfr_adapter
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_mpfr_type_model
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_exhaustive_small
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_arithmetic
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_arithmetic_variants
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_arithmetic_special
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_arithmetic_range
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_assignment_conversion
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_io
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_format
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_utilities_remainder
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_utilities
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_range_flags
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_roots
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_divisor_plan
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_constants
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_product_planner
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_transcendental_complete
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_transcendental_random
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_transcendental_range
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_transcendental_large
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_trig_hyperbolic
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_trig_hyperbolic_special
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_trig_hyperbolic_random
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_trig_hyperbolic_large
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_trig_hyperbolic_alias
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_special_functions
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_special_random
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_special_values
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_special_faithful
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_special_constants
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_special_alias
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_special_large
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_runtime_support
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_random_runtime
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./bin/test_conditional_formats
	$(MAKE) clean


tsan:
	$(MAKE) clean
	$(MAKE) -j1 PPF_OPENMP=OFF CFLAGS='-O1 -g -std=c11 -Wall -Wextra -Wpedantic -Werror -fPIC -fvisibility=hidden -fsanitize=thread -fno-omit-frame-pointer' LDFLAGS='-pthread -fsanitize=thread' core bin/test_thread_safety bin/test_object
	PPF_THREAD_STRESS_MULTIPLIER=4 TSAN_OPTIONS=halt_on_error=1:history_size=7:second_deadlock_stack=1 ./bin/test_thread_safety
	TSAN_OPTIONS=halt_on_error=1:history_size=7:second_deadlock_stack=1 ./bin/test_object
	$(MAKE) clean

clean:
	rm -rf build/generated build/mpfr_adapter.o bin lib polyphase-float.pc

PREFIX ?= /usr/local
INCLUDEDIR ?= $(PREFIX)/include
LIBDIR ?= $(PREFIX)/lib
PKGCONFIGDIR ?= $(LIBDIR)/pkgconfig
PC_LIBDIR ?= $(patsubst $(PREFIX)/%,%,$(LIBDIR))
PC_INCLUDEDIR ?= $(patsubst $(PREFIX)/%,%,$(INCLUDEDIR))

.PHONY: audit examples install

audit: $(CORE_SHARED)
	python3 tools/audit_independence.py $(CORE_SHARED)

coverage: $(CORE_SHARED)
	python3 scripts/check_api_coverage.py

bin/example_basic: examples/basic.c $(CORE_SHARED) | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(GMP_LIBS) -o $@

bin/example_mpfr_bridge: examples/mpfr_bridge.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/example_arithmetic: examples/arithmetic.c $(CORE_SHARED) | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(GMP_LIBS) -o $@

bin/example_assignment_conversion: examples/assignment_conversion.c $(CORE_SHARED) | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(GMP_LIBS) -o $@

bin/example_utilities_remainder: examples/utilities_remainder.c $(CORE_SHARED) | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(GMP_LIBS) -o $@

bin/example_constants: examples/constants.c $(CORE_SHARED) | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(GMP_LIBS) -o $@

bin/example_range_roots_division: examples/range_roots_division.c $(CORE_SHARED) | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(GMP_LIBS) -o $@

bin/example_transcendentals: examples/transcendentals.c $(CORE_SHARED) | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(GMP_LIBS) -o $@

bin/example_trig_hyperbolic: examples/trig_hyperbolic.c $(CORE_SHARED) | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(GMP_LIBS) -o $@

bin/example_special_functions: examples/special_functions.c $(CORE_SHARED) | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(GMP_LIBS) -o $@

bin/example_io_formatting: examples/io_formatting.c $(CORE_SHARED) | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(GMP_LIBS) -o $@

bin/example_arithmetic_completion: examples/arithmetic_completion.c $(CORE_SHARED) | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(GMP_LIBS) -o $@

bin/example_runtime_completion: examples/runtime_completion.c $(CORE_SHARED) | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(GMP_LIBS) -o $@

examples: bin/example_basic bin/example_mpfr_bridge bin/example_arithmetic bin/example_assignment_conversion bin/example_utilities_remainder bin/example_range_roots_division bin/example_constants bin/example_transcendentals bin/example_trig_hyperbolic bin/example_special_functions bin/example_io_formatting bin/example_arithmetic_completion bin/example_runtime_completion

VERSION ?= 0.18.0
SOVERSION ?= 0
DOCDIR ?= $(PREFIX)/share/doc/PolyphaseFloat

polyphase-float.pc: polyphase-float.pc.in
	sed -e 's|@CMAKE_INSTALL_LIBDIR@|$(PC_LIBDIR)|g' \
	    -e 's|@CMAKE_INSTALL_INCLUDEDIR@|$(PC_INCLUDEDIR)|g' \
	    -e 's|@PROJECT_VERSION@|$(VERSION)|g' \
	    -e 's|@PPF_PC_OPENMP_LIB@|$(OPENMP_LDFLAGS)|g' \
	    -e 's|@PPF_PC_MATH_LIB@|-lm|g' $< > $@

install: all polyphase-float.pc
	install -d $(DESTDIR)$(INCLUDEDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(PKGCONFIGDIR) $(DESTDIR)$(DOCDIR) $(DESTDIR)$(DOCDIR)/semantics $(DESTDIR)$(DOCDIR)/abi
	install -m 0644 include/polyphase_float.h include/polyphase_float_mpfr.h $(DESTDIR)$(INCLUDEDIR)/
	install -m 0755 $(CORE_SHARED) $(DESTDIR)$(LIBDIR)/libpolyphase_float.so.$(VERSION)
	ln -sf libpolyphase_float.so.$(VERSION) $(DESTDIR)$(LIBDIR)/libpolyphase_float.so.$(SOVERSION)
	ln -sf libpolyphase_float.so.$(SOVERSION) $(DESTDIR)$(LIBDIR)/libpolyphase_float.so
	install -m 0755 $(ADAPTER_SHARED) $(DESTDIR)$(LIBDIR)/libpolyphase_float_mpfr.so.$(VERSION)
	ln -sf libpolyphase_float_mpfr.so.$(VERSION) $(DESTDIR)$(LIBDIR)/libpolyphase_float_mpfr.so.$(SOVERSION)
	ln -sf libpolyphase_float_mpfr.so.$(SOVERSION) $(DESTDIR)$(LIBDIR)/libpolyphase_float_mpfr.so
	install -m 0644 $(CORE_STATIC) $(ADAPTER_STATIC) $(DESTDIR)$(LIBDIR)/
	install -m 0644 polyphase-float.pc $(DESTDIR)$(PKGCONFIGDIR)/
	install -m 0644 README.md LICENSE.md LICENSE COPYING AUTHORS.md THIRD_PARTY_NOTICES.md ROADMAP.md docs/ABI_POLICY.md docs/ABI_STABILITY_VALIDATION_0.12.0.md docs/ABI_STABILITY_RELEASE_REPORT_0.12.0.md docs/ABI_COMPATIBILITY_0.14.0.md docs/ABI_COMPATIBILITY_0.15.0.md docs/PUBLIC_SEMANTIC_CONTRACT_0.15.0.md docs/SEMANTIC_CONTRACT_VALIDATION_0.15.0.md docs/ALIASING_CONTRACT_0.15.0.md docs/ALIASING_CONTRACT_VALIDATION_0.15.0.md docs/IO_ALLOCATION_CONTRACT_0.15.0.md docs/IO_ALLOCATION_HARDENING_VALIDATION_0.15.0.md docs/COMPLETION_STATUS_0.15.0.md docs/PRODUCTION_CHANGESET_0.15.0.json docs/ABI_COMPATIBILITY_0.16.0.md docs/PUBLIC_SEMANTIC_CONTRACT_0.16.0.md docs/SEMANTIC_CONTRACT_VALIDATION_0.16.0.md docs/ALIASING_CONTRACT_0.16.0.md docs/ALIASING_CONTRACT_VALIDATION_0.16.0.md docs/CONSTANT_ALLOCATION_CONTRACT_0.16.0.md docs/CONSTANT_ALLOCATION_HARDENING_VALIDATION_0.16.0.md docs/COMPLETION_STATUS_0.16.0.md docs/PRODUCTION_CHANGESET_0.16.0.json docs/ABI_COMPATIBILITY_0.17.0.md docs/PUBLIC_SEMANTIC_CONTRACT_0.17.0.md docs/SEMANTIC_CONTRACT_VALIDATION_0.17.0.md docs/ALIASING_CONTRACT_0.17.0.md docs/ALIASING_CONTRACT_VALIDATION_0.17.0.md docs/TRANSCENDENTAL_SPECIAL_ALLOCATION_CONTRACT_0.17.0.md docs/TRANSCENDENTAL_SPECIAL_ALLOCATION_HARDENING_VALIDATION_0.17.0.md docs/COMPLETION_STATUS_0.17.0.md docs/PRODUCTION_CHANGESET_0.17.0.json docs/ABI_COMPATIBILITY_0.18.0.md docs/PUBLIC_SEMANTIC_CONTRACT_0.18.0.md docs/SEMANTIC_CONTRACT_VALIDATION_0.18.0.md docs/ALIASING_CONTRACT_0.18.0.md docs/ALIASING_CONTRACT_VALIDATION_0.18.0.md docs/ARITHMETIC_FUSED_ROOT_ALLOCATION_CONTRACT_0.18.0.md docs/ARITHMETIC_FUSED_ROOT_ALLOCATION_HARDENING_VALIDATION_0.18.0.md docs/COMPLETION_STATUS_0.18.0.md docs/PRODUCTION_CHANGESET_0.18.0.json $(DESTDIR)$(DOCDIR)/
	install -m 0644 semantics/contract-v1.json semantics/function-matrix-v1.csv semantics/aliasing-contract-v1.json semantics/aliasing-matrix-v1.csv $(DESTDIR)$(DOCDIR)/semantics/
	install -m 0644 abi/contract-v1.json $(DESTDIR)$(DOCDIR)/abi/

bin/benchmark_speedup_method: benchmarks/benchmark_speedup_method.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O3 -march=native -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/benchmark_hotpath_method: benchmarks/benchmark_hotpath_method.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O3 -march=native -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/benchmark_recsqrt_matched: benchmarks/benchmark_recsqrt_matched.c $(CORE_SHARED) $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O3 -march=native -std=c11 -Wall -Wextra -Wpedantic $(OPENMP_CFLAGS) $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/test_product_planner: tests/test_product_planner.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/benchmark_product_planner: benchmarks/benchmark_product_planner.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O3 -march=native -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@

bin/inspect_product_plan: benchmarks/inspect_product_plan.c $(CORE_SHARED) | bin
	$(CC) $(CPPFLAGS) -O2 -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(GMP_LIBS) -o $@

bin/benchmark_recsqrt_planner: benchmarks/benchmark_recsqrt_planner.c $(ADAPTER_SHARED) | bin
	$(CC) $(CPPFLAGS) $(MPFR_CPPFLAGS) $(MPFR_CFLAGS) -O3 -march=native -std=c11 -Wall -Wextra -Wpedantic $< -Llib -lpolyphase_float_mpfr -lpolyphase_float -Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) $(MPFR_LIBS) $(GMP_LIBS) -o $@
