#!/bin/bash
make clean > /dev/null 2>&1
make > /dev/null 2>&1
./cachesim < traces/perlbench.trace > /dev/null
echo "Exit code: $?"
