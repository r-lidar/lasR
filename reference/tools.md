# Tools inherited from base R

Tools inherited from base R

## Usage

``` r
# S3 method for class 'PipelinePtr'
print(x, ...)

# S3 method for class 'PipelinePtr'
e1 + e2

# S3 method for class 'PipelinePtr'
x[[i, ...]]
```

## Arguments

- x, e1, e2:

  lasR objects

- ...:

  lasR objects. Is equivalent to +

- i:

  index

## Examples

``` r
algo1 <- rasterize(1, "max")
algo2 <- rasterize(4, "min")
print(algo1)
#> -----------
#> rasterize (uid:fc270e33f3d6)
#>   method : [max] 
#>   window : 1.00 
#>   res : 1.00 
#>   filter : [] 
#>   output : /tmp/Rtmpq3MByp/file2408427c4184.tif 
#> -----------
#> 
#> NULL
pipeline <- algo1 + algo2
print(pipeline)
#> -----------
#> rasterize (uid:fc270e33f3d6)
#>   method : [max] 
#>   window : 1.00 
#>   res : 1.00 
#>   filter : [] 
#>   output : /tmp/Rtmpq3MByp/file2408427c4184.tif 
#> -----------
#> rasterize (uid:7709c33d06be)
#>   method : [min] 
#>   window : 4.00 
#>   res : 4.00 
#>   filter : [] 
#>   output : /tmp/Rtmpq3MByp/file24083c1922ae.tif 
#> -----------
#> 
#> NULL
```
