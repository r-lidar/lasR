import multiprocessing
import os
import platform
import shutil
import subprocess
import sys

from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext


class CMakeExtension(Extension):
    def __init__(self, name, sourcedir=""):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)


class CMakeBuild(build_ext):
    def run(self):
        try:
            out = subprocess.check_output(["cmake", "--version"])
            cmake_version = out.decode().split("\n")[0].split(" ")[2]
            print(f"Found CMake version: {cmake_version}")
        except Exception as e:
            raise RuntimeError(
                f"CMake must be installed to build this extension. Error: {str(e)}\n"
                "Please install CMake 3.18 or newer."
            ) from e

        for ext in self.extensions:
            self.build_extension(ext)

    def build_extension(self, ext):
        # Determine full path for the compiled extension
        ext_path = os.path.abspath(self.get_ext_fullpath(ext.name))
        # For in-place builds, place next to setup.py
        if self.inplace:
            extdir = os.path.dirname(os.path.abspath(__file__))
            print(f"Building in-place at: {extdir}")
        else:
            # Wheel build: place shared lib next to ext_path, inside build/lib... for packaging
            extdir = os.path.dirname(ext_path)
            print(f"Building wheel extension into: {extdir}")

        # Ensure the library output directory exists so the wheel can include shared libs
        os.makedirs(extdir, exist_ok=True)
        # Create build directory
        if not os.path.exists(self.build_temp):
            os.makedirs(self.build_temp)
        else:
            print(f"Cleaning existing build directory: {self.build_temp}")
            # Clear the CMake cache to ensure clean builds
            cache_file = os.path.join(self.build_temp, "CMakeCache.txt")
            if os.path.exists(cache_file):
                os.remove(cache_file)

        # Define cmake args
        cmake_args = [
            f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={extdir}",
            f"-DPYTHON_EXECUTABLE={sys.executable}",
            # Force CMake to use the Python interpreter we're building for
            f"-DPython_ROOT_DIR={os.path.dirname(os.path.dirname(sys.executable))}",
            "-DCMAKE_CXX_STANDARD=17",
            "-DCMAKE_CXX_STANDARD_REQUIRED=ON",
            # Add visibility settings
            "-DCMAKE_CXX_VISIBILITY_PRESET=hidden",
            "-DCMAKE_VISIBILITY_INLINES_HIDDEN=ON",
            # Verbose output for better debugging
            "-DCMAKE_VERBOSE_MAKEFILE=ON",
        ]

        # Add additional CMake args from environment if set
        if "CMAKE_ARGS" in os.environ:
            cmake_args += [item for item in os.environ["CMAKE_ARGS"].split(" ") if item]

        cfg = "Debug" if self.debug else "Release"
        build_args = ["--config", cfg]
        cmake_args += ["-DCMAKE_BUILD_TYPE=" + cfg]

        # Calculate number of parallel jobs
        parallelism = os.environ.get(
            "CMAKE_BUILD_PARALLEL_LEVEL", str(max(1, multiprocessing.cpu_count() - 1))
        )
        print(f"Using {parallelism} parallel build jobs")

        # Detect platform
        if platform.system() == "Windows":
            build_args += ["--", f"/maxcpucount:{parallelism}"]
        else:
            build_args += ["--", f"-j{parallelism}"]

        env = os.environ.copy()

        # Add C++17 compiler flags directly as well
        compiler_flags = env.get("CXXFLAGS", "")
        version_macro = f'-DVERSION_INFO=\\"{self.distribution.get_version()}\\"'
        if platform.system() == "Windows":
            env["CXXFLAGS"] = f"{compiler_flags} /std:c++17 {version_macro}"
        else:
            env["CXXFLAGS"] = f"{compiler_flags} -std=c++17 {version_macro}"

        # Check for GDAL environment variables
        if "GDAL_DIR" in env:
            print(f"Using GDAL_DIR: {env['GDAL_DIR']}")
        if "GDAL_INCLUDE_DIR" in env:
            print(f"Using GDAL_INCLUDE_DIR: {env['GDAL_INCLUDE_DIR']}")

        print(f"\nCMake args: {cmake_args}")
        print(f"Build args: {build_args}")
        print(f"Python executable: {sys.executable}")
        print(f"Output directory: {extdir}")
        print(f"Build temp directory: {self.build_temp}")
        print(f"Environment variables: CXXFLAGS={env.get('CXXFLAGS', '')}")

        try:
            print("\nRunning CMake configuration...")
            subprocess.check_call(
                ["cmake", ext.sourcedir] + cmake_args, cwd=self.build_temp, env=env
            )

            print("\nRunning CMake build...")
            os.makedirs(extdir, exist_ok=True)
            subprocess.check_call(
                ["cmake", "--build", "."] + build_args, cwd=self.build_temp
            )

            # Ensure the compiled module gets copied to the right place for bdist_wheel
            import sysconfig
            source_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), f"pylasr{sysconfig.get_config_var('EXT_SUFFIX')}")
            if os.path.exists(source_file):
                dest_file = os.path.join(extdir, os.path.basename(source_file))
                # Only copy if source and destination are different
                if os.path.abspath(source_file) != os.path.abspath(dest_file):
                    print(f"Copying {source_file} to {dest_file}")
                    shutil.copy2(source_file, dest_file)
                else:
                    print(f"Source and destination are the same, skipping copy: {source_file}")
            
            # Print the output directory content
            print(f"\nFiles in output directory {extdir}:")
            try:
                for f in os.listdir(extdir):
                    if f.endswith('.so') or f.endswith('.pyd'):
                        print(f"  {f} (Python extension module)")
                        # Also check if it's the right Python version
                        expected_suffix = sysconfig.get_config_var('EXT_SUFFIX')
                        if expected_suffix and expected_suffix in f:
                            print(f"    ✓ Matches expected suffix: {expected_suffix}")
                        else:
                            print(f"    ⚠ Expected suffix: {expected_suffix}")
                    else:
                        print(f"  {f}")
            except FileNotFoundError:
                print(f"  Directory not found or empty")

        except subprocess.CalledProcessError as e:
            print(f"Build error: {str(e)}")
            # Try to find and print the CMake error log
            error_log = os.path.join(self.build_temp, "CMakeFiles", "CMakeError.log")
            if os.path.exists(error_log):
                print("\nCMake Error Log:")
                with open(error_log, "r") as f:
                    print(f.read())
            raise RuntimeError(f"Error: {str(e)}") from e


setup(
    name="pylasr",
    version="0.17.0",
    description="Python bindings for LASR library",
    author="LASR Team",
    ext_modules=[CMakeExtension("pylasr", sourcedir=".")],  # Explicit project root
    cmdclass={"build_ext": CMakeBuild},
    zip_safe=False,
    python_requires=">=3.9",
    py_modules=[],  # Explicitly set to avoid auto-discovery issues
)
