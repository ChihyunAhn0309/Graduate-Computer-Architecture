#!/bin/bash

PROTOCOL_LIST=(MI MSI MESI MOESI)
PROC_LIST=("4proc" "8proc" "16proc")
STAT_PATTERN='^(Run Time|Cache Misses|Cache Accesses|Silent Upgrades|\$-to-\$ Transfers):'

for PROC in ${PROC_LIST[@]}
do
    for PROTOCOL in ${PROTOCOL_LIST[@]}
    do
        echo "Testing ${PROC}-${PROTOCOL}"
        diff -Naur \
            <(grep -E "${STAT_PATTERN}" traces/${PROC}_validation/${PROTOCOL}_validation.txt) \
            <(./sim_trace -p ${PROTOCOL} -t traces/${PROC}_validation 2>&1 1>/dev/null | grep -E "${STAT_PATTERN}") \
            > /dev/null
        EXIT_CODE=$?
        if [[ ${EXIT_CODE} == 0 ]];
        then
            echo -e "\e[32mO: Result is identical to the output\e[0m"
        else
            echo -e "\e[31mX: Result is not identical to the output\e[0m"
        fi
    done
done
