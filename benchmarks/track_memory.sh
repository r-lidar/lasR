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

    mem_usage=$(ps -o rss= -C "$program_name")

    if [[ -z "$mem_usage" ]]; then
        echo "0"
        exit 0
    fi

    mem_usage_mb=$(bc <<< "scale=2; $mem_usage / 1024")  # Convert to MB
    max_memory_usage_mb=$(bc <<< "scale=2; $max_memory_usage / 1024")  # Convert to MB

   # Update maximum memory usage if necessary
    if (( $(bc <<< "$mem_usage > $max_memory_usage") )); then
        max_memory_usage=$mem_usage
    fi

    echo "$mem_usage_mb"

    sleep "$interval"
done


