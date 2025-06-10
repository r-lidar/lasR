import uuid
import os
import tempfile
import json
from pathlib import Path
from typing import Union, List, Optional, Any, Dict
from dataclasses import dataclass, field
from enum import Enum
from contextlib import contextmanager


def generate_uid(size: int = 8) -> str:
    """Generate a unique identifier string."""
    return uuid.uuid4().hex[:size]


@dataclass
class LASRstage:
    """Represents a single processing stage in the LASR pipeline."""

    algoname: str
    output: str = ""
    filter: str = ""
    raster: bool = False
    vector: bool = False
    uid: str = field(default_factory=generate_uid)
    args: Dict[str, Any] = field(default_factory=dict)

    def set_attributes(
        self, raster: Optional[bool] = None, vector: Optional[bool] = None
    ) -> None:
        """Set raster and/or vector attributes."""
        if raster is not None:
            self.raster = raster
        if vector is not None:
            self.vector = vector

    def add_args(self, key: str, value: Any) -> None:
        """Add arbitrary data to the container."""
        self.args[key] = value

    def get_args(self, key: str) -> Any:
        """Retrieve data by key."""
        return self.args.get(key)

    def to_dict(self) -> Dict[str, Any]:
        """Convert the LASRstage instance to a dictionary with flattened args."""
        # Start with the fixed attributes
        data = {
            "algoname": self.algoname,
            "output": self.output,
            "filter": self.filter,
            "uid": self.uid,
        }
        # Add each key-value pair from args to the dictionary
        data.update(self.args)
        return data

    def __str__(self) -> str:
        """Provide a readable string representation of the instance."""
        lines = [
            "-" * 50,
            f"{self.algoname} (uid: {self.uid})",
            f"  output: {self.output}",
            f"  filter: {self.filter}",
            f"  raster: {self.raster}",
            f"  vector: {self.vector}",
        ]

        # Include named data
        if self.args:
            lines.append("  arguments:")
            for key, value in self.args.items():
                lines.append(f"    {key}: {value}")

        lines.append("-" * 50)
        return "\n".join(lines)

    def __repr__(self) -> str:
        """Provide a more detailed string representation for debugging."""
        return (
            f"LASRstage(algoname={self.algoname!r}, output={self.output!r}, "
            f"filter={self.filter!r}, uid={self.uid!r}, raster={self.raster!r}, "
            f"vector={self.vector!r}, args={self.args!r})"
        )

    @property
    def is_raster_stage(self) -> bool:
        """Check if this is a raster-producing stage."""
        return self.raster

    @property
    def is_vector_stage(self) -> bool:
        """Check if this is a vector-producing stage."""
        return self.vector


@dataclass
class LASRpipeline:
    """Represents a pipeline of LASR processing stages."""

    stages: List[LASRstage] = field(default_factory=list)

    def __post_init__(self):
        """Handle initialization with a single stage."""
        # This is handled by the factory functions now
        pass

    @classmethod
    def from_stage(cls, stage: LASRstage) -> "LASRpipeline":
        """Create a pipeline from a single stage."""
        if not isinstance(stage, LASRstage):
            raise TypeError("The provided argument must be an instance of LASRstage.")
        return cls(stages=[stage])

    def add_stage(self, stage: LASRstage) -> None:
        """Add a LASRstage to the pipeline."""
        if not isinstance(stage, LASRstage):
            raise TypeError("Only LASRstage instances can be added.")
        self.stages.append(stage)

    def __add__(self, other: "LASRpipeline") -> "LASRpipeline":
        """Concatenate two LASRpipeline instances."""
        if isinstance(other, LASRpipeline):
            return LASRpipeline(stages=self.stages + other.stages)
        return NotImplemented

    def to_json(self) -> str:
        """Convert the LASRpipeline instance to a JSON string with stages keyed by algoname."""
        stages_dict = {stage.algoname: stage.to_dict() for stage in self.stages}
        return json.dumps(stages_dict, indent=4)

    def __str__(self) -> str:
        """Provide a readable string representation of the pipeline."""
        if not self.stages:
            return "LASRpipeline (empty)"

        lines = [f"LASRpipeline with {len(self.stages)} stage(s):"]
        for i, stage in enumerate(self.stages, 1):
            lines.append(f"\n{i}. {stage.algoname} (uid: {stage.uid})")
            if stage.output:
                lines.append(f"   → output: {stage.output}")
            if stage.filter:
                lines.append(f"   → filter: {stage.filter}")

        return "\n".join(lines)

    def __repr__(self) -> str:
        """Provide a detailed string representation for debugging."""
        return f"LASRpipeline(stages={self.stages!r})"

    def __len__(self) -> int:
        """Return the number of stages in the pipeline."""
        return len(self.stages)

    def __iter__(self):
        """Make pipeline iterable over stages."""
        return iter(self.stages)

    @property
    def stage_names(self) -> List[str]:
        """Get list of algorithm names in the pipeline."""
        return [stage.algoname for stage in self.stages]


