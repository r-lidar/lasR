#!/usr/bin/env python3
"""
Create pipeline JSON files for various LiDAR processing workflows

This script demonstrates how to create, configure, and export
different types of processing pipelines to JSON format.
"""

import pylasr


def create_info_pipeline():
    """Create a simple information pipeline"""
    pipeline = pylasr.Pipeline()
    pipeline += pylasr.info()
    
    json_file = pipeline.write_json("info_pipeline.json")
    return json_file


def create_cleaning_pipeline():
    """Create a noise removal and cleaning pipeline"""
    pipeline = pylasr.Pipeline()
    pipeline += pylasr.info()
    pipeline += pylasr.classify_with_sor(k=10, m=6)
    pipeline += pylasr.delete_points(["Classification == 18"])
    pipeline += pylasr.write_las("cleaned_output.las")
    
    # Configure processing
    pipeline.set_concurrent_points_strategy(4)
    pipeline.set_verbose(False)
    pipeline.set_buffer(10.0)
    
    json_file = pipeline.write_json("cleaning_pipeline.json")
    return json_file


def create_terrain_pipeline():
    """Create DTM and CHM generation pipeline"""
    dtm_pipeline = pylasr.dtm_pipeline(1.0, "dtm_1m.tif")
    chm_pipeline = pylasr.chm_pipeline(0.5, "chm_50cm.tif")
    
    # Combine both workflows
    combined = dtm_pipeline + chm_pipeline
    combined.set_concurrent_files_strategy(2)
    combined.set_buffer(15.0)
    
    json_file = combined.write_json("terrain_pipeline.json")
    return json_file


def create_sampling_pipeline():
    """Create point sampling and thinning pipeline"""
    pipeline = pylasr.Pipeline()
    
    # Apply different sampling methods
    pipeline += pylasr.sampling_voxel(res=2.0, method="random")
    pipeline += pylasr.sampling_pixel(res=1.0, method="max")
    pipeline += pylasr.sampling_poisson(distance=1.5)
    pipeline += pylasr.write_las("sampled_output.las")
    
    # Configure for efficiency
    pipeline.set_sequential_strategy()
    pipeline.set_progress(True)
    
    json_file = pipeline.write_json("sampling_pipeline.json")
    return json_file


def create_classification_pipeline():
    """Create ground classification and filtering pipeline"""
    pipeline = pylasr.Pipeline()
    
    # Ground classification workflow
    pipeline += pylasr.classify_with_csf(cloth_resolution=0.5, iterations=500)
    pipeline += pylasr.classify_with_sor(k=8, m=6)
    pipeline += pylasr.delete_points(["Classification == 18"])  # Remove noise
    
    # Generate outputs
    pipeline += pylasr.write_las("classified_output.las")
    
    # Set processing options
    pipeline.set_nested_strategy(2, 4)
    pipeline.set_verbose(True)
    
    json_file = pipeline.write_json("classification_pipeline.json")
    return json_file


def main():
    print("📋 CREATING PIPELINE JSON FILES")
    print("=" * 50)
    
    pipelines = [
        ("Info Pipeline", create_info_pipeline),
        ("Cleaning Pipeline", create_cleaning_pipeline),
        ("Terrain Analysis", create_terrain_pipeline),
        ("Point Sampling", create_sampling_pipeline),
        ("Classification", create_classification_pipeline),
    ]
    
    created_files = []
    
    for name, creator in pipelines:
        try:
            json_file = creator()
            created_files.append(json_file)
            print(f"✅ {name}: {json_file}")
        except Exception as e:
            print(f"❌ {name}: Failed - {e}")
    
    print()
    print(f"📁 Created {len(created_files)} pipeline files")
    print()
    
    print("💡 USAGE EXAMPLES:")
    print("-" * 30)
    print("Inspect pipelines:")
    for file in created_files:
        print(f"  python examples/lasr_cli.py {file}")
    
    print()
    print("Process data programmatically:")
    print("  import pylasr")
    print("  info = pylasr.pipeline_info('info_pipeline.json')")
    print("  # Note: Use Pipeline.execute() method for actual processing")
    
    print()
    print("📖 See README.md for complete documentation")


if __name__ == "__main__":
    main()