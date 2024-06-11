#' Metric engine
#'
#' The metric engine is an internal tool that allow to derive any metric from a set of points by parsing a
#' string. It is used by \link{rasterize}, \link{summarise} as well as other functions. Each string
#' is composed of two parts separated by an underscore. The first part is the attribute on which the
#' metric must be computed (e.g., z, intensity, classification). The second part is the name of the
#' metric (e.g., `mean`, `sd`, `cv`). A string thus typically looks like `"z_max"`, `"intensity_min"`,
#' `"z_mean"`, `"classification_mode"`. For more details see the sections 'Attribute' and 'Metrics'
#' respectively.
#'
#' **Be careful**: the engine supports any combination of `attribute_metric` strings. While they are
#' all computable, they are not all meaningful. For example, `c_mode` makes sense but not `z_mode`. Also,
#' all metrics are computed with 32-bit floating point accuracy, so `x_mean` or `y_sum` might be
#' slightly inaccurate, but anyway, these metrics are not supposed to be useful.
#'
#' @section Attribute:
#' The available attributes are accessible via a single letter or via their lowercase name: t - gpstime,
#' a - angle, i - intensity, n - numberofreturns, r - returnnumber, c - classification,
#' s - synthetic, k - keypoint, w - withheld, o - overlap (format 6+), u - userdata, p - pointsourceid,
#' e - edgeofflightline, d - scandirectionflag, R - red, G - green, B - blue, N - nir.\cr
#' **Be careful** to the typos: attributes are non failing features. If the attribute does not exist `NaN`
#' is returned. Thus `intesity_mean` return `NaN` rather than failing.
#'
#' @section Metrics:
#' The available metric names are: `count`, `max`, `min`, `mean`, `median`, `sum`, `sd`, `cv`, `pX` (percentile), `aboveX`, and `mode`.
#' Some metrics have an attribute + name + a parameter `X`, such as `pX` where `X` can be substituted by a number.
#' Here, `z_pX` represents the Xth percentile; for instance, `z_p95` signifies the 95th
#' percentile of z. `z_aboveX` corresponds to the percentage of points above `X` (sometimes called canopy cover).\cr\cr
#' It is possible to call a metric without the name of the attribute. In this case, z is the default. e.g. `mean` equals `z_mean`
#'
#' @section Extrabytes attribute:
#' The core attributes are x, y, z, classification, intensity, and so on. Some point clouds have extra
#' attributes called extrabytes attributes. In this case, metrics can be derived the same way using
#' the names of the extra attributes. Be careful of typos. The attributes are not checked internally because
#' of the extrabytes attributes. For example, if a user requests: `ntensity_mean`, this could be a typo
#' or the name of an extra attribute. Because extrabytes are never failing, `ntensity_mean` will return `NaN`
#' rather than an error.
#'
#' @examples
#' metrics = c("z_max", "i_min", "r_mean", "n_median", "z_sd", "c_sd", "t_cv", "u_sum", "z_p95")
#' f <- system.file("extdata", "Example.las", package="lasR")
#' p <- summarise(metrics = metrics)
#' r <- rasterize(5, operators = metrics)
#' ans <- exec(p+r, on = f)
#' ans$summary$metrics
#' ans$rasterize
#' @md
#' @name metric_engine
#' @rdname metric_engine
NULL