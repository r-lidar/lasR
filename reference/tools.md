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
#> rasterize (uid:0906d12e05c2)
#>   method : [max] 
#>   window : 1.00 
#>   res : 1.00 
#>   filter : [] 
#>   output : /tmp/Rtmp8O2r5T/file21d479708b9c.tif 
#> -----------
#> 
#> NULL
pipeline <- algo1 + algo2
print(pipeline)
#> -----------
#> rasterize (uid:0906d12e05c2)
#>   method : [max] 
#>   window : 1.00 
#>   res : 1.00 
#>   filter : [] 
#>   output : /tmp/Rtmp8O2r5T/file21d479708b9c.tif 
#> -----------
#> rasterize (uid:05bf2c193f03)
#>   method : [min] 
#>   window : 4.00 
#>   res : 4.00 
#>   filter : [] 
#>   output : /tmp/Rtmp8O2r5T/file21d4eeb651f.tif 
#> -----------
#> 
#> NULL
```
