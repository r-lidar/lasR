# Write a Virtual Point Cloud

Borrowing the concept of virtual rasters from GDAL, the VPC file format
references other point cloud files in virtual point cloud (VPC)

## Usage

``` r
write_vpc(ofile, absolute_path = FALSE, use_gpstime = FALSE)
```

## Arguments

- ofile:

  character. The file path with extension .vpc where to write the
  virtual point cloud file

- absolute_path:

  boolean. The absolute path to the files is stored in the tile index
  file.

- use_gpstime:

  logical. To fill the datetime attribute in the VPC file, it uses the
  year and day of year recorded in the header. These attributes are
  usually NOT relevant. They are often zeroed and the official
  signification of these attributes corresponds to the creation of the
  LAS file. There is no guarantee that this date corresponds to the
  acquisition date. If `use_gpstime = TRUE`, it will use the gpstime of
  **the first point** recorded in each file to compute the day and year
  of acquisition. This works only if the GPS time is recorded as
  Adjusted Standard GPS Time and not with GPS Week Time.

## References

<https://www.lutraconsulting.co.uk/blog/2023/06/08/virtual-point-clouds/>  
<https://github.com/PDAL/wrench/blob/main/vpc-spec.md>

## Examples

``` r
if (FALSE) { # \dontrun{
pipeline = write_vpc("folder/dataset.vpc")
exec(pipeline, on = "folder")
} # }
```
