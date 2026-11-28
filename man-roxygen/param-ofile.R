#' @param ofile character. Full outputs are always stored on disk. If \code{ofile = ""}
#' then the stage will not store the result on disk and will return nothing. It will however
#' hold partial output results temporarily in memory. This is useful for stage that are only
#' intermediate stage. If \code{ofile} contains a wildcard \code{*} (e.g. \code{"folder/*_dtm.tif"})
#' one file is written per chunk and \code{*} is replaced by the name of the chunk. For a raster
#' stage, if \code{ofile} is a \code{.vrt} file (e.g. \code{"folder/dtm.vrt"}) one file is written
#' per chunk, named after the VRT (\code{"folder/<chunk>_dtm.tif"}), and they are combined into a
#' GDAL virtual raster at the end.
