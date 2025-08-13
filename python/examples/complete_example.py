#!/usr/bin/env python3
"""
Complete example showing how to use pylasr for LiDAR processing

This example demonstrates:
- System information
- Pipeline creation and configuration
- Pipeline introspection and direct execution
- Data processing (if example data is provided)
- Convenience functions
- Manual stage creation
- Multithreading comparison

Usage:
    python complete_example.py                    # Run without data processing
    python complete_example.py <path_to_las_or_dir> # Run with data processing (file or directory)
"""

import os
import sys
import tempfile
import time
from pathlib import Path

import pylasr


def main():
    print("🚀 PYLASR COMPLETE EXAMPLE")
    print("=" * 50)

    # Check command line arguments
    example_path = None
    if len(sys.argv) > 1:
        example_path = sys.argv[1]
        if not os.path.exists(example_path):
            print(f"❌ Error: Path '{example_path}' not found")
            print("💡 Usage: python complete_example.py [path_to_las_or_dir]")
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

    # 3. Pipeline introspection
    print("💾 PIPELINE INTROSPECTION")
    print("-" * 30)

    # Check pipeline properties directly
    print(f"✅ Has reader: {pipeline.has_reader()}")
    print(f"✅ Has catalog: {pipeline.has_catalog()}")
    print(f"✅ Pipeline ready for execution")
    print()

    # 4. Show example with actual data (if available)
    print("📊 DATA PROCESSING EXAMPLE")
    print("-" * 30)

    if example_path:
        target = Path(example_path)
        print(f"📂 Processing: {target}")
        if target.is_file():
            try:
                file_size = os.path.getsize(target)
                print(f"   File size: {file_size:,} bytes")
            except OSError:
                pass
        print("🔄 Processing data...")

        try:
            # New: accept dir, file, Path, iterable
            if target.is_dir():
                result = pipeline.execute(target)
            else:
                # Build mixed iterable demo
                result = pipeline.execute([target, target.parent])

            if result['success']:
                print("✅ Pipeline execution successful!")
                print(f"📁 Output written to: {os.path.basename(output_file)}")

                if os.path.exists(output_file):
                    size = os.path.getsize(output_file)
                    print(f"📊 Output file size: {size:,} bytes")
                
                if result['data']:
                    print(f"📊 Processing stages completed: {len(result['data'])}")
            else:
                print("❌ Pipeline execution failed")
                print(f"Error: {result.get('message', 'Unknown error')}")
        except Exception as e:
            print(f"❌ Error during processing: {e}")
    else:
        print("📂 No input path provided")
        print("💡 To process your data:")
        print("   python complete_example.py /path/to/las_or_dir")

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

    manual_stage = pylasr.classify_with_sor(k=12, m=8, classification=18)

    print(f"✅ Manual stage created using convenience function")
    print(f"   Stage type: {type(manual_stage).__name__}")
    print(f"   Stage can be added to pipelines")
    print()

    # 7. Multithreading demonstration
    print("🧵 MULTITHREADING DEMONSTRATION")
    print("-" * 30)

    if example_path:
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
                # New: pass Path directly
                result = test_pipeline.execute(target if target.is_dir() else [target])
                end_time = time.time()

                if result and os.path.exists(test_output):
                    print(f"✅ {name}: {end_time - start_time:.3f}s")
                    os.unlink(test_output)
                else:
                    print(f"❌ {name}: Failed")
            except Exception as e:
                print(f"❌ {name}: Error - {e}")
    else:
        print("⏭️  Skipped (no input path provided)")
        print("💡 To test multithreading:")
        print("   python complete_example.py /path/to/las_or_dir")

    print()

    # Clean up
    try:
        os.unlink(output_file)
    except OSError:
        pass

    print("🎉 EXAMPLE COMPLETED!")
    print("The pylasr Python API is ready for your LiDAR processing tasks.")


if __name__ == "__main__":
    main()
