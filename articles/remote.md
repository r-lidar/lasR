# Remote files

## Working with Remote Files

All operations demonstrated in the
[tutorial](https://r-lidar.github.io/lasR/articles/tutorial.md) also
apply to remote files. Thanks to GDAL’s virtual file system, `lasR` can
stream data directly from remote sources without requiring you to
download the entire file beforehand.

However, working with remote data introduces some constraints:

- Network latency can slow down access
- Random reads may be less efficient than local disk access

It is **strongly recommended** to use cloud-optimized formats (COPC)
with proper spatial or depth queries. This format is designed for
efficient partial access over the network.

## Protocols supported

| Prefix       | Protocol                         | Authentication                                                |
|--------------|----------------------------------|---------------------------------------------------------------|
| `/vsicurl/`  | HTTP/HTTPS                       | None, or `.netrc`                                             |
| `/vsis3/`    | Amazon S3                        | `AWS_ACCESS_KEY_ID` + `AWS_SECRET_ACCESS_KEY` env vars        |
| `/vsigs/`    | Google Cloud Storage             | `GOOGLE_APPLICATION_CREDENTIALS` env var                      |
| `/vsiaz/`    | Azure Blob Storage               | `AZURE_STORAGE_ACCOUNT` + `AZURE_STORAGE_ACCESS_KEY` env vars |
| `/vsiadls/`  | Azure Data Lake Storage Gen2     | Same as `/vsiaz/`                                             |
| `/vsioss/`   | Alibaba Cloud OSS                | `OSS_ACCESS_KEY_ID` + `OSS_SECRET_ACCESS_KEY` env vars        |
| `/vsiswift/` | OpenStack Swift (OVH, Rackspace) | `SWIFT_AUTH_TOKEN` + `SWIFT_STORAGE_URL` env vars             |

## Examples

``` r
url <- "https://s3.amazonaws.com/hobu-lidar/autzen-classified.copc.laz"
pipeline <-  reader_circles(637368.8, 851944.8, 15) + write_las()
ans <- exec(pipeline, on = url)
```

To deal with a collection of tiled remotes files we recommend using a
VPC file that indexes remotes files.
