#!/usr/bin/env python3
"""
Test all stage creation functions in the pylasr API
"""

import os
import sys
import unittest

# Add the parent directory to sys.path to import pylasr
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

try:
    import pylasr

    PYLASR_AVAILABLE = True
except ImportError as e:
    PYLASR_AVAILABLE = False
    IMPORT_ERROR = str(e)


class TestBasicOperations(unittest.TestCase):
    """Test basic operation stage functions"""

    def setUp(self):
        if not PYLASR_AVAILABLE:
            self.skipTest("pylasr not available")

    def test_info(self):
        """Test info pipeline creation"""
        pipeline = pylasr.info()
        self.assertIsInstance(pipeline, pylasr.Pipeline)

    def test_add_attribute(self):
        """Test add_attribute pipeline creation"""
        pipeline = pylasr.add_attribute(
            "double", "test_attr", "Test attribute", scale=1.0, offset=0.0
        )
        self.assertIsInstance(pipeline, pylasr.Pipeline)

    def test_add_rgb(self):
        """Test add_rgb pipeline creation"""
        pipeline = pylasr.add_rgb()
        self.assertIsInstance(pipeline, pylasr.Pipeline)


class TestClassificationFunctions(unittest.TestCase):
    """Test classification pipeline functions"""

    def setUp(self):
        if not PYLASR_AVAILABLE:
            self.skipTest("pylasr not available")

    def test_classify_with_sor(self):
        """Test Statistical Outlier Removal classification"""
        pipeline = pylasr.classify_with_sor(k=10, m=6, classification=18)
        self.assertIsInstance(pipeline, pylasr.Pipeline)

    def test_classify_with_ivf(self):
        """Test Isolated Voxel Filter classification"""
        pipeline = pylasr.classify_with_ivf(res=5.0, n=6, classification=18)
        self.assertIsInstance(pipeline, pylasr.Pipeline)

    def test_classify_with_csf(self):
        """Test Cloth Simulation Filter classification"""
        pipeline = pylasr.classify_with_csf(
            slope_smooth=False,
            class_threshold=0.5,
            cloth_resolution=0.5,
            rigidness=1,
            iterations=500,
            time_step=0.65,
            classification=2,
            filter=[""],
        )
        self.assertIsInstance(pipeline, pylasr.Pipeline)


class TestPointOperations(unittest.TestCase):
    """Test point operation pipeline functions"""

    def setUp(self):
        if not PYLASR_AVAILABLE:
            self.skipTest("pylasr not available")

    def test_delete_points(self):
        """Test delete_points pipeline creation"""
        pipeline = pylasr.delete_points(["Classification == 18"])
        self.assertIsInstance(pipeline, pylasr.Pipeline)

    def test_edit_attribute(self):
        """Test edit_attribute pipeline creation"""
        pipeline = pylasr.edit_attribute(["Classification == 1"], "Classification", 6)
        self.assertIsInstance(pipeline, pylasr.Pipeline)

    def test_remove_attribute(self):
        """Test remove_attribute pipeline creation"""
        pipeline = pylasr.remove_attribute("GPSTime")
        self.assertIsInstance(pipeline, pylasr.Pipeline)

    def test_sort_points(self):
        """Test sort_points pipeline creation"""
        pipeline = pylasr.sort_points(spatial=True)
        self.assertIsInstance(pipeline, pylasr.Pipeline)


class TestFilteringAndSampling(unittest.TestCase):
    """Test filtering and sampling pipeline functions"""

    def setUp(self):
        if not PYLASR_AVAILABLE:
            self.skipTest("pylasr not available")

    def test_filter_with_grid(self):
        """Test filter_with_grid pipeline creation"""
        pipeline = pylasr.filter_with_grid(1.0, "min", [""])
        self.assertIsInstance(pipeline, pylasr.Pipeline)

    def test_sampling_voxel(self):
        """Test sampling_voxel pipeline creation"""
        pipeline = pylasr.sampling_voxel(res=2.0, filter=[""], method="random")
        self.assertIsInstance(pipeline, pylasr.Pipeline)

    def test_sampling_pixel(self):
        """Test sampling_pixel pipeline creation"""
        pipeline = pylasr.sampling_pixel(
            res=2.0, filter=[""], method="random", use_attribute="Z"
        )
        self.assertIsInstance(pipeline, pylasr.Pipeline)

    def test_sampling_poisson(self):
        """Test sampling_poisson pipeline creation"""
        pipeline = pylasr.sampling_poisson(distance=2.0, filter=[""])
        self.assertIsInstance(pipeline, pylasr.Pipeline)


