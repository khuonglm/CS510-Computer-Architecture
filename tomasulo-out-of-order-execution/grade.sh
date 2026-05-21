#!/bin/bash

# compare.sh - Compare procsim output against expected output files
# Usage: ./compare.sh [traces_dir] [output_dir] [procsim_path]

TRACES_DIR="${1:-traces}"
OUTPUT_DIR="${2:-output}"
PROCSIM="${3:-./procsim}"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

PASS=0
FAIL=0
ERRORS=0

echo -e "${CYAN}========================================${NC}"
echo -e "${CYAN}        procsim Output Comparator       ${NC}"
echo -e "${CYAN}========================================${NC}"
echo ""

# Sanity checks
if [ ! -x "$PROCSIM" ]; then
    echo -e "${RED}ERROR: '$PROCSIM' not found or not executable.${NC}"
    exit 1
fi
if [ ! -d "$TRACES_DIR" ]; then
    echo -e "${RED}ERROR: Traces directory '$TRACES_DIR' not found.${NC}"
    exit 1
fi
if [ ! -d "$OUTPUT_DIR" ]; then
    echo -e "${RED}ERROR: Expected output directory '$OUTPUT_DIR' not found.${NC}"
    exit 1
fi

# Find all .100k.trace files
TRACE_FILES=("$TRACES_DIR"/*.100k.trace)
if [ ${#TRACE_FILES[@]} -eq 0 ] || [ ! -f "${TRACE_FILES[0]}" ]; then
    echo -e "${YELLOW}WARNING: No *.100k.trace files found in '$TRACES_DIR'.${NC}"
    exit 1
fi

for TRACE_FILE in "${TRACE_FILES[@]}"; do
    BASENAME=$(basename "$TRACE_FILE" .100k.trace)
    EXPECTED_FILE="$OUTPUT_DIR/${BASENAME}.output"

    printf "%-30s" "$BASENAME"

    if [ ! -f "$EXPECTED_FILE" ]; then
        echo -e " ${YELLOW}SKIP${NC}  (no expected output: $EXPECTED_FILE)"
        continue
    fi

    # Run procsim
    ACTUAL_TMP=$(mktemp)
    "$PROCSIM" < "$TRACE_FILE" > "$ACTUAL_TMP" 2>/dev/null
    EXIT_CODE=$?

    if [ $EXIT_CODE -ne 0 ]; then
        echo -e " ${RED}ERROR${NC}  (procsim exited with code $EXIT_CODE)"
        ((ERRORS++))
        rm -f "$ACTUAL_TMP"
        continue
    fi

    # Compare
    if diff -q "$EXPECTED_FILE" "$ACTUAL_TMP" > /dev/null 2>&1; then
        echo -e " ${GREEN}PASS${NC}"
        ((PASS++))
        rm -f "$ACTUAL_TMP"
        continue
    fi

    echo -e " ${RED}FAIL${NC}"
    ((FAIL++))

    # Stats
    EXPECTED_LINE_COUNT=$(wc -l < "$EXPECTED_FILE")
    ACTUAL_LINE_COUNT=$(wc -l < "$ACTUAL_TMP")
    DIFF_COUNT=$(diff "$EXPECTED_FILE" "$ACTUAL_TMP" | grep -c '^[<>]')

    echo ""
    printf "  %-28s %s\n" "Expected lines:"           "$EXPECTED_LINE_COUNT"
    printf "  %-28s %s\n" "Actual lines:"             "$ACTUAL_LINE_COUNT"
    printf "  %-28s %s\n" "Differing lines:"          "$DIFF_COUNT"

    # Find first differing line number
    FIRST_DIFF_LINE=$(diff "$EXPECTED_FILE" "$ACTUAL_TMP" | grep -oP '^\d+' | head -1)
    printf "  %-28s %s\n" "First difference at line:" "${FIRST_DIFF_LINE:-unknown}"
    echo ""

    # Side-by-side view around first difference
    if [ -n "$FIRST_DIFF_LINE" ]; then
        START=$(( FIRST_DIFF_LINE > 3 ? FIRST_DIFF_LINE - 3 : 1 ))
        END=$(( FIRST_DIFF_LINE + 6 ))

        echo -e "  ${BOLD}${CYAN}Side-by-side (lines $START-$END):${NC}"
        printf "  ${BOLD}%-7s  %-42s  %-42s${NC}\n" "LINE" "EXPECTED" "ACTUAL"
        printf "  %-7s  %-42s  %-42s\n" "-------" "------------------------------------------" "------------------------------------------"

        for (( LN=START; LN<=END; LN++ )); do
            EXP_LINE=$(sed -n "${LN}p" "$EXPECTED_FILE")
            ACT_LINE=$(sed -n "${LN}p" "$ACTUAL_TMP")

            if [ "$EXP_LINE" = "$ACT_LINE" ]; then
                printf "  %-7s  %-42s  %-42s\n" "$LN" "${EXP_LINE:0:42}" "${ACT_LINE:0:42}"
            else
                printf "  ${RED}%-7s  %-42s  %-42s${NC}\n" "*$LN" "${EXP_LINE:0:42}" "${ACT_LINE:0:42}"
            fi
        done
    fi

    echo ""
    rm -f "$ACTUAL_TMP"
done

echo ""
echo -e "${CYAN}========================================${NC}"
echo -e "  PASS:   ${GREEN}$PASS${NC}"
echo -e "  FAIL:   ${RED}$FAIL${NC}"
echo -e "  ERRORS: ${YELLOW}$ERRORS${NC}"
echo -e "  TOTAL:  $((PASS + FAIL + ERRORS))"
echo -e "${CYAN}========================================${NC}"

[ $((FAIL + ERRORS)) -eq 0 ] && exit 0 || exit 1