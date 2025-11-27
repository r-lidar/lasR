# Set global processing options

Set global processing options for the
[exec](https://r-lidar.github.io/lasR/reference/exec.md) function. By
default, pipelines are executed without a progress bar, processing one
file at a time sequentially. The following options can be passed to the
[`exec()`](https://r-lidar.github.io/lasR/reference/exec.md) function in
four ways. See details.

## Usage

``` r
set_exec_options(
  ncores = NULL,
  progress = NULL,
  buffer = NULL,
  chunk = NULL,
  ...
)

unset_exec_option()
```

## Arguments

- ncores:

  An object returned by one of
  [`sequential()`](https://r-lidar.github.io/lasR/reference/multithreading.md),
  [`concurrent_points()`](https://r-lidar.github.io/lasR/reference/multithreading.md),
  [`concurrent_files()`](https://r-lidar.github.io/lasR/reference/multithreading.md),
  or
  [`nested()`](https://r-lidar.github.io/lasR/reference/multithreading.md).
  See
  [multithreading](https://r-lidar.github.io/lasR/reference/multithreading.md).
  If `NULL` the default is `concurrent_points(half_cores())`. If a
  simple integer is provided it corresponds to
  `concurrent_files(ncores)`.

- progress:

  boolean. Displays a progress bar.

- buffer:

  numeric. Each file is read with a buffer. The default is NULL, which
  does not mean that the file won't be buffered. It means that the
  internal routine knows if a buffer is needed and will pick the
  greatest value between the internal suggestion and this value.

- chunk:

  numeric. By default, the collection of files is processed by file
  (`chunk = NULL` or `chunk = 0`). It is possible to process in
  arbitrary-sized chunks. This is useful for e.g., processing
  collections with large files or processing a massive `copc` file.

- ...:

  Other internal options not exposed to users.

## Details

There are 4 ways to pass processing options, and it is important to
understand the precedence rules:  
  
The first option is by explicitly naming each option. This option is
deprecated and used for convenience and backward compatibility.

    exec(pipeline, on = f, progress = TRUE, ncores = 8)

The second option is by passing a `list` to the `with` argument. This
option is more explicit and should be preferred. The `with` argument
takes precedence over the explicit arguments.

    exec(pipeline, on = f, with = list(progress = TRUE, chunk = 500))

The third option is by using a `LAScatalog` from the `lidR` package. A
`LAScatalog` already carries some processing options that are respected
by the `lasR` package. The options from a `LAScatalog` take precedence.

    exec(pipeline, on = ctg, ncores = 4)

The last option is by setting global processing options. This has global
precedence and is mainly intended to provide a way for users to override
options if they do not have access to the
[`exec()`](https://r-lidar.github.io/lasR/reference/exec.md) function.
This may happen when a developer creates a function that executes a
pipeline internally, and users cannot provide any options.

    set_exec_options(progress = TRUE, ncores = concurrent_files(2))
    exec(pipeline, on = f)

## See also

[multithreading](https://r-lidar.github.io/lasR/reference/multithreading.md)
