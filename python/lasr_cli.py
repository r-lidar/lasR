#!/usr/bin/env python3
import sys
import pylasr
import json


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <config_file.json>")
        sys.exit(1)

    config_file = sys.argv[1]

    # Print basic information about the pipeline
    try:
        info_json = pylasr.get_pipeline_info(config_file)
        info = json.loads(info_json)
        print("Pipeline Information:")
        print(f"  Streamable: {info['streamable']}")
        print(f"  Requires point data: {info['read_points']}")
        print(f"  Buffer needed: {info['buffer']} units")
        print(f"  Parallelizable: {info['parallelizable']}")
        print(f"  Internally parallelized: {info['parallelized']}")
        print(f"  Uses R API: {info['R_API']}")
        print()
    except Exception as e:
        print(f"Error getting pipeline info: {e}")

    # Process the pipeline
    print(f"Processing pipeline from {config_file}...")
    result = pylasr.process(config_file)

    if result:
        print("Pipeline processing successful")
        sys.exit(0)
    else:
        print("Pipeline processing failed")
        sys.exit(1)


if __name__ == "__main__":
    main()