# Additional Pythonic improvements


class StageType(Enum):
    """Enumeration of stage output types."""

    RASTER = "raster"
    VECTOR = "vector"
    PROCESSING = "processing"


class LASRPipelineBuilder:
    """Builder pattern for constructing LASR pipelines."""

    def __init__(self):
        self._stages: List[LASRstage] = []

    def add_load_raster(
        self, file: Union[str, Path], band: int = 1
    ) -> "LASRPipelineBuilder":
        """Add a load_raster stage to the pipeline."""
        stage = load_raster(file, band).stages[0]
        self._stages.append(stage)
        return self

    def add_local_maximum(self, ws: float, **kwargs) -> "LASRPipelineBuilder":
        """Add a local_maximum stage to the pipeline."""
        stage = local_maximum(ws, **kwargs).stages[0]
        self._stages.append(stage)
        return self

    def build(self) -> LASRpipeline:
        """Build the final pipeline."""
        return LASRpipeline(stages=self._stages.copy())


@contextmanager
def pipeline_context():
    """Context manager for pipeline operations."""
    try:
        yield LASRPipelineBuilder()
    except Exception as e:
        print(f"Pipeline construction failed: {e}")
        raise


# Convenience functions for common patterns
def create_basic_processing_pipeline(
    input_file: Union[str, Path], output_file: Union[str, Path]
) -> LASRpipeline:
    """Create a basic processing pipeline with common stages."""
    return load_raster(input_file) + local_maximum(ws=5.0) + write_las(output_file)


# Type aliases for better readability
StageArgs = Dict[str, Any]
PipelineStages = List[LASRstage]


# Missing utility functions
def tempgpkg() -> str:
    """Generate a temporary GPKG file path."""
    return tempfile.mktemp(suffix=".gpkg")


def get_stage(stage: Union["LASRstage", "LASRpipeline"]) -> "LASRstage":
    """Extract a single stage from a pipeline or return the stage itself."""
    if isinstance(stage, LASRstage):
        return stage

    if isinstance(stage, LASRpipeline):
        if len(stage.stages) != 1:
            raise ValueError(
                f"Cannot input a complex pipeline. Pipeline has {len(stage.stages)} stages"
            )
        return stage.stages[0]


def _create_stage(
    algoname: str,
    output: str = "",
    filter: str = "",
    raster: bool = False,
    vector: bool = False,
    **kwargs,
) -> LASRpipeline:
    """Helper function to create a stage with common parameters."""
    stage = LASRstage(
        algoname=algoname, output=output, filter=filter, raster=raster, vector=vector
    )

    # Add all keyword arguments to stage.args
    stage.args.update(kwargs)

    return LASRpipeline.from_stage(stage)


def load_raster(file: Union[str, Path], band: int = 1) -> LASRpipeline:
    """Load a raster file for later use in the pipeline.

    Args:
        file: Path to the raster file
        band: Band number to load (default: 1)

    Returns:
        LASRpipeline containing the load_raster stage
    """
    # Normalize the file path
    file_path = os.path.normpath(str(file))

    return _create_stage(algoname="load_raster", raster=True, file=file_path, band=band)


