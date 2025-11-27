# Select highest or lowest points

Select and retained only highest or lowest points per grid cell

## Usage

``` r
filter_with_grid(res, operator = "min", filter = "")
```

## Arguments

- res:

  numeric. The resolution of the grid

- operator:

  string. Can be min or max to retain lowest or highest points

- filter:

  the 'filter' argument allows filtering of the point-cloud to work with
  points of interest. For a given stage when a filter is applied, only
  the points that meet the criteria are processed. The most common
  strings are `Classification == 2"`, `"Z > 2"`, `"Intensity < 100"`.
  For more details see
  [filters](https://r-lidar.github.io/lasR/reference/filters.md).
