#!/usr/bin/env bash
# run_extra_tests.sh
# For each extra test:
#   1. Run the C++ simulator  -> user_output.json
#   2. Run reference_sim.py   -> output.json  (ground truth)
#   3. Compare with compare.py

./build.sh
echo ""

PASS=0
FAIL=0

echo "=== Extra tests (C++ simulator vs Python reference) ==="
echo ""

for tnum in ./extra_tests/*/; do
    desc=$(cat "${tnum}desc.txt")
    echo "[$(basename $tnum)] $desc"

    # Generate reference output
    python ./reference_sim.py "${tnum}input.json" "${tnum}output.json" 2>/dev/null

    # Run C++ simulator
    ./simulator "${tnum}input.json" "${tnum}user_output.json"

    # Compare
    result=$(python ./compare.py "${tnum}user_output.json" -r "${tnum}output.json" 2>&1)
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
