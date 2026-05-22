# Classify noise points

Classify points using Isolated Voxel Filter (IVF). The stage identifies
points that have only a few other points in their surrounding 3 x 3 x 3
= 27 voxels and edits the points to assign a target classification. Used
with class 18, it classifies points as noise. This stage modifies the
point cloud in the pipeline but does not produce any output.

## Usage

``` r
classify_with_ivf(res = 5, n = 6L, class = 18L, filter = "")
```

## Arguments

- res:

  numeric. Resolution of the voxels. Can be a vector of 3 for x,y and z
  resolutions

- n:

  integer. The maximal number of 'other points' in the 27 voxels.

- class:

  integer. The class to assign to the points that match the condition.

- filter:

  the 'filter' argument allows filtering of the point-cloud to work with
  points of interest. For a given stage when a filter is applied, only
  the points that meet the criteria are processed. The most common
  strings are `Classification == 2"`, `"Z > 2"`, `"Intensity < 100"`.
  For more details see
  [filters](https://r-lidar.github.io/lasR/reference/filters.md).

## Value

This stage transforms the point cloud in the pipeline. It consequently
returns nothing.
