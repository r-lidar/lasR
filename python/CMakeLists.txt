cmake_minimum_required(VERSION 3.7)

project(pylasr)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Get the parent directory
get_filename_component(LASR_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/.. ABSOLUTE)
message(STATUS "LASR_SOURCE_DIR: ${LASR_SOURCE_DIR}")

# OpenMP configuration for macOS 
if(APPLE)
    # We need a different approach for macOS with Apple Clang
    execute_process(
        COMMAND brew --prefix libomp
        OUTPUT_VARIABLE HOMEBREW_LIBOMP_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    message(STATUS "OpenMP homebrew prefix: ${HOMEBREW_LIBOMP_PREFIX}")
    
    # Use a simpler approach for Apple Clang
    if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp")
        set(OpenMP_C_FLAGS "-Xpreprocessor -fopenmp")
        
        # Include and lib directories
        set(OpenMP_CXX_INCLUDE_DIRS "${HOMEBREW_LIBOMP_PREFIX}/include")
        set(OpenMP_C_INCLUDE_DIRS "${HOMEBREW_LIBOMP_PREFIX}/include")
        
        # Link explicitly with libomp
        set(OpenMP_CXX_LIBRARIES "-L${HOMEBREW_LIBOMP_PREFIX}/lib -lomp")
        set(OpenMP_C_LIBRARIES "-L${HOMEBREW_LIBOMP_PREFIX}/lib -lomp")
        
        # Ensure we can find libomp at runtime
        set(CMAKE_INSTALL_RPATH "${HOMEBREW_LIBOMP_PREFIX}/lib")
        set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
        
        # Define the OpenMP variables manually so find_package(OpenMP) succeeds
        set(OpenMP_CXX_FOUND TRUE)
        set(OpenMP_C_FOUND TRUE)
    endif()
endif()

# Find OpenMP
option(USE_OPENMP "Use OpenMP" ON)
if(USE_OPENMP)
    if(NOT (APPLE AND (CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")))
        find_package(OpenMP)
    endif()
    
    if(OpenMP_CXX_FOUND)
        message(STATUS "OpenMP found and enabled")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
        include_directories(${OpenMP_CXX_INCLUDE_DIRS})
        link_libraries(${OpenMP_CXX_LIBRARIES})
        add_definitions(-D_OPENMP)
        add_definitions(-DUSE_OPENMP=1)
    else()
        message(WARNING "OpenMP not found. LASR will be compiled without parallel processing support.")
        add_definitions(-DUSE_OPENMP=0)
    endif()
else()
    message(STATUS "OpenMP disabled. LASR will be compiled without parallel processing support.")
    add_definitions(-DUSE_OPENMP=0)
endif()

# Compiler options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unknown-pragmas -Wno-unused-variable -Wno-deprecated -DNOMINMAX")

# Check for link-time optimization
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-flto=thin" HAS_FLTO_THIN)
if(HAS_FLTO_THIN)
  message(STATUS "Link-time optimization enabled")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto=thin")
endif()

# Find Python
find_package(Python COMPONENTS Interpreter Development REQUIRED)

# Find pybind11 via CMake config (installed by pip)
find_package(pybind11 CONFIG)

# If pybind11 wasn't found via config, try adding the subdirectory
if(NOT pybind11_FOUND)
  message(STATUS "pybind11 not found via CMake config, trying subdirectory")
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/pybind11/CMakeLists.txt")
    add_subdirectory(pybind11)
  else()
    message(FATAL_ERROR "pybind11 not found. Please install via pip: pip install pybind11")
  endif()
else()
  message(STATUS "Found pybind11 via CMake config")
endif()

if(NOT Python_FOUND)
  message(FATAL_ERROR "Python not found")
endif()

# Find GDAL
find_package(GDAL)
if(GDAL_FOUND)
  message(STATUS "Found GDAL version: ${GDAL_VERSION}")
  message(STATUS "GDAL include dirs: ${GDAL_INCLUDE_DIRS}")
  message(STATUS "GDAL libraries: ${GDAL_LIBRARIES}")
  add_definitions(-DGDAL_FOUND=1)
else()
  message(STATUS "GDAL not found")
  add_definitions(-DGDAL_FOUND=0)
endif()

# Include directories
include_directories(
  ${Python_INCLUDE_DIRS}
  ${PROJECT_SOURCE_DIR}
  ${LASR_SOURCE_DIR}
  ${LASR_SOURCE_DIR}/src
  ${LASR_SOURCE_DIR}/src/vendor
  ${LASR_SOURCE_DIR}/src/vendor/LASlib
  ${LASR_SOURCE_DIR}/src/vendor/LASzip
  ${LASR_SOURCE_DIR}/src/vendor/common
  ${LASR_SOURCE_DIR}/src/LASRcore
  ${LASR_SOURCE_DIR}/src/LASRreaders
  ${LASR_SOURCE_DIR}/src/LASRstages
  ${GDAL_INCLUDE_DIRS}
)

# Define the LASzip library target
add_library(laszip STATIC
  ${LASR_SOURCE_DIR}/src/vendor/LASzip/arithmeticdecoder.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASzip/arithmeticencoder.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASzip/arithmeticmodel.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASzip/integercompressor.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASzip/lascopc.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASzip/lasindex.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASzip/lasinterval.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASzip/lasquadtree.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASzip/lasreaditemcompressed_v1.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASzip/lasreaditemcompressed_v2.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASzip/lasreaditemcompressed_v3.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASzip/lasreaditemcompressed_v4.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASzip/lasreadpoint.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASzip/laswriteitemcompressed_v1.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASzip/laswriteitemcompressed_v2.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASzip/laswriteitemcompressed_v3.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASzip/laswriteitemcompressed_v4.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASzip/laswritepoint.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASzip/laszip.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASzip/mydefs.cpp
)

# Enable PIC for laszip since it's used in a shared library
set_target_properties(laszip PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(laszip PRIVATE 
  COMPILATION_CLOSED=1
  NDEBUG=1
  UNORDERED=1
  USING_R=0
)

# Create the Python extension module
pybind11_add_module(pylasr 
  # Core bindings
  bindings.cpp
  export_python.cpp
  
  # Core implementation
  ${LASR_SOURCE_DIR}/src/process.cpp
  ${LASR_SOURCE_DIR}/src/LASRcore/RAM.cpp
  ${LASR_SOURCE_DIR}/src/LASRcore/openmp.cpp
  ${LASR_SOURCE_DIR}/src/LASRcore/error.cpp
  ${LASR_SOURCE_DIR}/src/LASRcore/print.cpp
  ${LASR_SOURCE_DIR}/src/LASRcore/DrawflowParser.cpp
  ${LASR_SOURCE_DIR}/src/LASRcore/PointCloud.cpp
  ${LASR_SOURCE_DIR}/src/LASRcore/CRS.cpp
  ${LASR_SOURCE_DIR}/src/LASRcore/FileCollection.cpp
  ${LASR_SOURCE_DIR}/src/LASRcore/Shape.cpp
  ${LASR_SOURCE_DIR}/src/LASRcore/Header.cpp
  ${LASR_SOURCE_DIR}/src/LASRcore/PointSchema.cpp
  ${LASR_SOURCE_DIR}/src/LASRcore/PointFilter.cpp
  ${LASR_SOURCE_DIR}/src/LASRcore/GridPartition.cpp
  ${LASR_SOURCE_DIR}/src/LASRcore/Grid.cpp
  ${LASR_SOURCE_DIR}/src/LASRcore/Grouper.cpp
  ${LASR_SOURCE_DIR}/src/LASRcore/Stage.cpp
  ${LASR_SOURCE_DIR}/src/LASRcore/GDALdataset.cpp
  ${LASR_SOURCE_DIR}/src/LASRcore/Metrics.cpp
  ${LASR_SOURCE_DIR}/src/LASRcore/PointLAS.cpp
  ${LASR_SOURCE_DIR}/src/LASRcore/Profiler.cpp
  ${LASR_SOURCE_DIR}/src/LASRcore/Progress.cpp
  ${LASR_SOURCE_DIR}/src/LASRcore/Raster.cpp
  ${LASR_SOURCE_DIR}/src/LASRcore/Vector.cpp
  ${LASR_SOURCE_DIR}/src/LASRcore/parser.cpp
  ${LASR_SOURCE_DIR}/src/LASRcore/pipeline.cpp
  
  # I/O implementation
  ${LASR_SOURCE_DIR}/src/LASRreaders/LASio.cpp
  ${LASR_SOURCE_DIR}/src/LASRreaders/PCDio.cpp
  
  # Stages implementation
  ${LASR_SOURCE_DIR}/src/LASRstages/info.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/filter.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/pitfill.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/addattribute.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/boundaries.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/breakif.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/csf.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/focal.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/ivf.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/loadmatrix.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/loadraster.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/localmaximum.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/nnmetrics.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/nothing.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/rasterize.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/readlas.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/readpcd.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/regiongrowing.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/sampling.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/setcrs.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/sor.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/sort.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/svd.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/transformwith.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/triangulate.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/writelas.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/writelax.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/writepcd.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/writevpc.cpp
  ${LASR_SOURCE_DIR}/src/LASRstages/summary.cpp
  
  # Vendor implementations
  ${LASR_SOURCE_DIR}/src/vendor/delaunator/delaunator.cpp
  ${LASR_SOURCE_DIR}/src/vendor/csf/CSF.cpp
  ${LASR_SOURCE_DIR}/src/vendor/csf/Cloth.cpp
  ${LASR_SOURCE_DIR}/src/vendor/csf/Constraint.cpp
  ${LASR_SOURCE_DIR}/src/vendor/csf/Particle.cpp
  ${LASR_SOURCE_DIR}/src/vendor/csf/Rasterization.cpp
  ${LASR_SOURCE_DIR}/src/vendor/csf/c2cdist.cpp
  ${LASR_SOURCE_DIR}/src/vendor/csf/point_cloud.cpp
  ${LASR_SOURCE_DIR}/src/vendor/geophoton/chm_prep.cpp
  
  # LASlib vendor files
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/lasreader.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/lasreader_las.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/lasreader_txt.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/lasreader_asc.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/lasreader_bil.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/lasreader_bin.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/lasreader_dtm.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/lasreader_ply.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/lasreader_qfit.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/lasreader_shp.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/lasreadermerged.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/lasreaderbuffered.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/lasreaderpipeon.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/lasreaderstored.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/lastransform.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/laswriter.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/laswriter_las.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/laswriter_bin.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/laswriter_copc.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/laswriter_qfit.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/laswriter_txt.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/laswriter_wrl.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/lasutility.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/laskdtree.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/geoprojectionconverter.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/lasfilter.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/lasignore.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/fopen_compressed.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/laswaveform13reader.cpp
  ${LASR_SOURCE_DIR}/src/vendor/LASlib/laswaveform13writer.cpp
)

# Link libraries
target_link_libraries(pylasr PRIVATE laszip ${GDAL_LIBRARIES})

target_compile_definitions(pylasr PRIVATE 
  USING_PYTHON=1
  NDEBUG=1
  UNORDERED=1
  COMPILATION_CLOSED=1
)

# Set the output directory and output name for different platforms
if(APPLE)
  set_target_properties(pylasr PROPERTIES 
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_NAME "pylasr"
    SUFFIX ".so"  # Use .so extension even on macOS
  )
elseif(WIN32)
  set_target_properties(pylasr PROPERTIES 
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_NAME "pylasr"
    SUFFIX ".pyd"  # Windows Python extension
  )
else()
  set_target_properties(pylasr PROPERTIES 
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_NAME "pylasr"
  )
endif()

# Print output information
message(STATUS "Python module will be built to: ${CMAKE_CURRENT_SOURCE_DIR}/pylasr${CMAKE_SHARED_LIBRARY_SUFFIX}")
message(STATUS "Python interpreter: ${Python_EXECUTABLE}")
message(STATUS "Python include dirs: ${Python_INCLUDE_DIRS}")
message(STATUS "Python libraries: ${Python_LIBRARIES}")
