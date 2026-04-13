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
#> rasterize (uid:0161ba75de97)
#>   method : [max] 
#>   window : 1.00 
#>   res : 1.00 
#>   filter : [] 
#>   output : /tmp/Rtmpj40xB7/file22abeb2e1ef.tif 
#> -----------
#> 
#> NULL
pipeline <- algo1 + algo2
print(pipeline)
#> -----------
#> rasterize (uid:0161ba75de97)
#>   method : [max] 
#>   window : 1.00 
#>   res : 1.00 
#>   filter : [] 
#>   output : /tmp/Rtmpj40xB7/file22abeb2e1ef.tif 
#> -----------
#> rasterize (uid:7b99ff3b7a12)
#>   method : [min] 
#>   window : 4.00 
#>   res : 4.00 
#>   filter : [] 
#>   output : /tmp/Rtmpj40xB7/file22ab440c328a.tif 
#> -----------
#> 
#> NULL
```
