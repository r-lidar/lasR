#!/usr/bin/env python3
"""
Test runner for pylasr tests with comprehensive reporting
"""

import sys
import os
import unittest
import tempfile
import time
from io import StringIO

# Add the parent directory to sys.path to import pylasr
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

def run_all_tests():
    """Run all pylasr tests with detailed reporting"""
    
    print("=" * 80)
    print("PYLASR COMPREHENSIVE TEST SUITE")
    print("=" * 80)
    
    # Try to import pylasr first
    try:
        import pylasr
        print(f"✅ pylasr imported successfully (version: {pylasr.__version__})")
    except ImportError as e:
        print(f"❌ Failed to import pylasr: {e}")
        print("\nMake sure pylasr is built and available in the current directory.")
        return False
    
    print()
    
    # System information
    print("📊 SYSTEM INFORMATION")
    print("-" * 40)
    try:
        print(f"Available threads: {pylasr.available_threads()}")
        print(f"OpenMP support: {pylasr.has_omp_support()}")
        print(f"Available RAM: {pylasr.get_available_ram() / (1024**3):.2f} GB")
        print(f"Total RAM: {pylasr.get_total_ram() / (1024**3):.2f} GB")
    except Exception as e:
        print(f"⚠️  Could not retrieve system info: {e}")
    
    print()
    
    # Discover and run tests
    test_loader = unittest.TestLoader()
    test_suite = unittest.TestSuite()
    
    # Test modules to run
    test_modules = [
        'test_basic_import',
        'test_pipeline', 
        'test_functions',
        'test_integration'
    ]
    
    print("🧪 RUNNING TEST MODULES")
    print("-" * 40)
    
    for module_name in test_modules:
        try:
            # Import the test module
            module = __import__(module_name)
            
            # Load tests from the module
            module_suite = test_loader.loadTestsFromModule(module)
            test_suite.addTest(module_suite)
            
            print(f"✅ Loaded tests from {module_name}")
            
        except ImportError as e:
            print(f"❌ Failed to load {module_name}: {e}")
        except Exception as e:
            print(f"⚠️  Error loading {module_name}: {e}")
    
    print()
    
    # Run the tests
    print("🏃 EXECUTING TESTS")
    print("-" * 40)
    
    # Custom test result class for better reporting
    class VerboseTestResult(unittest.TextTestResult):
        def startTest(self, test):
            super().startTest(test)
            self.stream.write(f"Running {test._testMethodName}... ")
            self.stream.flush()
        
        def addSuccess(self, test):
            super().addSuccess(test)
            self.stream.write("✅ PASS\n")
        
        def addError(self, test, err):
            super().addError(test, err)
            self.stream.write("❌ ERROR\n")
        
        def addFailure(self, test, err):
            super().addFailure(test, err)
            self.stream.write("❌ FAIL\n")
        
        def addSkip(self, test, reason):
            super().addSkip(test, reason)
            self.stream.write(f"⏭️  SKIP ({reason})\n")
    
    # Run tests with custom result handler
    runner = unittest.TextTestRunner(
        stream=sys.stdout,
        verbosity=2,
        resultclass=VerboseTestResult
    )
    
    start_time = time.time()
    result = runner.run(test_suite)
    end_time = time.time()
    
    print()
    print("📈 TEST SUMMARY")
    print("-" * 40)
    print(f"Tests run: {result.testsRun}")
    print(f"Failures: {len(result.failures)}")
    print(f"Errors: {len(result.errors)}")
    print(f"Skipped: {len(result.skipped)}")
    print(f"Success rate: {((result.testsRun - len(result.failures) - len(result.errors)) / max(result.testsRun, 1)) * 100:.1f}%")
    print(f"Execution time: {end_time - start_time:.2f} seconds")
    
    # Report failures and errors
    if result.failures:
        print("\n❌ FAILURES:")
        print("-" * 40)
        for test, traceback in result.failures:
            print(f"FAIL: {test}")
            print(traceback)
            print()
    
    if result.errors:
        print("\n💥 ERRORS:")
        print("-" * 40)
        for test, traceback in result.errors:
            print(f"ERROR: {test}")
            print(traceback)
            print()
    
    # Overall result
    success = len(result.failures) == 0 and len(result.errors) == 0
    
    print("=" * 80)
    if success:
        print("🎉 ALL TESTS PASSED!")
        print("The pylasr Python API is working correctly.")
    else:
        print("⚠️  SOME TESTS FAILED")
        print("Please review the failures and errors above.")
    print("=" * 80)
    
    return success


def run_quick_test():
    """Run a quick smoke test to verify basic functionality"""
    
    print("🔥 PYLASR QUICK SMOKE TEST")
    print("-" * 40)
    
    try:
        import pylasr
        print("✅ Import successful")
        
        # Test basic system info
        threads = pylasr.available_threads()
        print(f"✅ System info: {threads} threads available")
        
        # Test pipeline creation with info stage
        pipeline = pylasr.info()
        print(f"✅ Pipeline creation with info stage")
        
        # Test pipeline creation  
        pipeline = pylasr.Pipeline()
        pipeline += pylasr.info()
        print("✅ Pipeline creation and stage addition")
        
        # Test JSON export
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            json_file = pipeline.write_json(f.name)
            if os.path.exists(json_file):
                print("✅ JSON export")
                os.unlink(json_file)
            else:
                print("❌ JSON export failed")
                return False
        
        # Test convenience functions
        dtm_pipeline = pylasr.dtm(1.0, "test.tif")
        print("✅ Convenience functions")
        
        print("\n🎉 QUICK TEST PASSED - pylasr is working!")
        return True
        
    except Exception as e:
        print(f"❌ Quick test failed: {e}")
        return False


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--quick':
        success = run_quick_test()
    else:
        success = run_all_tests()
    
    sys.exit(0 if success else 1)