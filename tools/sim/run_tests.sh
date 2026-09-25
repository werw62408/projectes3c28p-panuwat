#!/bin/bash
# run_tests.sh <sketch folder>   builds and runs every test_*.cpp, prints PASS/FAIL lines
cd "$(dirname "$0")"
SK="${1:-../../SomudTick_v11.2/SomudTick}"
mkdir -p run
fail=0
for t in test_*.cpp; do
  n=${t%.cpp}
  if ! ./build.sh "$t" "$SK" "$n" 2> "run/$n.build.log"; then echo "BUILD FAILED: $t (see run/$n.build.log)"; fail=1; continue; fi
  ./bin/$n > "run/$n.log" 2>&1
  grep -E "PASS|FAIL" "run/$n.log"
  grep -q "FAIL" "run/$n.log" && fail=1
done
[ $fail = 0 ] && echo "ALL TESTS PASSED" || echo "SOME TESTS FAILED"
exit $fail