def local_maximum(
    ws: float,
    min_height: float = 2,
    filter: str = "",
    ofile: Optional[str] = None,
    use_attribute: str = "Z",
    record_attributes: bool = False,
) -> LASRpipeline:
    """Find local maximum points in the point cloud.

    Args:
        ws: Window size for local maximum detection
        min_height: Minimum height threshold
        filter: Point filter string
        ofile: Output file path (auto-generated if None)
        use_attribute: Attribute to use for detection
        record_attributes: Whether to record point attributes

    Returns:
        LASRpipeline containing the local_maximum stage
    """
    if ofile is None:
        ofile = tempgpkg()

    return _create_stage(
        algoname="local_maximum",
        output=ofile,
        filter=filter,
        vector=True,
        ws=ws,
        min_height=min_height,
        use_attribute=use_attribute,
        record_attributes=record_attributes,
    )


def local_maximum_raster(
    raster: Union[LASRstage, LASRpipeline],
    ws: float,
    min_height: float = 2,
    filter: str = "",
    ofile: Optional[str] = None,
) -> LASRpipeline:
    """Find local maximum points from a raster stage.

    Args:
        raster: Raster stage to process
        ws: Window size for local maximum detection
        min_height: Minimum height threshold
        filter: Point filter string
        ofile: Output file path (auto-generated if None)

    Returns:
        LASRpipeline containing the local_maximum stage
    """
    if ofile is None:
        ofile = tempgpkg()

    # Ensure the raster stage is valid
    raster_stage = get_stage(raster)
    if not raster_stage.is_raster_stage:
        raise ValueError("the stage must be a raster stage")

    return _create_stage(
        algoname="local_maximum",
        output=ofile,
        filter=filter,
        vector=True,
        connect=raster_stage.uid,
        ws=ws,
        min_height=min_height,
    )


def neighborhood_metrics(
    neighborhood: Union[LASRstage, LASRpipeline],
    metrics: List[str],
    k: int = 10,
    r: float = 0,
    ofile: Optional[str] = None,
) -> LASRpipeline:
    """Compute metrics for neighborhoods around points.

    Args:
        neighborhood: Neighborhood stage (must be local_maximum)
        metrics: List of metrics to compute
        k: Number of neighbors
        r: Search radius
        ofile: Output file path (auto-generated if None)

    Returns:
        LASRpipeline containing the neighborhood_metrics stage
    """
    if ofile is None:
        ofile = tempgpkg()

    nn = get_stage(neighborhood)

    # Validate that the neighborhood stage is a "local_maximum" stage
    if nn.algoname != "local_maximum":
        raise ValueError("the stage must be a local_maximum stage")

    return _create_stage(
        algoname="neighborhood_metrics",
        output=ofile,
        connect=nn.uid,
        k=k,
        r=r,
        metrics=metrics,
    )


def nothing(
    read: bool = False, stream: bool = False, loop: bool = False
) -> LASRpipeline:
    """Create a 'nothing' stage for testing or placeholder purposes.

    Args:
        read: Whether to read data
        stream: Whether to stream data
        loop: Whether to loop

    Returns:
        LASRpipeline containing the nothing stage
    """
    return _create_stage(algoname="nothing", read=read, stream=stream, loop=loop)


def reader_las_coverage(filter: str = "") -> LASRpipeline:
    """Create a LAS coverage reader stage.

    Args:
        filter: Point filter string

    Returns:
        LASRpipeline containing the reader_las stage
    """
    return _create_stage(algoname="reader_las", filter=filter)


def reader_las_circles(
    xc: List[float],
    yc: List[float],
    r: Union[float, List[float]],
    filter: str = "",
    **kwargs,
) -> LASRpipeline:
    """Create a LAS reader for circular regions.

    Args:
        xc: X coordinates of circle centers
        yc: Y coordinates of circle centers
        r: Radius (single value or list)
        filter: Point filter string
        **kwargs: Additional arguments

    Returns:
        LASRpipeline containing the reader_las stage
    """
    if len(xc) != len(yc):
        raise ValueError("xc and yc must have the same length")
    if isinstance(r, (int, float)):
        r = [r] * len(xc)
    elif len(r) != len(xc):
        raise ValueError("xc and r must have the same length if r is not scalar")

    # Use reader_las_coverage to create the initial stage
    pipeline = reader_las_coverage(filter)
    stage = pipeline.stages[0]
    stage.args["xcenter"] = xc
    stage.args["ycenter"] = yc
    stage.args["radius"] = r
    return pipeline


