#!/bin/bash

computer_name=$(hostname)

# Define parameters
p1_options=("lidR" "lasR")
p2_options=(1 2 3 4)

# If no parameters are provided, loop over all possible combinations
if [ $# -eq 0 ]; then
    for p1 in "${p1_options[@]}"; do
        for p2 in "${p2_options[@]}"; do
            ./benchmark.R "$p1" "$p2" > /dev/tty &
            ./track_memory.sh 2 > "../inst/extdata/benchmarks/${p1}_test_${p2}_${computer_name}.data" &
            wait
        done
    done
else
    # If parameters are provided, use them directly
    ./benchmark.R "$1" "$2" > /dev/tty &
    ./track_memory.sh 2 > "../inst/extdata/benchmarks/${1}_test_${2}_${computer_name}.data" &
    wait
fi