class TestGeometricAnalysis(unittest.TestCase):
    """Test geometric analysis pipeline functions"""

    def setUp(self):
        if not PYLASR_AVAILABLE:
            self.skipTest("pylasr not available")

    def test_geometry_features(self):
        """Test geometry_features pipeline creation"""
        pipeline = pylasr.geometry_features(k=10, r=1.0, features="eigen_values")
        self.assertIsInstance(pipeline, pylasr.Pipeline)

    def test_local_maximum(self):
        """Test local_maximum pipeline creation"""
        pipeline = pylasr.local_maximum(
            ws=3.0, min_height=2.0, filter=[""], ofile="", use_attribute="Z"
        )
        self.assertIsInstance(pipeline, pylasr.Pipeline)

    def test_triangulate(self):
        """Test triangulate pipeline creation"""
        pipeline = pylasr.triangulate(
            max_edge=0.0, filter=[""], ofile="", use_attribute="Z"
        )
        self.assertIsInstance(pipeline, pylasr.Pipeline)

    def test_hull(self):
        """Test hull pipeline creation"""
        pipeline = pylasr.hull(ofile="")
        self.assertIsInstance(pipeline, pylasr.Pipeline)


class TestRasterization(unittest.TestCase):
    """Test rasterization pipeline functions"""

    def setUp(self):
        if not PYLASR_AVAILABLE:
            self.skipTest("pylasr not available")

    def test_rasterize(self):
        """Test rasterize pipeline creation"""
        pipeline = pylasr.rasterize(
            res=1.0,
            window=1.0,
            operators=["max"],
            filter=[""],
            ofile="test.tif",
            default_value=-99999.0,
        )
        self.assertIsInstance(pipeline, pylasr.Pipeline)


class TestDataLoading(unittest.TestCase):
    """Test data loading pipeline functions"""

    def setUp(self):
        if not PYLASR_AVAILABLE:
            self.skipTest("pylasr not available")

    def test_load_raster(self):
        """Test load_raster pipeline creation"""
        pipeline = pylasr.load_raster("test.tif", band=1)
        self.assertIsInstance(pipeline, pylasr.Pipeline)

    def test_load_matrix(self):
        """Test load_matrix pipeline creation"""
        # 4x4 identity matrix for valid input
        matrix = [
            1.0,
            0.0,
            0.0,
            0.0,
            0.0,
            1.0,
            0.0,
            0.0,
            0.0,
            0.0,
            1.0,
            0.0,
            0.0,
            0.0,
            0.0,
            1.0,
        ]
        pipeline = pylasr.load_matrix(matrix, check=True)
        self.assertIsInstance(pipeline, pylasr.Pipeline)


class TestReaders(unittest.TestCase):
    """Test reader pipeline functions"""

    def setUp(self):
        if not PYLASR_AVAILABLE:
            self.skipTest("pylasr not available")

    def test_reader_coverage(self):
        """Test reader_coverage pipeline creation"""
        pipeline = pylasr.reader_coverage(filter=[""], select="*", copc_depth=-1)
        self.assertIsInstance(pipeline, pylasr.Pipeline)

    def test_reader_circles(self):
        """Test reader_circles pipeline creation"""
        pipeline = pylasr.reader_circles(
            xc=[100.0, 200.0],
            yc=[100.0, 200.0],
            r=[50.0, 50.0],
            filter=[""],
            select="*",
            copc_depth=-1,
        )
        self.assertIsInstance(pipeline, pylasr.Pipeline)

    def test_reader_rectangles(self):
        """Test reader_rectangles pipeline creation"""
        pipeline = pylasr.reader_rectangles(
            xmin=[0.0, 100.0],
            ymin=[0.0, 100.0],
            xmax=[50.0, 150.0],
            ymax=[50.0, 150.0],
            filter=[""],
            select="*",
            copc_depth=-1,
        )
        self.assertIsInstance(pipeline, pylasr.Pipeline)


