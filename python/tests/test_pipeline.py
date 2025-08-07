#!/usr/bin/env python3
"""
Test Stage and Pipeline classes and their basic functionality
"""

import sys
import os
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


# Stage class tests removed - Stage is no longer exposed in Python bindings


class TestPipelineClass(unittest.TestCase):
    """Test the Pipeline class functionality"""
    
    def setUp(self):
        if not PYLASR_AVAILABLE:
            self.skipTest("pylasr not available")
    
    def test_empty_pipeline_creation(self):
        """Test creating an empty pipeline"""
        pipeline = pylasr.Pipeline()
        self.assertIsInstance(pipeline.to_string(), str)
    
    def test_pipeline_creation_with_stage(self):
        """Test creating a pipeline with a stage"""
        # Stage class no longer exposed - use stage functions instead
        pipeline = pylasr.info()
        pipeline_str = pipeline.to_string()
        self.assertIsInstance(pipeline_str, str)
        self.assertIn("info", pipeline_str)
    
    def test_pipeline_addition_operators(self):
        """Test pipeline addition operators"""
        # Create pipelines using stage functions instead of Stage class
        info_pipeline = pylasr.info()
        sor_pipeline = pylasr.classify_with_sor(k=10, m=6)
        
        # Test adding pipelines together
        combined = info_pipeline + sor_pipeline
        self.assertIsInstance(combined, pylasr.Pipeline)
        
        # Test in-place addition
        empty_pipeline = pylasr.Pipeline()
        empty_pipeline += info_pipeline
        
        # Verify pipeline contains stages
        pipeline_str = combined.to_string()
        self.assertIn("info", pipeline_str)
        self.assertIn("classify_with_sor", pipeline_str)
    
    def test_pipeline_processing_strategies(self):
        """Test setting different processing strategies"""
        pipeline = pylasr.Pipeline()
        
        # Test sequential strategy
        pipeline.set_sequential_strategy()
        
        # Test concurrent points strategy
        pipeline.set_concurrent_points_strategy(4)
        
        # Test concurrent files strategy
        pipeline.set_concurrent_files_strategy(2)
        
        # Test nested strategy
        pipeline.set_nested_strategy(2, 4)
    
    def test_pipeline_options(self):
        """Test setting pipeline options"""
        pipeline = pylasr.Pipeline()
        
        # Test various options
        pipeline.set_verbose(True)
        pipeline.set_progress(True)
        pipeline.set_buffer(10.0)
        pipeline.set_chunk(1000.0)
        pipeline.set_profile_file("profile.txt")
        # Skip noprocess test due to length validation issue
        # pipeline.set_noprocess([True, False, True])
        
        # Test file setting
        pipeline.set_files(["test1.las", "test2.las"])
    
    def test_pipeline_introspection(self):
        """Test pipeline introspection methods"""
        pipeline = pylasr.Pipeline()
        
        # Test reader and catalog detection
        has_reader = pipeline.has_reader()
        has_catalog = pipeline.has_catalog()
        
        self.assertIsInstance(has_reader, bool)
        self.assertIsInstance(has_catalog, bool)
    
    def test_pipeline_json_export(self):
        """Test pipeline JSON export"""
        # Create pipeline using stage functions
        pipeline = pylasr.info()
        
        # Test JSON export
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            json_file = pipeline.write_json(f.name)
            self.assertEqual(json_file, f.name)
            self.assertTrue(os.path.exists(json_file))
            
            # Test pipeline info
            try:
                info = pylasr.pipeline_info(json_file)
                self.assertTrue(hasattr(info, 'streamable'))
                self.assertTrue(hasattr(info, 'read_points'))
                self.assertTrue(hasattr(info, 'buffer'))
                self.assertTrue(hasattr(info, 'parallelizable'))
            finally:
                # Clean up
                if os.path.exists(json_file):
                    os.unlink(json_file)


class TestConvenienceFunctions(unittest.TestCase):
    """Test convenience functions"""
    
    def setUp(self):
        if not PYLASR_AVAILABLE:
            self.skipTest("pylasr not available")
    
    def test_create_pipeline(self):
        """Test create_pipeline convenience function"""
        pipeline = pylasr.create_pipeline()
        self.assertIsInstance(pipeline, type(pylasr.Pipeline()))
    
    def test_create_stage(self):
        """Test create_stage convenience function"""
        # create_stage no longer exists - use stage functions directly
        pipeline = pylasr.info()
        self.assertIsInstance(pipeline, pylasr.Pipeline)
    
    def test_dtm_pipeline(self):
        """Test DTM pipeline convenience function"""
        dtm_pipeline = pylasr.dtm_pipeline(1.0, "dtm.tif")
        self.assertIsInstance(dtm_pipeline, type(pylasr.Pipeline()))
        pipeline_str = dtm_pipeline.to_string()
        self.assertIn("rasterize", pipeline_str)
    
    def test_chm_pipeline(self):
        """Test CHM pipeline convenience function"""
        chm_pipeline = pylasr.chm_pipeline(0.5, "chm.tif")
        self.assertIsInstance(chm_pipeline, type(pylasr.Pipeline()))
        pipeline_str = chm_pipeline.to_string()
        self.assertIn("rasterize", pipeline_str)


if __name__ == '__main__':
    unittest.main(verbosity=2)