def reader_las_rectangles(
    xmin: List[float],
    ymin: List[float],
    xmax: List[float],
    ymax: List[float],
    filter: str = "",
) -> LASRpipeline:
    """Create a LAS reader for rectangular regions.

    Args:
        xmin: Minimum X coordinates
        ymin: Minimum Y coordinates
        xmax: Maximum X coordinates
        ymax: Maximum Y coordinates
        filter: Point filter string

    Returns:
        LASRpipeline containing the reader_las stage
    """
    if not (len(xmin) == len(ymin) == len(xmax) == len(ymax)):
        raise ValueError("xmin, ymin, xmax, and ymax must all have the same length")

    # Use reader_las_coverage to create the initial stage
    pipeline = reader_las_coverage(filter)
    stage = pipeline.stages[0]
    stage.args["xmin"] = xmin
    stage.args["ymin"] = ymin
    stage.args["xmax"] = xmax
    stage.args["ymax"] = ymax
    return pipeline


def region_growing(
    raster: Union[LASRstage, LASRpipeline],
    seeds: Union[LASRstage, LASRpipeline],
    th_tree: float = 2,
    th_seed: float = 0.45,
    th_cr: float = 0.55,
    max_cr: float = 20,
    ofile: Optional[str] = None,
) -> LASRpipeline:
    """Perform region growing segmentation.

    Args:
        raster: Raster stage for region growing
        seeds: Seeds stage for region growing
        th_tree: Tree threshold
        th_seed: Seed threshold
        th_cr: Crown ratio threshold
        max_cr: Maximum crown ratio
        ofile: Output file path (auto-generated if None)

    Returns:
        LASRpipeline containing the region_growing stage
    """
    raster_stage = get_stage(raster)
    seeds_stage = get_stage(seeds)

    if ofile is None:
        ofile = tempfile.mktemp(suffix=".tif")

    return _create_stage(
        algoname="region_growing",
        output=ofile,
        raster=True,
        connect1=seeds_stage.uid,
        connect2=raster_stage.uid,
        th_tree=th_tree,
        th_seed=th_seed,
        th_cr=th_cr,
        max_cr=max_cr,
    )


def sampling_voxel(res: float = 2, filter: str = "", **kwargs) -> LASRpipeline:
    """Create a voxel-based sampling stage.

    Args:
        res: Voxel resolution
        filter: Point filter string
        **kwargs: Additional arguments including shuffle_size

    Returns:
        LASRpipeline containing the sampling_voxel stage
    """
    shuffle_size = kwargs.get("shuffle_size", 2**31 - 1)

    return _create_stage(
        algoname="sampling_voxel", filter=filter, res=res, shuffle_size=shuffle_size
    )


def sampling_pixel(res: float = 2, filter: str = "", **kwargs) -> LASRpipeline:
    """Create a pixel-based sampling stage.

    Args:
        res: Pixel resolution
        filter: Point filter string
        **kwargs: Additional arguments including shuffle_size

    Returns:
        LASRpipeline containing the sampling_pixel stage
    """
    shuffle_size = kwargs.get("shuffle_size", 2**31 - 1)

    return _create_stage(
        algoname="sampling_pixel", filter=filter, res=res, shuffle_size=shuffle_size
    )


def sampling_poisson(distance: float = 2, filter: str = "", **kwargs) -> LASRpipeline:
    """Create a Poisson disk sampling stage.

    Args:
        distance: Minimum distance between points
        filter: Point filter string
        **kwargs: Additional arguments including shuffle_size

    Returns:
        LASRpipeline containing the sampling_poisson stage
    """
    shuffle_size = kwargs.get("shuffle_size", 1000)

    return _create_stage(
        algoname="sampling_poisson",
        filter=filter,
        distance=distance,
        shuffle_size=shuffle_size,
    )


def stop_if_outside(xmin: float, ymin: float, xmax: float, ymax: float) -> LASRpipeline:
    """Create a conditional stop stage based on bounding box.

    Args:
        xmin: Minimum X coordinate
        ymin: Minimum Y coordinate
        xmax: Maximum X coordinate
        ymax: Maximum Y coordinate

    Returns:
        LASRpipeline containing the stop_if stage
    """
    return _create_stage(
        algoname="stop_if",
        condition="outside_bbox",
        xmin=xmin,
        ymin=ymin,
        xmax=xmax,
        ymax=ymax,
    )


