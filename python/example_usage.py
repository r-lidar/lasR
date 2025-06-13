#!/usr/bin/env python3
"""
Example usage of the LASR Python API using the C++ API backend.

This demonstrates the unified C++ API approach suggested in GitHub issue #155,
which ensures consistency between R and Python APIs.
"""

import pylasr
import tempfile
import os
from pathlib import Path

def example_basic_pipeline():
    """Basic pipeline example similar to the R API test function."""
    
    # Create a pipeline
    pipeline = pylasr.Pipeline()
    
    # Set processing options
    pipeline.set_ncores(4)
    pipeline.set_verbose(True)
    pipeline.set_progress(True)
    
    # Add stages to the pipeline
    pipeline += pylasr.info()
    pipeline += pylasr.delete_points(["Z > 100"])  # Remove points above 100m
    
    # Create output file in temp directory
    temp_dir = Path(tempfile.gettempdir())
    output_file = temp_dir / "filtered_output.las"
    pipeline += pylasr.write_las(str(output_file))
    
    print("Pipeline created:")
    print(pipeline.to_string())
    
    # Execute on files (replace with actual LAS file paths)
    # files = ["path/to/your/file1.las", "path/to/your/file2.las"]
    # success = pipeline.execute(files)
    # print(f"Pipeline execution {'succeeded' if success else 'failed'}")
    
    return pipeline

def example_classification_pipeline():
    """Example pipeline for noise classification and ground filtering."""
    
    # Create stages individually
    info_stage = pylasr.info()
    sor_stage = pylasr.classify_with_sor(k=10, m=8, classification=18)  # Noise classification
    
    # Create pipeline by combining stages
    pipeline = pylasr.Pipeline(info_stage)
    pipeline += sor_stage
    
    # Add more processing
    pipeline += pylasr.add_rgb()  # Add RGB if available
    
    # Set up output
    temp_dir = Path(tempfile.gettempdir())
    output_file = temp_dir / "classified_output.las"
    pipeline += pylasr.write_las(str(output_file), keep_buffer=False)
    
    # Configure pipeline options
    pipeline.set_ncores(8)
    pipeline.set_buffer(50.0)  # 50m buffer
    pipeline.set_verbose(True)
    
    return pipeline

def example_raster_operations():
    """Example showing raster-based operations."""
    
    pipeline = pylasr.Pipeline()
    
    # Load a DEM raster
    # dem_stage = pylasr.load_raster("path/to/dem.tif", band=1)
    # pipeline += dem_stage
    
    # Create a CHM (Canopy Height Model) by rasterizing points
    # Note: This would need the actual rasterize function when implemented
    
    # Apply focal operations
    # focal_stage = pylasr.focal(
    #     connect_uid=dem_stage.get_uid(),
    #     size=3.0,
    #     fun="mean",
    #     ofile="smoothed_dem.tif"
    # )
    # pipeline += focal_stage
    
    # Fill pits in the raster
    # pit_fill_stage = pylasr.pit_fill(
    #     connect_uid=focal_stage.get_uid(),
    #     ofile="filled_dem.tif"
    # )
    # pipeline += pit_fill_stage
    
    return pipeline

def example_stage_parameters():
    """Example showing how to work with stage parameters."""
    
    # Create a stage and set parameters manually
    sor_stage = pylasr.Stage("classify_with_sor")
    sor_stage.set("k", 12)
    sor_stage.set("m", 6)
    sor_stage.set("classification", 18)
    
    print(f"Stage name: {sor_stage.get_name()}")
    print(f"Stage UID: {sor_stage.get_uid()}")
    print(f"K parameter: {sor_stage.get('k')}")
    print(f"Has 'threshold' parameter: {sor_stage.has('threshold')}")
    
    # Create pipeline with this stage
    pipeline = pylasr.Pipeline(sor_stage)
    
    return pipeline

def example_system_info():
    """Example showing system information functions."""
    
    print("System Information:")
    print(f"Available threads: {pylasr.available_threads()}")
    print(f"OpenMP support: {pylasr.has_omp_support()}")
    print(f"Available RAM: {pylasr.get_available_ram() / (1024**3):.2f} GB")
    print(f"Total RAM: {pylasr.get_total_ram() / (1024**3):.2f} GB")
    
    # Display LAS filter usage
    print("\nLAS Filter Usage:")
    pylasr.las_filter_usage()
    
    print("\nLAS Transform Usage:")
    pylasr.las_transform_usage()

def example_pipeline_combination():
    """Example showing how to combine pipelines."""
    
    # Create first pipeline for preprocessing
    preprocess = pylasr.Pipeline()
    preprocess += pylasr.info()
    preprocess += pylasr.classify_with_sor()
    
    # Create second pipeline for output
    output_pipeline = pylasr.Pipeline()
    temp_dir = Path(tempfile.gettempdir())
    output_file = temp_dir / "combined_output.las"
    output_pipeline += pylasr.write_las(str(output_file))
    
    # Combine pipelines
    full_pipeline = preprocess + output_pipeline
    
    print("Combined pipeline:")
    print(full_pipeline.to_string())
    
    return full_pipeline

def main():
    """Run all examples."""
    
    print("=== LASR Python API Examples ===\n")
    
    print("1. System Information:")
    example_system_info()
    print()
    
    print("2. Basic Pipeline:")
    basic_pipeline = example_basic_pipeline()
    print()
    
    print("3. Classification Pipeline:")
    classification_pipeline = example_classification_pipeline()
    print()
    
    print("4. Stage Parameters:")
    param_pipeline = example_stage_parameters()
    print()
    
    print("5. Pipeline Combination:")
    combined_pipeline = example_pipeline_combination()
    print()
    
    # Example of writing pipeline to JSON
    json_file = Path(tempfile.gettempdir()) / "example_pipeline.json"
    json_path = basic_pipeline.write_json(str(json_file))
    print(f"Pipeline JSON written to: {json_path}")
    
    # Clean up
    if json_file.exists():
        json_file.unlink()

if __name__ == "__main__":
    main() 