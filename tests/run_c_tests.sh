#!/bin/bash

. ./tests/colors.sh

result=0

for test in http buffer
do
    ./tests/bin/${test}_test || result=1
done

if [ "$result" != "1" ]; then
    echo "====== FINAL RESULT: ${GREEN}PASS${RESET} ======"
else
    echo "====== FINAL RESULT: ${RED}FAIL${RESET} ======"
fi

exit $result
