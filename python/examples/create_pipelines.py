#!/usr/bin/env python3
"""
Create and demonstrate various LiDAR processing pipelines

This script demonstrates how to create, configure, and execute
different types of processing pipelines directly.
"""

import pylasr


def create_info_pipeline():
    """Create a simple information pipeline"""
    pipeline = pylasr.Pipeline()
    pipeline += pylasr.info()
    
    print(f"  ✅ Info pipeline ready")
    return pipeline


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
    
    print(f"  ✅ Cleaning pipeline ready (4 cores, 10.0 buffer)")
    return pipeline


def create_terrain_pipeline():
    """Create DTM and CHM generation pipeline"""
    dtm_pipeline = pylasr.dtm_pipeline(1.0, "dtm_1m.tif")
    chm_pipeline = pylasr.chm_pipeline(0.5, "chm_50cm.tif")
    
    # Combine both workflows
    combined = dtm_pipeline + chm_pipeline
    combined.set_concurrent_files_strategy(2)
    combined.set_buffer(15.0)
    
    print(f"  ✅ Terrain analysis pipeline ready: DTM (1m) + CHM (0.5m)")
    return combined


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
    
    print(f"  ✅ Multi-stage sampling pipeline ready")
    return pipeline


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
    
    print(f"  ✅ Classification pipeline ready (nested 2x4 cores)")
    return pipeline


def main():
    print("📋 CREATING PIPELINE EXAMPLES")
    print("=" * 50)
    
    pipelines = [
        ("Info Pipeline", create_info_pipeline),
        ("Cleaning Pipeline", create_cleaning_pipeline),
        ("Terrain Analysis", create_terrain_pipeline),
        ("Point Sampling", create_sampling_pipeline),
        ("Classification", create_classification_pipeline),
    ]
    
    created_pipelines = []
    
    for name, creator in pipelines:
        try:
            print(f"\n🔧 {name}:")
            pipeline = creator()
            created_pipelines.append((name, pipeline))
        except Exception as e:
            print(f"❌ {name}: Failed - {e}")
    
    print()
    print(f"✅ Created {len(created_pipelines)} ready-to-use pipelines")
    print()
    
    print("💡 USAGE EXAMPLES:")
    print("-" * 30)
    print("Execute any pipeline directly:")
    print("  import pylasr")
    print("  pipeline = create_info_pipeline()")
    print("  success = pipeline.execute(['your_file.las'])")
    
    print()
    print("Try with your data:")
    for name, pipeline in created_pipelines:
        print(f"  # {name}")
        print(f"  success = pipeline.execute(['input.las'])")
    
    print()
    print("🚀 No JSON files needed - just create and execute!")
    print("📖 See README.md for complete documentation")


if __name__ == "__main__":
    main()