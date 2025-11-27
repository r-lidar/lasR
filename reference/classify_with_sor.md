# Classify noise points

Classify points using the Statistical Outliers Removal (SOR) methods
first described in the PCL library and also implemented in CloudCompare
(see references). For each point, it computes the mean distance to all
its k-nearest neighbors. The points that are farther than the average
distance plus a number of times (multiplier) the standard deviation are
considered noise.

## Usage

``` r
classify_with_sor(k = 8, m = 6, class = 18L)
```

## Arguments

- k:

  numeric. The number of neighbours

- m:

  numeric. Multiplier. The maximum distance will be: ⁠avg distance + m \*
  std deviation⁠

- class:

  integer. The class to assign to the points that match the condition.

## Value

This stage transforms the point cloud in the pipeline. It consequently
returns nothing.
