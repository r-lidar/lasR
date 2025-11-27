# Add attributes to a point cloud

Modify the memory layout of the point cloud to add an attributes to a
point cloud. Values are zeroed: the underlying point cloud is edited to
support a new extrabyte attribute. This new attribute can be populated
later in another stage

## Usage

``` r
add_extrabytes(data_type, name, description, scale = 1, offset = 0)

remove_attribute(name)
```

## Arguments

- data_type:

  character. The data type of the extra bytes attribute. Can be "uchar",
  "char", "ushort", "short", "uint", "int", "uint64", "int64", "float",
  "double".

- name:

  character. The name of the extra bytes attribute to add or remove to
  the file.

- description:

  character. A short description of the extra bytes attribute to add to
  the file (32 characters).

- scale, offset:

  numeric. The scale and offset of the data. See LAS specification.
  Leave unchanged if not working with LAS files.

## Value

This stage transforms the point cloud in the pipeline. It consequently
returns nothing.

## Examples

``` r
f <- system.file("extdata", "Example.las", package = "lasR")
fun <- function(data) { data$RAND <- runif(nrow(data), 0, 100); return(data) }
pipeline <- reader() +
  add_extrabytes("float", "RAND", "Random numbers") +
  callback(fun, expose = "xyz")
exec(pipeline, on = f)
#> NULL
```
