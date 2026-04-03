#!/bin/bash
# validate_reference.sh
# Validates the Python reference simulator against all 9 given tests.
# Run this first to confirm reference_sim.py is correct before using it
# as a ground truth for extra tests.

PASS=0
FAIL=0

echo "=== Validating reference_sim.py against given tests ==="
echo ""

for tnum in ./given_tests/*/; do
    desc=$(cat "${tnum}desc.txt")
    echo "[$(basename $tnum)] $desc"

    python ./reference_sim.py "${tnum}input.json" "/tmp/ref_check.json" 2>/dev/null

    result=$(python ./compare.py /tmp/ref_check.json -r "${tnum}output.json" 2>&1)
    if echo "$result" | grep -q "PASSED"; then
        echo "  PASSED"
        PASS=$((PASS+1))
    else
        echo "  FAILED"
        echo "$result" | grep "\[Error\]" | head -5
        FAIL=$((FAIL+1))
    fi
    echo ""
done

echo "=== Result: $PASS passed, $FAIL failed ==="
