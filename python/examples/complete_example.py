#!/usr/bin/env python3
"""
Complete example showing how to use pylasr for LiDAR processing

This example demonstrates:
- System information
- Pipeline creation and configuration
- JSON export and introspection
- Data processing (if example data is provided)
- Convenience functions
- Manual stage creation
- Multithreading comparison

Usage:
    python complete_example.py                    # Run without data processing
    python complete_example.py <path_to_las_file> # Run with data processing
"""

import os
import sys
import tempfile
import time

import pylasr


def main():
    print("🚀 PYLASR COMPLETE EXAMPLE")
    print("=" * 50)

    # Check command line arguments
    example_file = None
    if len(sys.argv) > 1:
        example_file = sys.argv[1]
        if not os.path.exists(example_file):
            print(f"❌ Error: File '{example_file}' not found")
            print("💡 Usage: python complete_example.py [path_to_las_file]")
            sys.exit(1)

    # Show system information
    print(f"Version: {pylasr.__version__}")
    print(f"Available threads: {pylasr.available_threads()}")
    print(f"OpenMP support: {pylasr.has_omp_support()}")
    print()

    # 1. Create a simple pipeline
    print("📋 CREATING PIPELINE")
    print("-" * 30)

    pipeline = pylasr.Pipeline()
    pipeline += pylasr.info()

    # Note: Aggressive filtering is commented out for the small example dataset
    # With only 30 points, SOR noise classification might remove all points
    # For larger datasets, you would typically include:
    # pipeline += pylasr.classify_with_sor(k=8, m=6)  # Classify noise
    # pipeline += pylasr.delete_points(["Classification == 18"])  # Remove noise

    # Add output
    with tempfile.NamedTemporaryFile(suffix=".las", delete=False) as f:
        output_file = f.name

    pipeline += pylasr.write_las(output_file)

    print(f"✅ Pipeline created with {len(pipeline.to_string().split(chr(10)))} stages")
    print()

    # 2. Configure processing options
    print("⚙️  CONFIGURING PROCESSING")
    print("-" * 30)

    pipeline.set_concurrent_points_strategy(4)
    pipeline.set_verbose(False)  # Keep output clean
    pipeline.set_progress(True)
    pipeline.set_buffer(10.0)

    print("✅ Processing options configured")
    print()

    # 3. Export pipeline to JSON
    print("💾 EXPORTING PIPELINE")
    print("-" * 30)

    with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as f:
        json_file = f.name

    pipeline.write_json(json_file)
    print(f"✅ Pipeline exported to: {os.path.basename(json_file)}")

    # Get pipeline info
    info = pylasr.pipeline_info(json_file)
    print(f"✅ Pipeline info - Streamable: {info.streamable}, Buffer: {info.buffer}")
    print()

    # 4. Show example with actual data (if available)
    print("📊 DATA PROCESSING EXAMPLE")
    print("-" * 30)

    if example_file:
        print(f"📂 Processing: {os.path.basename(example_file)}")
        file_size = os.path.getsize(example_file)
        print(f"   File size: {file_size:,} bytes")
        print("🔄 Processing data...")

        try:
            success = pipeline.execute([example_file])
            if success:
                print("✅ Pipeline execution successful!")
                print(f"📁 Output written to: {os.path.basename(output_file)}")

                if os.path.exists(output_file):
                    size = os.path.getsize(output_file)
                    print(f"📊 Output file size: {size:,} bytes")
            else:
                print("❌ Pipeline execution failed")
        except Exception as e:
            print(f"❌ Error during processing: {e}")
    else:
        print("📂 No input file provided")
        print("💡 To process your data:")
        print("   python complete_example.py your_file.las")
        print()
        print("💡 Example data locations:")
        print("   - ../inst/extdata/Example.las (if running from lasR repo)")
        print("   - Any .las or .laz file you have")

    print()

    # 5. Convenience function examples
    print("🛠️  CONVENIENCE FUNCTIONS")
    print("-" * 30)

    # DTM pipeline
    dtm_pipeline = pylasr.dtm_pipeline(1.0, "dtm.tif")
    print("✅ DTM pipeline created")

    # CHM pipeline
    chm_pipeline = pylasr.chm_pipeline(0.5, "chm.tif")
    print("✅ CHM pipeline created")

    # Combined workflow
    combined = dtm_pipeline + chm_pipeline
    print("✅ Combined DTM+CHM workflow")
    print(f"✅ Combined pipeline has {len(combined.to_string().split('\n'))} stages")
    print()

    # 6. Manual stage creation
    print("🔧 MANUAL STAGE CREATION")
    print("-" * 30)

    manual_stage = pylasr.Stage("classify_with_sor")
    manual_stage.set("k", 12)
    manual_stage.set("m", 8)
    manual_stage.set("classification", 18)

    print(f"✅ Manual stage: {manual_stage.get_name()}")
    print(f"   Parameters: k={manual_stage.get('k')}, m={manual_stage.get('m')}")
    print()

    # 7. Multithreading demonstration
    print("🧵 MULTITHREADING DEMONSTRATION")
    print("-" * 30)

    if example_file:
        print("Testing different threading strategies...")

        strategies = [
            ("Sequential", lambda p: p.set_sequential_strategy()),
            ("2 Threads", lambda p: p.set_concurrent_points_strategy(2)),
            ("4 Threads", lambda p: p.set_concurrent_points_strategy(4)),
        ]

        for name, config_func in strategies:
            test_pipeline = pylasr.Pipeline()
            test_pipeline += pylasr.info()

            with tempfile.NamedTemporaryFile(suffix=".las", delete=False) as f:
                test_output = f.name
            test_pipeline += pylasr.write_las(test_output)

            config_func(test_pipeline)
            test_pipeline.set_verbose(False)

            start_time = time.time()
            try:
                result = test_pipeline.execute([example_file])
                end_time = time.time()

                if result and os.path.exists(test_output):
                    print(f"✅ {name}: {end_time - start_time:.3f}s")
                    os.unlink(test_output)
                else:
                    print(f"❌ {name}: Failed")
            except Exception as e:
                print(f"❌ {name}: Error - {e}")
    else:
        print("⏭️  Skipped (no input file provided)")
        print("💡 To test multithreading:")
        print("   python complete_example.py your_file.las")

    print()

    # Clean up
    try:
        os.unlink(output_file)
        os.unlink(json_file)
    except OSError:
        pass

    print("🎉 EXAMPLE COMPLETED!")
    print("The pylasr Python API is ready for your LiDAR processing tasks.")


if __name__ == "__main__":
    main()
