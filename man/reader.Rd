% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/stages.R
\name{reader}
\alias{reader}
\alias{reader_coverage}
\alias{reader_circles}
\alias{reader_rectangles}
\title{Initialize the pipeline}
\usage{
reader(x, filter = "", buffer = 0, ...)

reader_coverage(x, filter = "", buffer = 0)

reader_circles(x, xc, yc, r, filter = "", buffer = 0)

reader_rectangles(x, xmin, ymin, xmax, ymax, filter = "", buffer = 0)
}
\arguments{
\item{x}{Can be the paths of the files to use, the path of the folder in which the files are stored,
the path to a \href{https://www.lutraconsulting.co.uk/blog/2023/06/08/virtual-point-clouds/}{virtual point cloud}
file or a \code{data.frame} containing hte point cloud. It supports also a \code{LAScatalog} or a \code{LAS} objects
from \code{lidR}.}

\item{filter}{the 'filter' argument allows filtering of the point-cloud to work with points of
interest. The available filters are those from LASlib and can be found by running \link{filter_usage}.
For a given algorithm when a filter is applied, only the points that meet the criteria are processes.
The most common strings are "-keep_first", "-keep_class 2", "drop_z_below 2". For more details see
\link{filters}.}

\item{buffer}{numeric. Each file is read with a buffer. The default is 0, which does not mean that
the file won't be buffered. It means that the internal routine knows if a buffer is needed and will
pick the greatest value between the internal suggestion and this provided value.}

\item{...}{passed to other readers}

\item{xc, yc, r}{numeric. Circle centres and radius or radii.}

\item{xmin, ymin, xmax, ymax}{numeric. Coordinates of the rectangles}
}
\description{
This is the first stage that must be called in each pipeline. It specifies which files must be read.
The stage does nothing and returns nothing if it is not associated to another processing stage.
It only initializes the pipeline. \code{reader()} is the main function that dispatches into to other
functions. \verb{reader_*()} reads from LAS/LAZ files on disk.\code{reader_coverage()} processes the entire
point cloud. \code{reader_circles()} and \code{reader_rectangles()} read and process only some selected regions
of interest.
}
\examples{
f <- system.file("extdata", "Topography.las", package = "lasR")
read <- reader(f)
ans <- processor(read)

}
