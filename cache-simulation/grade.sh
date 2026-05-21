#!/usr/bin/env bash
# =============================================================================
# grade.sh — Cache Simulator Grading Script
# Usage: ./grade.sh [OPTIONS]
#   -s  Path to cachesim binary      (default: ./cachesim)
#   -t  Path to traces directory     (default: ./traces)
#   -o  Path to expected outputs dir (default: ./outputs)  *.trace.out
# =============================================================================

set -uo pipefail

# ── Defaults ─────────────────────────────────────────────────────────────────
CACHESIM="./cachesim"
TRACES_DIR="./traces"
OUTPUTS_DIR="./output"

# ── Colours ───────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; RESET='\033[0m'

pass() { echo -e "  ${GREEN}✓ PASS${RESET}  $1"; }
fail() { echo -e "  ${RED}✗ FAIL${RESET}  $1"; }
warn() { echo -e "  ${YELLOW}⚠ WARN${RESET}  $1"; }
info() { echo -e "  ${CYAN}→${RESET} $1"; }

# ── Argument parsing ──────────────────────────────────────────────────────────
while getopts "s:t:o:h" opt; do
  case $opt in
    s) CACHESIM="$OPTARG" ;;
    t) TRACES_DIR="$OPTARG" ;;
    o) OUTPUTS_DIR="$OPTARG" ;;
    h) sed -n '2,8p' "$0"; exit 0 ;;
    *) echo "Unknown option: -$OPTARG"; exit 1 ;;
  esac
done

# ── Config per trace ──────────────────────────────────────────────────────────
# Format: "c b s C B S k"
declare -A CFG
CFG["astar"]="12 5 3 15 6 5 2"
CFG["bzip2"]="12 5 3 15 6 5 2"
CFG["mcf"]="12 5 3 15 6 5 2"
CFG["perlbench"]="12 5 3 15 6 5 2"

# ── Validate binary ───────────────────────────────────────────────────────────
if [[ ! -x "$CACHESIM" ]]; then
  echo -e "${RED}ERROR:${RESET} cachesim not found or not executable: $CACHESIM"
  exit 1
fi

# ── Counters ──────────────────────────────────────────────────────────────────
TOTAL=0; PASSED=0; FAILED=0; SKIPPED=0
declare -a FAIL_NAMES=()

# ── Header ────────────────────────────────────────────────────────────────────
echo -e "\n${BOLD}╔══════════════════════════════════════════════╗"
echo -e "║       Cache Simulator Grading Script         ║"
echo -e "╚══════════════════════════════════════════════╝${RESET}"
echo -e "Binary  : ${CYAN}${CACHESIM}${RESET}"
echo -e "Traces  : ${CYAN}${TRACES_DIR}${RESET}"
echo -e "Expected: ${CYAN}${OUTPUTS_DIR}${RESET}"

# ── Main loop ─────────────────────────────────────────────────────────────────
for name in "${!CFG[@]}"; do
  trace_file="${TRACES_DIR}/${name}.trace"
  expected_file="${OUTPUTS_DIR}/${name}.trace.out"

  read -r c b s C B S k <<< "${CFG[$name]}"

  echo -e "\n${BOLD}── $name ──────────────────────────────────────────────────${RESET}"
  info "Config: -c $c -b $b -s $s -C $C -B $B -S $S -k $k"

  if [[ ! -f "$trace_file" ]]; then
    warn "Trace not found: $trace_file (skipping)"; ((SKIPPED++)); continue
  fi
  if [[ ! -f "$expected_file" ]]; then
    warn "Expected output not found: $expected_file (skipping)"; ((SKIPPED++)); continue
  fi

  ((TOTAL++))

  # Capture terminal output of cachesim
  actual=$("$CACHESIM" -c "$c" -b "$b" -s "$s" \
                        -C "$C" -B "$B" -S "$S" \
                        -k "$k" < "$trace_file" 2>&1) || {
    fail "$name — simulator exited with error"
    echo "$actual" | head -5 | sed 's/^/    /'
    ((FAILED++)); FAIL_NAMES+=("$name"); continue
  }

  # Normalise and compare
  norm_actual=$(echo "$actual" | sed 's/[[:space:]]*$//' | grep -v '^$')
  norm_expected=$(sed 's/[[:space:]]*$//' "$expected_file" | grep -v '^$')

  if [[ "$norm_actual" == "$norm_expected" ]]; then
    pass "$name — output matches perfectly"
    ((PASSED++))
  else
    fail "$name — output differs"
    ((FAILED++)); FAIL_NAMES+=("$name")
    echo ""
    diff_out=$(diff <(echo "$norm_expected") <(echo "$norm_actual") || true)
    echo "$diff_out" | head -40 | \
      sed "s/^-/${RED}-/; s/^+/${GREEN}+/; s/^@/${CYAN}@/; s/$/${RESET}/"
    lines=$(echo "$diff_out" | wc -l)
    (( lines > 40 )) && echo "  ... $((lines - 40)) more lines omitted"
    echo ""
  fi
done

# ── Summary ───────────────────────────────────────────────────────────────────
echo -e "${BOLD}════════════════════════════════════════════════${RESET}"
echo -e "${BOLD}Results: ${GREEN}${PASSED} passed${RESET} / ${RED}${FAILED} failed${RESET} / ${YELLOW}${SKIPPED} skipped${RESET} (${TOTAL} total)"

(( ${#FAIL_NAMES[@]} > 0 )) && echo -e "${RED}Failed: ${FAIL_NAMES[*]}${RESET}"

if   (( FAILED == 0 && SKIPPED == 0 )); then echo -e "${GREEN}${BOLD}All tests passed!${RESET}"
elif (( FAILED == 0 ));                 then echo -e "${YELLOW}All runnable tests passed (${SKIPPED} skipped).${RESET}"
else                                         echo -e "${RED}Some tests failed. Review the diffs above.${RESET}"
fi

echo ""
(( FAILED == 0 )) && exit 0 || exit 1