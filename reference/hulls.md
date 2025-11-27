# Contour of a point cloud

This stage uses a Delaunay triangulation and computes its contour. The
contour of a strict Delaunay triangulation is the convex hull, but in
lasR, the triangulation has a `max_edge` argument. Thus, the contour
might be a convex hull with holes. Used without triangulation it returns
the bouding box of the points.

## Usage

``` r
hulls(mesh = NULL, ofile = tempgpkg())
```

## Arguments

- mesh:

  NULL or LASRalgorithm. A `triangulate` stage. If NULL take the
  bounding box of the header of each file.

- ofile:

  character. Full outputs are always stored on disk. If `ofile = ""`
  then the stage will not store the result on disk and will return
  nothing. It will however hold partial output results temporarily in
  memory. This is useful for stage that are only intermediate stage.

## Value

This stage produces a vector. The path provided to \`ofile\` is expected
to be \`.gpkg\` or any other format supported by GDAL. Vector stages may
produce geometries with Z coordinates. Thus, it is discouraged to store
them in formats with no 3D support, such as shapefiles.

## See also

[triangulate](https://r-lidar.github.io/lasR/reference/triangulate.md)

## Examples

``` r
f <- system.file("extdata", "Topography.las", package = "lasR")
read <- reader()
tri <- triangulate(20, filter = keep_ground())
contour <- hulls(tri)
pipeline <- read + tri + contour
ans <- exec(pipeline, on = f)
plot(ans)

```
