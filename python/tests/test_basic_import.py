#!/usr/bin/env python3
"""
Basic import and system information tests for pylasr
"""

import os
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


class TestBasicImport(unittest.TestCase):
    """Test basic import and module availability"""

    def test_import_success(self):
        """Test that pylasr can be imported"""
        self.assertTrue(
            PYLASR_AVAILABLE,
            f"Failed to import pylasr: {IMPORT_ERROR if not PYLASR_AVAILABLE else ''}",
        )

    def test_version_available(self):
        """Test that version information is available"""
        if not PYLASR_AVAILABLE:
            self.skipTest("pylasr not available")

        self.assertTrue(hasattr(pylasr, "__version__"))
        version = pylasr.__version__
        self.assertIsInstance(version, str)
        self.assertGreater(len(version), 0)
        print(f"pylasr version: {version}")


class TestSystemInfo(unittest.TestCase):
    """Test system information functions"""

    def setUp(self):
        if not PYLASR_AVAILABLE:
            self.skipTest("pylasr not available")

    def test_available_threads(self):
        """Test available_threads function"""
        threads = pylasr.available_threads()
        self.assertIsInstance(threads, int)
        self.assertGreater(threads, 0)
        print(f"Available threads: {threads}")

    def test_has_omp_support(self):
        """Test OpenMP support detection"""
        omp_support = pylasr.has_omp_support()
        self.assertIsInstance(omp_support, bool)
        print(f"OpenMP support: {omp_support}")

    def test_memory_info(self):
        """Test memory information functions"""
        available_ram = pylasr.get_available_ram()
        total_ram = pylasr.get_total_ram()

        self.assertIsInstance(available_ram, int)
        self.assertIsInstance(total_ram, int)
        self.assertGreater(available_ram, 0)
        self.assertGreater(total_ram, 0)
        self.assertLessEqual(available_ram, total_ram)

        print(f"Available RAM: {available_ram / (1024**3):.2f} GB")
        print(f"Total RAM: {total_ram / (1024**3):.2f} GB")


class TestUtilityFunctions(unittest.TestCase):
    """Test utility functions"""

    def setUp(self):
        if not PYLASR_AVAILABLE:
            self.skipTest("pylasr not available")

    def test_las_filter_usage(self):
        """Test LAS filter usage function"""
        try:
            pylasr.las_filter_usage()
        except Exception as e:
            self.fail(f"las_filter_usage() raised an exception: {e}")

    def test_las_transform_usage(self):
        """Test LAS transform usage function"""
        try:
            pylasr.las_transform_usage()
        except Exception as e:
            self.fail(f"las_transform_usage() raised an exception: {e}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
