# Process the pipeline

Process the pipeline. Every other functions of the package do nothing.
This function must be called on a pipeline in order to actually process
the point-cloud. To process in parallel using multiple cores, refer to
the
[multithreading](https://r-lidar.github.io/lasR/reference/multithreading.md)
page.

## Usage

``` r
exec(pipeline, on, with = NULL, ...)
```

## Arguments

- pipeline:

  a pipeline. A serie of stages called in order

- on:

  Can be the paths of the files to use, the path of the folder in which
  the files are stored, the path to a [virtual point
  cloud](https://www.lutraconsulting.co.uk/blog/2023/06/08/virtual-point-clouds/)
  file or a `data.frame` containing the point cloud. It supports also a
  `LAScatalog` or a `LAS` objects from `lidR`. It supports PCD, LAS, LAZ
  file formats.

- with:

  list. A list of options to control how the pipeline is executed. This
  includes options to control parallel processing, progress bar display,
  tile buffering and so on. See
  [set_exec_options](https://r-lidar.github.io/lasR/reference/set_exec_options.md)
  for more details on the available options.

- ...:

  The processing options can be explicitly named and passed outside the
  `with` argument. See
  [set_exec_options](https://r-lidar.github.io/lasR/reference/set_exec_options.md)

## See also

[multithreading](https://r-lidar.github.io/lasR/reference/multithreading.md)
[set_exec_options](https://r-lidar.github.io/lasR/reference/set_exec_options.md)

## Examples

``` r
if (FALSE) { # \dontrun{
f <- paste0(system.file(package="lasR"), "/extdata/bcts/")
f <- list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

read <- reader_las()
tri <- triangulate(15)
dtm <- rasterize(5, tri)
lmf <- local_maximum(5)
met <- rasterize(2, "imean")
pipeline <- read + tri + dtm + lmf + met
ans <- exec(pipeline, on = f, with = list(progress = TRUE))
} # }
```