def stop_if_chunk_id_below(index: int) -> LASRpipeline:
    """Create a conditional stop stage based on chunk ID.

    Args:
        index: Chunk index threshold

    Returns:
        LASRpipeline containing the stop_if stage
    """
    return _create_stage(
        algoname="stop_if", condition="chunk_id_below", index=int(index)
    )


def sort_points(spatial: bool = True) -> LASRpipeline:
    """Create a point sorting stage.

    Args:
        spatial: Whether to use spatial sorting

    Returns:
        LASRpipeline containing the sort stage
    """
    return _create_stage(algoname="sort", spatial=spatial)


def summarise(
    zwbin: float = 2,
    iwbin: float = 50,
    metrics: Optional[List[str]] = None,
    filter: str = "",
) -> LASRpipeline:
    """Create a summary statistics stage.

    Args:
        zwbin: Z-value bin size
        iwbin: Intensity bin size
        metrics: List of metrics to compute
        filter: Point filter string

    Returns:
        LASRpipeline containing the summarise stage
    """
    return _create_stage(
        algoname="summarise",
        filter=filter,
        zwbin=zwbin,
        iwbin=iwbin,
        metrics=metrics if metrics is not None else [],
    )


def triangulate(
    max_edge: float = 0, filter: str = "", ofile: str = "", use_attribute: str = "Z"
) -> LASRpipeline:
    """Create a triangulation stage.

    Args:
        max_edge: Maximum edge length for triangulation
        filter: Point filter string
        ofile: Output file path
        use_attribute: Attribute to use for triangulation

    Returns:
        LASRpipeline containing the triangulate stage
    """
    return _create_stage(
        algoname="triangulate",
        output=ofile,
        filter=filter,
        vector=True,
        max_edge=max_edge,
        use_attribute=use_attribute,
    )


def transform_with(
    stage: Union[LASRstage, LASRpipeline],
    operator: str = "-",
    store_in_attribute: str = "",
) -> LASRpipeline:
    """Transform points using another stage (triangulation or raster).

    Args:
        stage: Stage to use for transformation (triangulation or raster)
        operator: Transformation operator
        store_in_attribute: Attribute to store result in

    Returns:
        LASRpipeline containing the transform_with stage
    """
    # Get the stage and validate it
    stage_obj = get_stage(stage)

    if not (stage_obj.algoname == "triangulate" or stage_obj.is_raster_stage):
        raise ValueError("the stage must be a triangulation or a raster stage")

    return _create_stage(
        algoname="transform_with",
        connect=stage_obj.uid,
        operator=operator,
        store_in_attribute=store_in_attribute,
    )


def write_las(
    ofile: Optional[Union[str, Path]] = None,
    filter: str = "",
    keep_buffer: bool = False,
) -> LASRpipeline:
    """Write point cloud to LAS file.

    Args:
        ofile: Output file path (auto-generated if None)
        filter: Point filter string
        keep_buffer: Whether to keep buffer points

    Returns:
        LASRpipeline containing the write_las stage
    """
    if ofile is None:
        ofile = str(Path(tempfile.gettempdir()) / "*.las")
    else:
        ofile = str(ofile)

    return _create_stage(
        algoname="write_las", filter=filter, output=ofile, keep_buffer=keep_buffer
    )


def write_vpc(
    ofile: Union[str, Path], absolute_path: bool = False, use_gpstime: bool = False
) -> LASRpipeline:
    """Write a Virtual Point Cloud file.

    Args:
        ofile: Output file path
        absolute_path: Whether to use absolute paths
        use_gpstime: Whether to use GPS time

    Returns:
        LASRpipeline containing the write_vpc stage
    """
    return _create_stage(
        algoname="write_vpc",
        output=str(ofile),
        absolute=absolute_path,
        use_gpstime=use_gpstime,
    )


def write_lax(embedded: bool = False, overwrite: bool = False) -> LASRpipeline:
    """Write LAX index files.

    Args:
        embedded: Whether to embed the index
        overwrite: Whether to overwrite existing files

    Returns:
        LASRpipeline containing the write_lax stage
    """
    return _create_stage(algoname="write_lax", embedded=embedded, overwrite=overwrite)
