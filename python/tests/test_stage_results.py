#!/usr/bin/env python3
"""
Comprehensive Test Suite for lasR Python API

This unified test suite validates:
1. All stage types return correct results
2. Proper error handling for various failure scenarios
3. JSON serialization and API consistency

Run with: python test_comprehensive_suite.py
"""

import pylasr
import tempfile
import os
import json
import unittest
from pathlib import Path
from typing import Dict, Any, List, Optional, Union


class LasRComprehensiveTestSuite(unittest.TestCase):
    """Comprehensive test suite for lasR Python API."""
    
    @classmethod
    def setUpClass(cls):
        """Set up test data and utilities."""
        cls.test_file = "../inst/extdata/Example.laz"
        cls.temp_dirs = []
        
        # Verify test file exists
        if not os.path.exists(cls.test_file):
            raise unittest.SkipTest(f"Test file not found: {cls.test_file}")
    
    @classmethod
    def tearDownClass(cls):
        """Clean up temporary directories."""
        for temp_dir in cls.temp_dirs:
            try:
                import shutil
                if os.path.exists(temp_dir):
                    shutil.rmtree(temp_dir)
            except:
                pass
    
    def create_temp_output(self, suffix: str = ".tif") -> str:
        """Create a temporary output file path."""
        temp_dir = tempfile.mkdtemp()
        self.temp_dirs.append(temp_dir)
        return os.path.join(temp_dir, f"test_output{suffix}")
    
    def validate_success_structure(self, results: Any, expected_stages: List[str] = None) -> Dict[str, Any]:
        """Validate successful result structure and return stage data."""
        # Must be a dictionary
        self.assertIsInstance(results, dict, "Results must be a dictionary")
        
        # Must have required fields
        self.assertIn('success', results, "Results must have 'success' field")
        self.assertIn('data', results, "Results must have 'data' field")
        self.assertIn('json_config', results, "Results must have 'json_config' field")
        
        # Success must be True
        self.assertTrue(results['success'], "Success field must be True")
        
        # Success results should NOT have redundant message field  
        self.assertNotIn('message', results, "Success results should not have redundant 'message' field")
        
        # Data must be a list
        self.assertIsInstance(results['data'], list, "Data field must be a list")
        
        # Extract all stages from data list
        all_stages = {}
        for data_item in results['data']:
            self.assertIsInstance(data_item, dict, "Each data item must be a dictionary")
            all_stages.update(data_item)
        
        # Validate expected stages if provided
        if expected_stages:
            for stage in expected_stages:
                self.assertIn(stage, all_stages, f"Expected stage '{stage}' not found in results")
        
        return all_stages
    
    def validate_error_structure(self, results: Any, should_have_message: bool = True) -> None:
        """Validate error result structure."""
        # Must be a dictionary (not None!)
        self.assertIsInstance(results, dict, "Error results must be a dictionary, not None")
        
        # Must have required fields
        self.assertIn('success', results, "Error results must have 'success' field")
        self.assertEqual(results['success'], False, "Error results must have success=False")
        
        # Should have error message
        if should_have_message:
            self.assertIn('message', results, "Error results should have 'message' field")
            self.assertIsInstance(results['message'], str, "Error message must be a string")
            self.assertGreater(len(results['message']), 0, "Error message must not be empty")
        
        # Data should be None for errors
        self.assertIsNone(results.get('data'), "Error results should have data=None")
    
    # =========================================================================
    # DATA-PRODUCING STAGES TESTS
    # =========================================================================
    
    def test_summary_stage_complete_data(self):
        """Test summary stage returns complete statistical data."""
        pipeline = pylasr.Pipeline()
        pipeline += pylasr.summarise()
        results = pipeline.execute([self.test_file])
        
        stage_data = self.validate_success_structure(results, ['summary'])
        
        summary = stage_data['summary']
        self.assertIsInstance(summary, dict, "Summary data must be a dictionary")
        
        # Validate required statistical fields
        required_fields = ['npoints', 'nsingle', 'crs', 'z_histogram', 'i_histogram']
        for field in required_fields:
            self.assertIn(field, summary, f"Summary missing required field: {field}")
        
        # Validate data types and values
        self.assertIsInstance(summary['npoints'], int, "npoints must be integer")
        self.assertGreater(summary['npoints'], 0, "npoints must be positive")
        
        self.assertIsInstance(summary['crs'], dict, "CRS must be dictionary")
        self.assertIn('epsg', summary['crs'], "CRS must have EPSG code")
        self.assertIn('wkt', summary['crs'], "CRS must have WKT string")
        
        self.assertIsInstance(summary['z_histogram'], list, "Z histogram must be list")
        self.assertIsInstance(summary['i_histogram'], list, "I histogram must be list")
    
    def test_local_maximum_stage(self):
        """Test local maximum detection stage."""
        pipeline = pylasr.Pipeline()
        pipeline += pylasr.local_maximum(ws=3)
        results = pipeline.execute([self.test_file])
        
        stage_data = self.validate_success_structure(results)
        
        # Local maximum might not find any maxima, which is valid
        if 'local_maximum' in stage_data:
            maxima = stage_data['local_maximum']
            self.assertIsInstance(maxima, (dict, list), "Local maximum data must be dict or list")
    
    def test_geometry_features_stage(self):
        """Test geometry features computation stage."""  
        pipeline = pylasr.Pipeline()
        pipeline += pylasr.geometry_features(k=8, r=1.0)
        results = pipeline.execute([self.test_file])
        
        stage_data = self.validate_success_structure(results)
        
        # Geometry features might not produce output depending on implementation
        if 'geometry_features' in stage_data:
            features = stage_data['geometry_features']
            self.assertIsInstance(features, (dict, list), "Geometry features data must be dict or list")
    
    # =========================================================================
    # FILE-PRODUCING STAGES TESTS
    # =========================================================================
    
    def test_rasterize_stage_file_output(self):
        """Test rasterize stage creates output files."""
        output_file = self.create_temp_output(".tif")
        
        pipeline = pylasr.Pipeline()
        pipeline += pylasr.rasterize(1.0, 1.0, ofile=output_file)
        results = pipeline.execute([self.test_file])
        
        stage_data = self.validate_success_structure(results, ['rasterize'])
        
        raster_result = stage_data['rasterize']
        
        # Should return file path(s)
        if isinstance(raster_result, str):
            self.assertTrue(os.path.exists(raster_result), f"Raster file should exist: {raster_result}")
        elif isinstance(raster_result, list):
            self.assertGreater(len(raster_result), 0, "Raster result list should not be empty")
            for file_path in raster_result:
                self.assertIsInstance(file_path, str, "Each raster result should be a string path")
                self.assertTrue(os.path.exists(file_path), f"Raster file should exist: {file_path}")
        else:
            self.fail(f"Raster result should be string or list, got {type(raster_result)}")
    
    def test_write_las_stage_file_output(self):
        """Test write_las stage creates output files."""
        output_file = self.create_temp_output(".las")
        
        pipeline = pylasr.Pipeline()
        pipeline += pylasr.write_las(ofile=output_file)
        results = pipeline.execute([self.test_file])
        
        stage_data = self.validate_success_structure(results, ['write_las'])
        
        las_result = stage_data['write_las']
        
        # Should return file path(s)
        if isinstance(las_result, str):
            self.assertTrue(os.path.exists(las_result), f"LAS file should exist: {las_result}")
        elif isinstance(las_result, list):
            self.assertGreater(len(las_result), 0, "LAS result list should not be empty")
            for file_path in las_result:
                self.assertTrue(os.path.exists(file_path), f"LAS file should exist: {file_path}")
        else:
            self.fail(f"LAS result should be string or list, got {type(las_result)}")
    
    def test_write_pcd_stage_file_output(self):
        """Test write_pcd stage creates output files."""
        output_file = self.create_temp_output(".pcd")
        
        pipeline = pylasr.Pipeline()
        pipeline += pylasr.write_pcd(ofile=output_file)
        results = pipeline.execute([self.test_file])
        
        stage_data = self.validate_success_structure(results, ['write_pcd'])
        
        pcd_result = stage_data['write_pcd']
        
        # Should return file path(s)
        if isinstance(pcd_result, str):
            self.assertTrue(os.path.exists(pcd_result), f"PCD file should exist: {pcd_result}")
        elif isinstance(pcd_result, list):
            for file_path in pcd_result:
                self.assertTrue(os.path.exists(file_path), f"PCD file should exist: {file_path}")
        else:
            self.fail(f"PCD result should be string or list, got {type(pcd_result)}")
    
    # =========================================================================
    # PROCESSING STAGES TESTS (NO OUTPUT)
    # =========================================================================
    
    def test_processing_stages_no_output(self):
        """Test processing stages that don't produce direct output."""
        test_cases = [
            ('classify_with_csf', pylasr.classify_with_csf()),
            ('sort_points', pylasr.sort_points()),
            ('classify_with_sor', pylasr.classify_with_sor(k=8, m=2)),
        ]
        
        for stage_name, stage in test_cases:
            with self.subTest(stage=stage_name):
                pipeline = pylasr.Pipeline()
                pipeline += stage
                results = pipeline.execute([self.test_file])
                
                stage_data = self.validate_success_structure(results)
                
                # Processing stages typically don't appear in results
                # This is expected behavior - they modify data but don't produce output
    
    # =========================================================================
    # MULTI-STAGE PIPELINE TESTS
    # =========================================================================
    
    def test_multi_stage_pipeline_comprehensive(self):
        """Test complex multi-stage pipeline with different stage types."""
        raster_output = self.create_temp_output(".tif")
        las_output = self.create_temp_output(".las")
        
        pipeline = pylasr.Pipeline()
        pipeline += pylasr.classify_with_csf()           # Processing stage
        pipeline += pylasr.summarise()                   # Data-producing stage
        pipeline += pylasr.rasterize(1.0, 1.0, ofile=raster_output)  # File-producing stage
        pipeline += pylasr.write_las(ofile=las_output)   # File-producing stage
        
        results = pipeline.execute([self.test_file])
        
        stage_data = self.validate_success_structure(results)
        
        # Should have data-producing and file-producing stages
        expected_stages = ['summary', 'rasterize', 'write_las']
        for stage in expected_stages:
            self.assertIn(stage, stage_data, f"Multi-stage pipeline missing: {stage}")
        
        # Validate each stage's output
        self.assertIsInstance(stage_data['summary'], dict, "Summary should be dict")
        self.assertIn('npoints', stage_data['summary'], "Summary should have npoints")
        
        # Validate file outputs exist
        raster_files = stage_data['rasterize']
        if isinstance(raster_files, str):
            raster_files = [raster_files]
        for file_path in raster_files:
            self.assertTrue(os.path.exists(file_path), f"Raster output should exist: {file_path}")
        
        las_files = stage_data['write_las']
        if isinstance(las_files, str):
            las_files = [las_files]
        for file_path in las_files:
            self.assertTrue(os.path.exists(file_path), f"LAS output should exist: {file_path}")
    
    # =========================================================================
    # JSON SERIALIZATION TESTS
    # =========================================================================
    
    def test_json_serialization_roundtrip(self):
        """Test that all results are properly JSON serializable."""
        pipeline = pylasr.Pipeline()
        pipeline += pylasr.summarise()
        results = pipeline.execute([self.test_file])
        
        # Test serialization
        json_str = json.dumps(results, indent=2)
        self.assertIsInstance(json_str, str)
        self.assertGreater(len(json_str), 100, "JSON should have substantial content")
        
        # Test deserialization
        reconstructed = json.loads(json_str)
        self.assertEqual(reconstructed, results, "JSON roundtrip should preserve data")
        
        # Validate structure is preserved
        self.validate_success_structure(reconstructed, ['summary'])
    
    def test_complex_pipeline_json_serialization(self):
        """Test JSON serialization of complex multi-stage results."""
        output_file = self.create_temp_output(".tif")
        
        pipeline = pylasr.Pipeline()
        pipeline += pylasr.summarise()
        pipeline += pylasr.rasterize(2.0, 2.0, ofile=output_file)
        results = pipeline.execute([self.test_file])
        
        # Test complex results serialization
        json_str = json.dumps(results)
        reconstructed = json.loads(json_str)
        
        self.assertEqual(reconstructed, results, "Complex results should survive JSON roundtrip")
        self.validate_success_structure(reconstructed, ['summary', 'rasterize'])
    
    # =========================================================================
    # ERROR HANDLING TESTS
    # =========================================================================
    
    def test_error_file_not_found_exception(self):
        """Test that non-existent files raise proper exceptions."""
        pipeline = pylasr.Pipeline()
        pipeline += pylasr.summarise()
        
        with self.assertRaises(Exception) as cm:
            pipeline.execute(["nonexistent_file.las"])
        
        error_msg = str(cm.exception)
        self.assertIn("File not found", error_msg, "Error should mention file not found")
        self.assertIn("nonexistent_file.las", error_msg, "Error should mention the filename")
    
    def test_error_invalid_output_directory_exception(self):
        """Test that invalid output directories raise proper exceptions."""
        pipeline = pylasr.Pipeline()
        pipeline += pylasr.rasterize(1.0, 1.0, ofile="/nonexistent/path/output.tif")
        
        with self.assertRaises(Exception) as cm:
            pipeline.execute([self.test_file])
        
        error_msg = str(cm.exception)
        self.assertIn("No such file or directory", error_msg, "Error should mention directory issue")
    
    def test_error_invalid_las_file_exception(self):
        """Test that corrupted LAS files raise proper exceptions."""
        # Create a fake LAS file
        temp_dir = tempfile.mkdtemp()
        fake_las = os.path.join(temp_dir, "fake.las")
        with open(fake_las, 'w') as f:
            f.write("This is not a LAS file")
        
        pipeline = pylasr.Pipeline()
        pipeline += pylasr.summarise()
        
        with self.assertRaises(Exception) as cm:
            pipeline.execute([fake_las])
        
        error_msg = str(cm.exception)
        self.assertIn("LASlib internal error", error_msg, "Error should mention LAS reading issue")
        
        # Cleanup
        try:
            os.unlink(fake_las)
            os.rmdir(temp_dir)
        except:
            pass
    
    def test_error_handling_infrastructure_exists(self):
        """Test that error handling infrastructure is properly set up."""
        # This test validates that our error handling improvements are in place
        
        # Test successful case has proper structure (no redundant message)
        pipeline = pylasr.Pipeline()
        pipeline += pylasr.summarise()
        results = pipeline.execute([self.test_file])
        
        self.assertIsInstance(results, dict, "Results should be dict")
        self.assertTrue(results.get('success'), "Success should be True")
        self.assertNotIn('message', results, "Success should not have redundant message")
        self.assertIn('data', results, "Success should have data")
        
        # If we could trigger a structured error (success=False), it would have proper structure
        # But most errors throw exceptions, which is also good behavior
        
    # =========================================================================
    # EDGE CASES AND ROBUSTNESS TESTS
    # =========================================================================
    
    def test_empty_pipeline_behavior(self):
        """Test behavior with empty pipeline."""
        pipeline = pylasr.Pipeline()
        results = pipeline.execute([self.test_file])
        
        # Empty pipeline should still return structured results
        self.assertIsInstance(results, dict, "Empty pipeline should return dict")
        self.assertTrue(results.get('success'), "Empty pipeline should succeed")
        self.assertIn('data', results, "Empty pipeline should have data field")
    
    def test_pipeline_with_invalid_parameters(self):
        """Test pipeline with questionable parameters."""
        # Negative resolution - should this be an error? Currently it's not.
        pipeline = pylasr.Pipeline()
        pipeline += pylasr.rasterize(-1.0, 1.0)  # Negative resolution
        results = pipeline.execute([self.test_file])
        
        # Currently this succeeds, but arguably should be validated
        self.assertIsInstance(results, dict, "Invalid params should return dict")
        # Note: This might be a area for future improvement
    
    def test_stage_result_consistency(self):
        """Test that the same stage produces consistent results."""
        # Run the same pipeline twice
        pipeline1 = pylasr.Pipeline()
        pipeline1 += pylasr.summarise()
        results1 = pipeline1.execute([self.test_file])
        
        pipeline2 = pylasr.Pipeline()
        pipeline2 += pylasr.summarise()
        results2 = pipeline2.execute([self.test_file])
        
        # Extract stage data
        stage_data1 = self.validate_success_structure(results1, ['summary'])
        stage_data2 = self.validate_success_structure(results2, ['summary'])
        
        # Core statistics should be identical
        summary1 = stage_data1['summary']
        summary2 = stage_data2['summary']
        
        self.assertEqual(summary1['npoints'], summary2['npoints'], "Point count should be consistent")
        self.assertEqual(summary1['nsingle'], summary2['nsingle'], "Single return count should be consistent")
        self.assertEqual(summary1['crs'], summary2['crs'], "CRS should be consistent")
    
    # =========================================================================
    # PERFORMANCE AND MEMORY TESTS
    # =========================================================================
    
    def test_memory_efficiency_no_leaks(self):
        """Test that results don't contain excessive memory usage."""
        pipeline = pylasr.Pipeline()
        pipeline += pylasr.summarise()
        results = pipeline.execute([self.test_file])
        
        # Serialize to check size
        json_str = json.dumps(results)
        json_size = len(json_str)
        
        # Should be reasonable size (not massive)
        self.assertLess(json_size, 50000, "Results should not be excessively large")
        self.assertGreater(json_size, 100, "Results should have substantial content")
    
    def test_large_pipeline_scalability(self):
        """Test that large pipelines with many stages work correctly."""
        # Create a pipeline with multiple stages
        output1 = self.create_temp_output("_1.tif")
        output2 = self.create_temp_output("_2.tif")
        output3 = self.create_temp_output(".las")
        
        pipeline = pylasr.Pipeline()
        pipeline += pylasr.classify_with_csf()
        pipeline += pylasr.summarise()
        pipeline += pylasr.rasterize(1.0, 1.0, ofile=output1)
        pipeline += pylasr.rasterize(2.0, 2.0, ofile=output2)
        pipeline += pylasr.write_las(ofile=output3)
        
        results = pipeline.execute([self.test_file])
        
        stage_data = self.validate_success_structure(results)
        
        # Should have multiple stages
        expected_stages = ['summary', 'rasterize', 'write_las']
        for stage in expected_stages:
            self.assertIn(stage, stage_data, f"Large pipeline missing: {stage}")

if __name__ == "__main__":
    unittest.main(argv=[''], verbosity=2, exit=False)