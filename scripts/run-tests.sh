#!/usr/bin/env bash
# URUS test runner — POSIX shells
#
# Marker support (first 30 lines of each test file):
#   // harness: skip          — don't run this file (documentation marker)
#   // harness: args <flags>  — extra urusc flags for this file
#
# b020: also self-tests the --max-input-bytes override.
set -eu
BIN=${1:-compiler/build/urusc}
PASS=0; FAIL=0; SKIP=0

directives() {
    # prints "skip" and/or "args:<flags>" lines for the given file
    head -n 30 "$1" | sed -n \
        -e 's|^[[:space:]]*//[[:space:]]*harness:[[:space:]]*skip[[:space:]]*$|skip|p' \
        -e 's|^[[:space:]]*//[[:space:]]*harness:[[:space:]]*args[[:space:]]*\(.*\)$|args:\1|p'
}

run_one() {
    # $1 = file, $2 = expect ("ok" or "err")
    f=$1; expect=$2
    name=$(basename "$f")
    extra=""
    skip=0
    while IFS= read -r d; do
        case "$d" in
            skip)   skip=1 ;;
            args:*) extra=${d#args:} ;;
        esac
    done <<EOF
$(directives "$f")
EOF
    if [ "$skip" -eq 1 ]; then
        echo "  SKIP  $name"; SKIP=$((SKIP+1)); return
    fi
    # shellcheck disable=SC2086
    if "$BIN" "$f" --emit-c $extra >/dev/null 2>&1; then rc=0; else rc=1; fi
    if { [ "$expect" = ok ] && [ "$rc" -eq 0 ]; } || \
       { [ "$expect" = err ] && [ "$rc" -ne 0 ]; }; then
        echo "  PASS  $name"; PASS=$((PASS+1))
    else
        echo "  FAIL  $name"; FAIL=$((FAIL+1))
    fi
}

echo "==> tests/run"
for f in tests/run/*.urus; do run_one "$f" ok; done

echo "==> tests/fail"
for f in tests/fail/*.urus; do run_one "$f" err; done

echo "==> harness self-tests"
if ! "$BIN" tests/run/01*.urus --emit-c --max-input-bytes 16 >/dev/null 2>&1; then
    echo "  PASS  --max-input-bytes 16 refuses oversized input"; PASS=$((PASS+1))
else
    echo "  FAIL  --max-input-bytes 16 should have refused"; FAIL=$((FAIL+1))
fi
if ! "$BIN" tests/run/01*.urus --emit-c --max-input-bytes 0 >/dev/null 2>&1; then
    echo "  PASS  --max-input-bytes 0 rejected as invalid"; PASS=$((PASS+1))
else
    echo "  FAIL  --max-input-bytes 0 should be invalid"; FAIL=$((FAIL+1))
fi

echo "Result: $PASS passed, $FAIL failed, $SKIP skipped"
[ "$FAIL" -eq 0 ]
