# Classify noise points

Classify points using Isolated Point Filter (IPF). The stage identifies
points that have only a few other points in their surrounding sphere
neighborhood and edits the points to assign a target classification.
Used with class 18, it classifies points as noise. With a r = 1 and n =
0: if a point has 0 neighbor within a radius of 1 m it is reclassified.
With a r = 2 and n = 1: if a point has 0 or 1 neighbor within a radius
of 2 m it is reclassified. This stage modifies the point cloud in the
pipeline but does not produce any output.

## Usage

``` r
classify_with_ipf(r = 1, n = 0L, class = 18L)
```

## Arguments

- r:

  numeric. Radius of the sphere.

- n:

  integer. The maximal number of 'other points' in the sphere. Less than
  that and the point is reclassified

- class:

  integer. The class to assign to the points that match the condition.

## Value

This stage transforms the point cloud in the pipeline. It consequently
returns nothing.
