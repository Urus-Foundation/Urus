/*
 * libFuzzer target for the URUS compile pipeline (v0.0.1-b023).
 *
 * Drives urus_compile_buffer — the exact production entry point used by
 * `urusc --emit-c` — with mutated byte streams.  Any crash, sanitizer
 * report, or hang here is by definition a compiler bug: the pipeline must
 * reject arbitrary input with diagnostics, never with UB.
 *
 * Build (clang only):
 *   cmake -B build-fuzz -S . -DURUS_BUILD_FUZZER=ON \
 *         -DCMAKE_C_COMPILER=clang
 *   cmake --build build-fuzz
 *   ./build-fuzz/fuzz/fuzz_compile fuzz/corpus -max_total_time=300
 *
 * The seed corpus is generated from tests/ by scripts/make-corpus.sh.
 */
#include <stddef.h>
#include <stdint.h>
#include "compile.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* Cap fuzz input at 64 KiB.  Every resource-cap path triggers well
     * below this, and codegen output can be super-linear in input size
     * (nested statement-expressions, defer flushing), so bigger inputs
     * only buy slower exec/s and RSS spikes, not coverage. */
    if (size > (64u << 10)) return 0;
    urus_compile_buffer((const char *)data, size, "fuzz", NULL);
    return 0;
}
