#!/usr/bin/env python
"""
Script to fix module installation issues by copying the module to the correct location.
"""
import os
import sys
import site
import shutil
import glob

def find_module_so():
    """Find the compiled .so or .dylib module file in the current directory."""
    # Look for .so files first (Linux/macOS)
    so_files = glob.glob("pylasr*.so")
    if so_files:
        return so_files[0]
    
    # Look for .dylib files on macOS
    dylib_files = glob.glob("pylasr*.dylib")
    if dylib_files:
        return dylib_files[0]
    
    # Look for .pyd files on Windows
    pyd_files = glob.glob("pylasr*.pyd")
    if pyd_files:
        return pyd_files[0]
    
    # Look in the build directory
    for root, dirs, files in os.walk("build"):
        for file in files:
            if file.startswith("pylasr") and (file.endswith(".so") or 
                                             file.endswith(".dylib") or 
                                             file.endswith(".pyd")):
                return os.path.join(root, file)
    
    return None

def copy_to_site_packages():
    """Copy the module to the site-packages directory."""
    # Find the module file
    module_file = find_module_so()
    if not module_file:
        print("Error: Could not find the compiled module file.")
        return False
    
    print(f"Found module file: {module_file}")
    
    # Get the site-packages directory
    site_packages = site.getsitepackages()[0]
    dest_file = os.path.join(site_packages, "pylasr.so")
    
    # On Windows, use .pyd extension
    if sys.platform == "win32":
        dest_file = os.path.join(site_packages, "pylasr.pyd")
    
    print(f"Copying to: {dest_file}")
    
    try:
        shutil.copy2(module_file, dest_file)
        print("Module successfully copied to site-packages.")
        return True
    except Exception as e:
        print(f"Error copying module: {str(e)}")
        return False

def test_import():
    """Test importing the module."""
    print("Testing import...")
    try:
        import pylasr
        print("Success! Module imported correctly.")
        return True
    except ImportError as e:
        print(f"Import failed: {str(e)}")
        return False

if __name__ == "__main__":
    if copy_to_site_packages():
        test_import() 