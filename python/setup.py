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
        extdir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))

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
            subprocess.check_call(
                ["cmake", "--build", "."] + build_args, cwd=self.build_temp
            )

            # Print the output directory content
            print(f"\nFiles in output directory {extdir}:")
            for f in os.listdir(extdir):
                print(f"  {f}")

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
    version="0.1.0",
    description="Python bindings for LASR library",
    author="LASR Team",
    ext_modules=[CMakeExtension("pylasr")],
    cmdclass={"build_ext": CMakeBuild},
    zip_safe=False,
    python_requires=">=3.9",
    include_package_data=True,
    # Add a custom PyPI classifier for easy identification
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Science/Research",
        "License :: OSI Approved :: MIT License",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "Topic :: Scientific/Engineering :: GIS",
    ],
    long_description="""
    Python bindings for LASR library.

    This package includes several third-party open source libraries:
    - Eigen: Linear Algebra library (MPL2) - https://eigen.tuxfamily.org/
    - LASlib/LASzip: LAS/LAZ file handling (LGPL) - https://github.com/LAStools/LAStools
    - CSF: Cloth Simulation Filter - https://github.com/jianboqi/CSF
    - Delaunator: Delaunay triangulation - https://github.com/delfrrr/delaunator-cpp
    - Geophoton: CHM enhancement - https://github.com/Geophoton-inc/chm_prep

    Please refer to the individual LICENSE files in the src/vendor directory for detailed license information.
    """,
)
