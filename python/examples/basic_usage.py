#!/usr/bin/env python3
"""
Basic usage examples for pylasr

This script demonstrates the most common use cases for LiDAR processing
with pylasr in a simple, easy-to-understand format.

Usage:
    python basic_usage.py                    # Run without data processing
    python basic_usage.py <path_to_las_file> # Run with data processing
"""

import os
import sys

import pylasr


def main():
    print("🚀 PYLASR BASIC USAGE EXAMPLES")
    print("=" * 50)

    # Check command line arguments
    example_file = None
    if len(sys.argv) > 1:
        example_file = sys.argv[1]
        if not os.path.exists(example_file):
            print(f"❌ Error: File '{example_file}' not found")
            print("💡 Usage: python basic_usage.py [path_to_las_file]")
            sys.exit(1)

    # System information
    print(f"Version: {pylasr.__version__}")
    print(f"Available threads: {pylasr.available_threads()}")
    print(f"OpenMP support: {pylasr.has_omp_support()}")
    print()

    # Example 1: Simple information pipeline
    print("📋 Example 1: Basic Info Pipeline")
    print("-" * 40)

    info_pipeline = pylasr.info()
    print("✅ Info pipeline created")
    print(f"   Pipeline type: {type(info_pipeline).__name__}")
    print()

    # Example 2: Classification and cleaning
    print("🧹 Example 2: Classification and Cleaning")
    print("-" * 40)

    cleaning_pipeline = pylasr.Pipeline()
    # Note: For demonstration, we'll create a basic cleaning pipeline
    # Real noise filtering would include:
    # cleaning_pipeline += pylasr.classify_with_sor(k=10, m=6)
    # cleaning_pipeline += pylasr.delete_points(["Classification == 18"])
    cleaning_pipeline += pylasr.info()  # Show info instead for small example
    cleaning_pipeline += pylasr.write_las("cleaned.las")

    print("✅ Cleaning pipeline created")
    print(f"   Number of stages: {len(cleaning_pipeline.to_string().split('\\n'))}")
    print()

    # Example 3: DTM/CHM workflow
    print("🗻 Example 3: DTM and CHM Generation")
    print("-" * 40)

    dtm_pipeline = pylasr.dtm_pipeline(1.0, "dtm.tif")
    chm_pipeline = pylasr.chm_pipeline(0.5, "chm.tif")

    # Combine workflows
    terrain_pipeline = dtm_pipeline + chm_pipeline

    print("✅ DTM pipeline created (1m resolution)")
    print("✅ CHM pipeline created (0.5m resolution)")
    print("✅ Combined terrain analysis workflow")
    print()

    # Example 4: Manual stage creation
    print("🔧 Example 4: Manual Stage Creation")
    print("-" * 40)

    # Create stage using convenience function instead
    sor_stage = pylasr.classify_with_sor(k=12, m=8, classification=18)

    print(f"✅ Manual stage created using convenience function")
    print(f"   Stage type: {type(sor_stage).__name__}")
    print(f"   Can be added to pipelines: {hasattr(sor_stage, 'get_name')}")
    print()

    # Example 5: Pipeline configuration
    print("⚙️ Example 5: Processing Configuration")
    print("-" * 40)

    pipeline = pylasr.Pipeline()
    pipeline += pylasr.info()

    # Configure processing
    pipeline.set_concurrent_points_strategy(4)  # Use 4 threads
    pipeline.set_verbose(False)  # Quiet output
    pipeline.set_progress(True)  # Show progress
    pipeline.set_buffer(10.0)  # 10 unit buffer

    print("✅ Processing strategy: 4 concurrent threads")
    print("✅ Progress display enabled")
    print("✅ Buffer set to 10.0 units")
    print()

    # Example 6: Pipeline introspection
    print("💾 Example 6: Pipeline Introspection")
    print("-" * 40)

    # Check pipeline properties directly
    print(f"✅ Has reader: {pipeline.has_reader()}")
    print(f"✅ Has catalog: {pipeline.has_catalog()}")
    print(f"✅ Pipeline ready to execute")
    print()

    # Example 7: Data processing (if data provided)
    print("📊 Example 7: Data Processing")
    print("-" * 40)

    if example_file:
        print(f"📂 Processing: {os.path.basename(example_file)}")
        file_size = os.path.getsize(example_file)
        print(f"   File size: {file_size:,} bytes")

        # Simple processing pipeline
        simple_pipeline = pylasr.Pipeline()
        simple_pipeline += pylasr.info()
        simple_pipeline += pylasr.write_las("processed_output.las")

        try:
            # Execute using the cleanest method: pipeline.execute(files)
            result = simple_pipeline.execute(example_file)
            if result['success']:
                print("✅ Processing successful!")
                if os.path.exists("processed_output.las"):
                    output_size = os.path.getsize("processed_output.las")
                    print(f"📁 Output: processed_output.las ({output_size:,} bytes)")
                if result['data']:
                    print(f"📊 Stage results available: {len(result['data'])}")
            else:
                print("❌ Processing failed")
                print(f"Error: {result.get('message', 'Unknown error')}")
        except Exception as e:
            print(f"❌ Error: {e}")
    else:
        print("📂 No input file provided")
        print("💡 To process your data:")
        print("   python basic_usage.py your_file.las")
        print()
        print("💡 Example data locations:")
        print("   - ../inst/extdata/Example.las (if running from lasR repo)")
        print("   - Any .las or .laz file you have")

    print()

    # Cleanup
    try:
        if os.path.exists("processed_output.las"):
            os.unlink("processed_output.las")
    except OSError:
        pass

    print("🎉 BASIC EXAMPLES COMPLETED!")
    print()
    print("Next steps:")
    print("- See complete_example.py for advanced features")
    print("- See create_pipelines.py for more pipeline examples")
    print("- Check the README.md for full documentation")


if __name__ == "__main__":
    main()
