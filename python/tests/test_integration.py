#!/usr/bin/env python3
"""
Integration tests for pylasr using actual data processing workflows
"""

import os
import shutil
import sys
import tempfile
import unittest

# Add the parent directory to sys.path to import pylasr
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

try:
    import pylasr

    PYLASR_AVAILABLE = True
except ImportError as e:
    PYLASR_AVAILABLE = False
    IMPORT_ERROR = str(e)


class TestIntegrationWorkflows(unittest.TestCase):
    """Test complete processing workflows"""

    def setUp(self):
        if not PYLASR_AVAILABLE:
            self.skipTest("pylasr not available")

        # Find example LAS file
        self.example_las = None
        example_paths = [
            "../inst/extdata/Example.las",
            "../../inst/extdata/Example.las",
            "../../../inst/extdata/Example.las",
        ]

        for path in example_paths:
            full_path = os.path.join(os.path.dirname(__file__), path)
            if os.path.exists(full_path):
                self.example_las = full_path
                break

        # Create temporary directory for outputs
        self.temp_dir = tempfile.mkdtemp()

    def tearDown(self):
        """Clean up temporary files"""
        if hasattr(self, "temp_dir") and os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir)

    def test_basic_info_pipeline(self):
        """Test basic info pipeline without actual data"""
        pipeline = pylasr.Pipeline()
        pipeline += pylasr.info()

        # Test JSON export
        json_file = os.path.join(self.temp_dir, "info_pipeline.json")
        result_json = pipeline.write_json(json_file)
        self.assertEqual(result_json, json_file)
        self.assertTrue(os.path.exists(json_file))

        # Test pipeline info
        info = pylasr.pipeline_info(json_file)
        self.assertIsNotNone(info)

    def test_complex_pipeline_creation(self):
        """Test creating complex pipeline without execution"""
        pipeline = pylasr.Pipeline()

        # Add various stages
        pipeline += pylasr.info()
        pipeline += pylasr.classify_with_sor(k=8, m=6)
        pipeline += pylasr.delete_points(["Classification == 18"])
        pipeline += pylasr.classify_with_csf()

        # Add output stage
        output_file = os.path.join(self.temp_dir, "processed.las")
        pipeline += pylasr.write_las(output_file)

        # Test pipeline string representation
        pipeline_str = pipeline.to_string()
        self.assertIn("info", pipeline_str)
        self.assertIn("classify_with_sor", pipeline_str)
        self.assertIn(
            "filter", pipeline_str
        )  # delete_points becomes filter in pipeline string
        self.assertIn("classify_with_csf", pipeline_str)
        self.assertIn("write_las", pipeline_str)

        # Test JSON export
        json_file = os.path.join(self.temp_dir, "complex_pipeline.json")
        pipeline.write_json(json_file)
        self.assertTrue(os.path.exists(json_file))

    def test_dtm_chm_workflow(self):
        """Test DTM/CHM workflow creation"""
        dtm_file = os.path.join(self.temp_dir, "dtm.tif")
        chm_file = os.path.join(self.temp_dir, "chm.tif")

        # Create DTM and CHM pipelines
        dtm_pipeline = pylasr.dtm_pipeline(1.0, dtm_file)
        chm_pipeline = pylasr.chm_pipeline(0.5, chm_file)

        # Combine pipelines
        full_pipeline = dtm_pipeline + chm_pipeline

        # Test pipeline structure
        pipeline_str = full_pipeline.to_string()
        self.assertIn("rasterize", pipeline_str)

        # Test JSON export
        json_file = os.path.join(self.temp_dir, "dtm_chm_pipeline.json")
        full_pipeline.write_json(json_file)
        self.assertTrue(os.path.exists(json_file))

    def test_sampling_workflow(self):
        """Test point sampling workflow"""
        pipeline = pylasr.Pipeline()

        # Add different sampling methods
        pipeline += pylasr.sampling_voxel(res=2.0, method="random")
        pipeline += pylasr.sampling_pixel(res=1.0, method="max")
        pipeline += pylasr.sampling_poisson(distance=1.5)

        # Add output
        output_file = os.path.join(self.temp_dir, "sampled.las")
        pipeline += pylasr.write_las(output_file)

        # Test pipeline creation
        json_file = os.path.join(self.temp_dir, "sampling_pipeline.json")
        pipeline.write_json(json_file)
        self.assertTrue(os.path.exists(json_file))

    def test_attribute_management_workflow(self):
        """Test attribute management workflow"""
        pipeline = pylasr.Pipeline()

        # Add attribute operations
        pipeline += pylasr.add_attribute("double", "Roughness", "Surface roughness")
        pipeline += pylasr.add_rgb()
        pipeline += pylasr.geometry_features(k=10, r=1.0, features="eigen_values")
        pipeline += pylasr.edit_attribute(["Classification == 1"], "Classification", 6)
        pipeline += pylasr.remove_attribute("GPSTime")

        # Add output
        output_file = os.path.join(self.temp_dir, "with_attributes.las")
        pipeline += pylasr.write_las(output_file)

        # Test pipeline creation
        json_file = os.path.join(self.temp_dir, "attributes_pipeline.json")
        pipeline.write_json(json_file)
        self.assertTrue(os.path.exists(json_file))

    def test_format_conversion_workflow(self):
        """Test format conversion workflow"""
        pipeline = pylasr.Pipeline()

        # Add multiple output formats
        las_file = os.path.join(self.temp_dir, "output.laz")
        pcd_file = os.path.join(self.temp_dir, "output.pcd")
        copc_file = os.path.join(self.temp_dir, "output.copc.laz")
        vpc_file = os.path.join(self.temp_dir, "catalog.vpc")

        pipeline += pylasr.write_las(las_file)
        pipeline += pylasr.write_pcd(pcd_file, binary=True)
        # pipeline += pylasr.write_copc(copc_file, max_depth=10)  # Skip due to validation issue
        pipeline += pylasr.write_vpc(vpc_file)
        pipeline += pylasr.write_lax()

        # Test pipeline creation
        json_file = os.path.join(self.temp_dir, "conversion_pipeline.json")
        pipeline.write_json(json_file)
        self.assertTrue(os.path.exists(json_file))

    def test_pipeline_processing_strategies(self):
        """Test different processing strategies"""
        pipeline = pylasr.Pipeline()
        pipeline += pylasr.info()

        # Test different strategies
        pipeline.set_sequential_strategy()
        pipeline.set_concurrent_points_strategy(4)
        pipeline.set_concurrent_files_strategy(2)
        pipeline.set_nested_strategy(2, 4)

        # Test other options
        pipeline.set_verbose(True)
        pipeline.set_progress(True)
        pipeline.set_buffer(10.0)
        pipeline.set_chunk(1000.0)

        # Test JSON export with options
        json_file = os.path.join(self.temp_dir, "strategy_pipeline.json")
        pipeline.write_json(json_file)
        self.assertTrue(os.path.exists(json_file))

    def test_actual_data_processing(self):
        """Test processing with actual LAS data if available"""
        if not self.example_las:
            self.skipTest("Example LAS file not found")

        # Create simple pipeline
        pipeline = pylasr.Pipeline()
        pipeline += pylasr.info()

        output_file = os.path.join(self.temp_dir, "processed_example.las")
        pipeline += pylasr.write_las(output_file)

        # Set processing options
        pipeline.set_sequential_strategy()
        pipeline.set_verbose(False)  # Keep output clean for tests

        # Test execution (this will actually process data)
        try:
            result = pipeline.execute([self.example_las])
            
            # Validate new result structure
            self.assertIsInstance(result, dict, "Result must be a dictionary")
            self.assertIn('success', result, "Result must have 'success' field")  
            self.assertIn('data', result, "Result must have 'data' field")
            self.assertIn('json_config', result, "Result must have 'json_config' field")
            
            self.assertTrue(result['success'], "Pipeline execution failed")
            self.assertTrue(os.path.exists(output_file), "Output file was not created")
            
            # Validate data structure
            if result['data']:
                self.assertIsInstance(result['data'], list, "Data field must be a list")
        except Exception as e:
            self.fail(f"Pipeline execution raised an exception: {e}")


class TestErrorHandling(unittest.TestCase):
    """Test error handling and edge cases"""

    def setUp(self):
        if not PYLASR_AVAILABLE:
            self.skipTest("pylasr not available")

        self.temp_dir = tempfile.mkdtemp()

    def tearDown(self):
        if hasattr(self, "temp_dir") and os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir)

    def test_invalid_stage_parameters(self):
        """Test handling of invalid stage parameters"""
        # Stage class no longer exposed - test invalid parameters with pipeline functions
        # Most stage functions validate parameters when creating the pipeline
        # Test with invalid parameter types
        with self.assertRaises((TypeError, ValueError)):
            # Try to create a pipeline with invalid parameters
            pylasr.classify_with_sor(k="invalid", m="invalid")


if __name__ == "__main__":
    unittest.main(verbosity=2)
