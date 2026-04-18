#!/bin/bash
# Eculidim REPL Integration Tests

cd "$(dirname "$0")"
EC="./Eculidim.exe"
PASS=0 FAIL=0

run() {
    local label="$1"; shift
    local expected="$1"; shift
    local out
    if [[ "$*" == *"--"* ]] || [[ "$*" == *"./"* ]]; then
        # single command (no stdin piping)
        out=$("$@" 2>&1)
    else
        out=$(printf '%s\n' "$@" | "$EC" 2>&1)
    fi
    if echo "$out" | grep -qF "$expected"; then
        echo "PASS: $label"
        ((PASS++))
    else
        echo "FAIL: $label"
        echo "  Expected to contain: $expected"
        echo "  Got: $out"
        ((FAIL++))
    fi
}

echo "=== Eculidim REPL Integration Tests ==="
echo ""

# Normal mode
run "normal: x^2+1" "{x}^{2} + 1" 'x^2+1' 'exit'
run "normal: sin(pi/2)" "1" '\sin(\pi)' 'exit'
run "normal: 1+2*3" "" '1+2*3' 'exit'

# Differentiation
run "diff: x^2" "\\frac{d" '\diff' 'x^2' 'exit'
run "diff: sin(x)" "\\frac{d" '\diff' '\sin(x)' 'exit'

# Integration
run "int: x^2" "C" '\int' 'x^2' 'exit'
run "int: sin(x)" "C" '\int' '\sin(x)' 'exit'

# Def
run "def: define x=5" "x = 5" '\def x = 5' 'exit'
run "def: use x after def" "10" '\def x = 3' 'x^2+1' 'exit'

# JSON single mode
run "json single: 2+2" '"result":' ./Eculidim.exe --json '2+2'

# Help
run "help" "Eculidim" '\help' 'exit'

# Ans
run "ans: previous result" "" '2+2' '\ans' 'exit'

echo ""
echo "=== 结果: $PASS 通过, $FAIL 失败 ==="
exit $FAIL
