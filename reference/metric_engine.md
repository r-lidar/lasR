# Metric engine

The metric engine is an internal tool that allow to derive any metric
from a set of points by parsing a string. It is used by
[rasterize](https://r-lidar.github.io/lasR/reference/rasterize.md),
[summarise](https://r-lidar.github.io/lasR/reference/summarise.md) as
well as other functions. Each string is composed of two parts separated
by an underscore. The first part is the attribute on which the metric
must be computed (e.g., z, intensity, classification). The second part
is the name of the metric (e.g., `mean`, `sd`, `cv`). A string thus
typically looks like `"z_max"`, `"intensity_min"`, `"z_mean"`,
`"classification_mode"`. For more details see the sections 'Attribute'
and 'Metrics' respectively.

## Details

**Be careful**: the engine supports any combination of
`attribute_metric` strings. While they are all computable, they are not
all meaningful. For example, `c_mode` makes sense but not `z_mode`.
Also, all metrics are computed with 32-bit floating point accuracy, so
`x_mean` or `y_sum` might be slightly inaccurate, but anyway, these
metrics are not supposed to be useful.

## Attribute

The available attributes are accessible via their name. Some standard
attribute have a shortcut by using a single letter: t - gpstime, a -
angle, i - intensity, n - numberofreturns, r - returnnumber, c -
classification, u - userdata, p - pointsourceid, e - edgeofflightline,
d - scandirectionflag, R - red, G - green, B - blue, N - nir.  
**Be careful** to the typos: attributes are non failing features. If the
attribute does not exist `NaN` is returned. Thus `intesity_mean` return
`NaN` rather than failing.

## Metrics

The available metric names are: `count`, `max`, `min`, `mean`, `median`,
`sum`, `sd`, `cv`, `pX` (percentile), `aboveX`, `mode`, `kurt`
(kurtosis), `skew` (skewness). Some metrics have an attribute + name + a
parameter `X`, such as `pX` where `X` can be substituted by a number.
Here, `z_pX` represents the Xth percentile; for instance, `z_p95`
signifies the 95th percentile of z. `z_aboveX` corresponds to the
percentage of points above `X` (sometimes called canopy cover).  
  
It is possible to call a metric without the name of the attribute. In
this case, z is the default. e.g. `mean` equals `z_mean`

## Extra attribute

The core attributes natively supported are x, y, z, classification,
intensity, and so on. Some point clouds have other may have other
attributes. In this case, metrics can be derived the same way using the
names of the attributes. Be careful of typos. The attributes existance
are not checked internally because. For example, if a user requests:
`ntensity_mean`, this could be a typo or the name of an extra attribute.
Because extra attribute are never failing, `ntensity_mean` will return
`NaN` rather than an error.

## Examples

``` r
metrics = c("z_max", "i_min", "r_mean", "n_median", "z_sd", "c_sd", "t_cv", "u_sum", "z_p95")
f <- system.file("extdata", "Example.las", package="lasR")
p <- summarise(metrics = metrics)
r <- rasterize(5, operators = metrics)
ans <- exec(p+r, on = f)
ans$summary$metrics
#>        c_sd i_min n_median   r_mean         t_cv u_sum   z_max    z_p95
#> 1 0.3051286    27        1 1.133333 4.466148e-07   960 978.345 978.2653
#>       z_sd
#> 1 1.459199
ans$rasterize
#> class       : SpatRaster 
#> size        : 1, 4, 9  (nrow, ncol, nlyr)
#> resolution  : 5, 5  (x, y)
#> extent      : 339000, 339020, 5248000, 5248005  (xmin, xmax, ymin, ymax)
#> coord. ref. : NAD83 / UTM zone 17N (EPSG:26917) 
#> source      : file21b8d274e6f.tif 
#> names       : z_max, i_min, r_mean, n_median, z_sd, c_sd, ... 
```
