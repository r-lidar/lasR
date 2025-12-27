# Add RGB attributes to a LAS file

Modifies the LAS format to convert into a format with RGB attributes.
Values are zeroed: the underlying point cloud is edited to be
transformed in a format that supports RGB. RGB can be populated later in
another stage. If the point cloud already has RGB, nothing happens, RGB
values are preserved.

## Usage

``` r
add_rgb()
```

## Value

If this stage transforms the point cloud in the pipeline it returns
nothing. Otherwise it returns the R object returned by the function
'fun'

## Examples

``` r
f <- system.file("extdata", "Example.las", package="lasR")

pipeline <- add_rgb() + write_las()
exec(pipeline, on = f)
#> [1] "/tmp/RtmpedO4G2/Example.las"
```
