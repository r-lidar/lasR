#!/usr/bin/env python3
"""
Example usage of LASR Python bindings
"""

import os
import sys
import json

# Add the python directory to the path
python_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "python"))
sys.path.insert(0, python_dir)

import pylasr

def print_system_info():
    """Print system information using LASR functions"""
    print("System Information:")
    print(f"  Available threads: {pylasr.available_threads()}")
    print(f"  Has OpenMP support 1: {pylasr.has_omp_support()}")
    
    # Convert bytes to GB for better readability
    available_ram_gb = pylasr.get_available_ram() / (1024**3)
    total_ram_gb = pylasr.get_total_ram() / (1024**3)
    print(f"  Available RAM: {available_ram_gb:.2f} GB")
    print(f"  Total RAM: {total_ram_gb:.2f} GB")
    print()

def main():
    # Print system information
    print_system_info()
    
    # Check if a config file was provided
    if len(sys.argv) < 2:
        print("Usage: python example.py <config_file.json>")
        sys.exit(1)
    
    config_file = sys.argv[1]
    
    # Check if the file exists
    if not os.path.isfile(config_file):
        print(f"Error: Config file '{config_file}' does not exist")
        sys.exit(1)
    
    # Get pipeline information
    try:
        # Check if get_pipeline_info is available in the module
        if hasattr(pylasr, 'get_pipeline_info'):
            info = pylasr.get_pipeline_info(config_file)
            print(f"Pipeline information:")
            print(json.dumps(json.loads(info), indent=2))
        else:
            print("Note: get_pipeline_info function is not available in pylasr module")
    except Exception as e:
        print(f"Error getting pipeline info: {e}")
    
    # Process the pipeline
    try:
        print(f"Processing pipeline from {config_file}...")
        result = pylasr.process(config_file)
        print(f"Pipeline processing {'successful' if result else 'failed'}")
    except Exception as e:
        print(f"Error processing pipeline: {e}")

if __name__ == "__main__":
    main() 