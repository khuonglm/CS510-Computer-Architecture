# #!/bin/bash
# set -euo pipefail

# PROTOCOL_LIST=(MESI)
# PROC_LIST=("4proc")

# for PROC in "${PROC_LIST[@]}"; do
#     for PROTOCOL in "${PROTOCOL_LIST[@]}"; do
#         echo "Testing ${PROC}-${PROTOCOL}"

#         ACTUAL_OUTPUT=$(mktemp)

#         ./sim_trace -p "${PROTOCOL}" -t "traces/${PROC}_validation" \
#             > "${ACTUAL_OUTPUT}" 2>&1

#         cp "${ACTUAL_OUTPUT}" error.txt

#         if diff -Naur \
#             "traces/${PROC}_validation/${PROTOCOL}_validation.txt" \
#             "${ACTUAL_OUTPUT}" > /dev/null; then

#             echo -e "\e[32mO: Result is identical to the output\e[0m"
#         else
#             echo -e "\e[31mX: Result is not identical to the output\e[0m"
#             echo "Saved actual output to: error"
#             exit 1
#         fi

#         rm -f "${ACTUAL_OUTPUT}"
#     done
# done

#!/bin/bash
set -euo pipefail

PROTOCOL_LIST=(MI MSI MESI MOESI)
PROC_LIST=("4proc" "8proc" "16proc")

for PROC in "${PROC_LIST[@]}"; do
    for PROTOCOL in "${PROTOCOL_LIST[@]}"; do
        echo "Testing ${PROC}-${PROTOCOL}"

        ACTUAL_OUTPUT=$(mktemp)

        ./sim_trace -p "${PROTOCOL}" -t "traces/${PROC}_validation" \
            > "${ACTUAL_OUTPUT}" 2>&1

        cp "${ACTUAL_OUTPUT}" error.txt

        diff -Naur \
            "traces/${PROC}_validation/${PROTOCOL}_validation.txt" \
            "${ACTUAL_OUTPUT}"

        rm -f "${ACTUAL_OUTPUT}"

        echo -e "\e[32mO: Result is identical to the output\e[0m"
    done
done
