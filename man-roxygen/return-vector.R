#' @returns This stage produces a vector. The path provided to `ofile` is expected to be `.gpkg` or
#' any other format supported by GDAL. Vector stages may produce geometries with Z coordinates.
#' Thus, it is discouraged to store them in formats with no 3D support, such as shapefiles.
