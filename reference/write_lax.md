# Write spatial indexing .lax files

Creates a .lax file for each `.las` or `.laz` file of the processed
datase. A .lax file contains spatial indexing information. Spatial
indexing drastically speeds up tile buffering and spatial queries. In
lasR, it is mandatory to have spatially indexed point clouds, either
using .lax files or .copc.laz files. If the processed file collection is
not spatially indexed, a `write_lax()` file will automatically be added
at the beginning of the pipeline (see Details).

## Usage

``` r
write_lax(embedded = FALSE, overwrite = FALSE)
```

## Arguments

- embedded:

  boolean. A .lax file is an auxiliary file that accompanies its
  corresponding las or laz file. A .lax file can also be embedded within
  a laz file to produce a single file.

- overwrite:

  boolean. This stage does not create a new spatial index if the
  corresponding point cloud already has a spatial index. If TRUE, it
  forces the creation of a new one. `copc.laz` files are never reindexed
  with `lax` files.

## Details

When this stage is added automatically by `lasR`, it is placed at the
beginning of the pipeline, and las/laz files are indexed **on-the-fly**
when they are used. The advantage is that users do not need to do
anything; it works transparently and does not delay the processing. The
drawback is that, under this condition, the stage cannot be run in
parallel. When this stage is explicitly added by the users, it can be
placed anywhere in the pipeline but will always be executed first before
anything else. All the files will be indexed first in parallel, and then
the actual processing will start. To avoid overthinking about how it
works, it is best and simpler to run `exec(write_lax(), on = files)` on
the non indexed point cloud before doing anything with the point cloud.

## Examples

``` r
if (FALSE) { # \dontrun{
exec(write_lax(), on = files)
} # }
```
