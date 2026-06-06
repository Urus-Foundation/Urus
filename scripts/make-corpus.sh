#!/usr/bin/env bash
# Seed the libFuzzer corpus from the test suite (v0.0.1-b023).
# Every .urus file in tests/run + tests/fail becomes a corpus entry —
# positive cases give the fuzzer deep-pipeline coverage, negative cases
# seed the diagnostic paths.
set -eu
DEST=${1:-fuzz/corpus}
mkdir -p "$DEST"
n=0
for f in tests/run/*.urus tests/fail/*.urus examples/*.urus; do
    [ -f "$f" ] || continue
    cp "$f" "$DEST/seed-$(basename "${f%.urus}")"
    n=$((n+1))
done
echo "corpus: $n seeds in $DEST"