class TestCRSOperations(unittest.TestCase):
    """Test CRS operation pipeline functions"""

    def setUp(self):
        if not PYLASR_AVAILABLE:
            self.skipTest("pylasr not available")

    def test_set_crs_epsg(self):
        """Test set_crs_epsg pipeline creation"""
        pipeline = pylasr.set_crs_epsg(4326)
        self.assertIsInstance(pipeline, pylasr.Pipeline)

    def test_set_crs_wkt(self):
        """Test set_crs_wkt pipeline creation"""
        wkt = 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]]]'
        pipeline = pylasr.set_crs_wkt(wkt)
        self.assertIsInstance(pipeline, pylasr.Pipeline)


class TestWriters(unittest.TestCase):
    """Test writer pipeline functions"""

    def setUp(self):
        if not PYLASR_AVAILABLE:
            self.skipTest("pylasr not available")

    def test_write_las(self):
        """Test write_las pipeline creation"""
        pipeline = pylasr.write_las("output.las", filter=[""], keep_buffer=False)
        self.assertIsInstance(pipeline, pylasr.Pipeline)

    @unittest.skip("COPC validation issue - skip for now")
    def test_write_copc(self):
        """Test write_copc pipeline creation"""
        pipeline = pylasr.write_copc(
            "output.copc.laz",
            filter=[""],
            keep_buffer=False,
            max_depth=-1,
            density="dense",
        )
        self.assertIsInstance(pipeline, pylasr.Pipeline)

    def test_write_pcd(self):
        """Test write_pcd pipeline creation"""
        pipeline = pylasr.write_pcd("output.pcd", binary=True)
        self.assertIsInstance(pipeline, pylasr.Pipeline)

    def test_write_vpc(self):
        """Test write_vpc pipeline creation"""
        pipeline = pylasr.write_vpc(
            "catalog.vpc", absolute_path=False, use_gpstime=False
        )
        self.assertIsInstance(pipeline, pylasr.Pipeline)

    def test_write_lax(self):
        """Test write_lax pipeline creation"""
        pipeline = pylasr.write_lax(embedded=False, overwrite=False)
        self.assertIsInstance(pipeline, pylasr.Pipeline)


class TestControlFlow(unittest.TestCase):
    """Test control flow pipeline functions"""

    def setUp(self):
        if not PYLASR_AVAILABLE:
            self.skipTest("pylasr not available")

    def test_stop_if_outside(self):
        """Test stop_if_outside pipeline creation"""
        pipeline = pylasr.stop_if_outside(xmin=0.0, ymin=0.0, xmax=100.0, ymax=100.0)
        self.assertIsInstance(pipeline, pylasr.Pipeline)

    def test_stop_if_chunk_id_below(self):
        """Test stop_if_chunk_id_below pipeline creation"""
        pipeline = pylasr.stop_if_chunk_id_below(index=5)
        self.assertIsInstance(pipeline, pylasr.Pipeline)


class TestStatistics(unittest.TestCase):
    """Test statistics pipeline functions"""

    def setUp(self):
        if not PYLASR_AVAILABLE:
            self.skipTest("pylasr not available")

    def test_summarise(self):
        """Test summarise pipeline creation"""
        pipeline = pylasr.summarise(zwbin=2.0, iwbin=50.0, metrics=[], filter=[""])
        self.assertIsInstance(pipeline, pylasr.Pipeline)


class TestNonAPIFunctions(unittest.TestCase):
    """Test non-API functions (testing/debugging)"""

    def setUp(self):
        if not PYLASR_AVAILABLE:
            self.skipTest("pylasr not available")

    def test_nothing(self):
        """Test nothing pipeline creation"""
        pipeline = pylasr.nothing(read=False, stream=False, loop=False)
        self.assertIsInstance(pipeline, pylasr.Pipeline)


if __name__ == "__main__":
    unittest.main(verbosity=2)
