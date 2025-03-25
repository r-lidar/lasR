#!/usr/bin/env python
"""
Diagnostic script to troubleshoot Python module import issues.
"""
import sys
import os
import site
import importlib.util

def check_site_packages():
    """Check all site-packages directories for the module."""
    print(f"Python version: {sys.version}")
    print(f"Python executable: {sys.executable}")
    
    # Check sys.path
    print("\nPython search paths (sys.path):")
    for i, path in enumerate(sys.path):
        print(f"  {i}: {path}")
    
    # Check site-packages directories
    print("\nSite-packages directories:")
    for path in site.getsitepackages():
        print(f"  {path}")
    print(f"  User site-packages: {site.getusersitepackages()}")
    
    # Check for the module in each site-packages directory
    module_name = "pylasr"
    print(f"\nLooking for '{module_name}' module:")
    found = False
    
    # Check all possible locations
    potential_locations = sys.path + site.getsitepackages() + [site.getusersitepackages()]
    
    for path in potential_locations:
        # Check for .so file
        so_path = os.path.join(path, f"{module_name}.so")
        if os.path.exists(so_path):
            print(f"  Found .so file: {so_path}")
            found = True
            
        # Check for .egg-link file (editable install)
        egg_link = os.path.join(path, f"{module_name}.egg-link")
        if os.path.exists(egg_link):
            with open(egg_link, 'r') as f:
                link_path = f.read().strip()
            print(f"  Found egg-link: {egg_link} -> {link_path}")
            found = True
            
        # Check for module directory
        module_path = os.path.join(path, module_name)
        if os.path.isdir(module_path):
            print(f"  Found module directory: {module_path}")
            found = True
            # List files in the directory
            print(f"    Contents: {os.listdir(module_path)}")
    
    # Check if we can find the module using importlib
    spec = importlib.util.find_spec(module_name)
    if spec:
        print(f"\nModule '{module_name}' found by importlib at: {spec.origin}")
    else:
        print(f"\nModule '{module_name}' NOT found by importlib")
    
    if not found:
        print(f"  Module '{module_name}' not found in any site-packages directory")
    
    # Try importing directly and report any errors
    print("\nAttempting direct import:")
    try:
        print("  import pylasr")
        import pylasr
        print(f"  Success! Module loaded from: {pylasr.__file__}")
    except ImportError as e:
        print(f"  Failed: {str(e)}")
    except Exception as e:
        print(f"  Error: {type(e).__name__}: {str(e)}")

if __name__ == "__main__":
    check_site_packages() 