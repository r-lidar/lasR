#!/bin/bash

program_name="R"      # Get program name from command line argument
interval=${1:-2}      # Interval in seconds (default: 2 seconds)
max_memory_usage=0    # Variable to store maximum memory usage

if [[ -z "$program_name" ]]; then
    echo "Please provide the program name as a parameter."
    exit 1
fi

echo "0"

while true; do

     total_mem_usage=0  # Total memory usage for all instances

    # Loop through each instance of the program
    for pid in $(pgrep -d ' ' -x "$program_name"); do
        mem_usage=$(ps -o rss= -p "$pid")
        if [[ -n "$mem_usage" ]]; then
            total_mem_usage=$((total_mem_usage + mem_usage))
        fi
    done

    if [[ "$total_mem_usage" -eq 0 ]]; then
        echo "0"
        exit 0
    fi

    total_mem_usage_mb=$(bc <<< "scale=2; $total_mem_usage / 1024")  # Convert to MB
    max_memory_usage_mb=$(bc <<< "scale=2; $max_memory_usage / 1024")  # Convert to MB

   # Update maximum memory usage if necessary
    if (( $(bc <<< "$total_mem_usage_mb > $max_memory_usage") )); then
        max_memory_usage=$total_mem_usage_mb
    fi

    echo "$total_mem_usage_mb"

    sleep "$interval"
done


