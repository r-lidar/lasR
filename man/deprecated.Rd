% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/deprecated.R
\name{deprecated}
\alias{deprecated}
\alias{processor}
\alias{reader}
\alias{reader_coverage}
\alias{reader_circles}
\alias{reader_rectangles}
\title{Deprecated}
\usage{
processor(pipeline, ncores = half_cores(), progress = FALSE, ...)

reader(x, filter = "", buffer = 0, ...)

reader_coverage(x, filter = "", buffer = 0, ...)

reader_circles(x, xc, yc, r, filter = "", buffer = 0, ...)

reader_rectangles(x, xmin, ymin, xmax, ymax, filter = "", buffer = 0, ...)
}
\arguments{
\item{pipeline}{a LASRpipeline. A serie of algorithms called in order}

\item{ncores}{integer. Number of cores to use. Some stages or some steps in some stages
are parallelised but overall one file is process at a time.}

\item{progress}{boolean. Displays a progress bar.}

\item{...}{passed to other readers}

\item{x}{Can be the paths of the files to use, the path of the folder in which the files are stored,
the path to a [virtual point cloud](https://www.lutraconsulting.co.uk/blog/2023/06/08/virtual-point-clouds/)
file or a `data.frame` containing hte point cloud. It supports also a `LAScatalog` or a `LAS` objects
from `lidR`.}

\item{filter}{the 'filter' argument allows filtering of the point-cloud to work with points of
interest. The available filters are those from LASlib and can be found by running \link{filter_usage}.
For a given algorithm when a filter is applied, only the points that meet the criteria are processes.
The most common strings are "-keep_first", "-keep_class 2", "drop_z_below 2". For more details see
\link{filters}.}

\item{buffer}{numeric. Each file is read with a buffer. The default is 0, which does not mean that
the file won't be buffered. It means that the internal routine knows if a buffer is needed and will
pick the greatest value between the internal suggestion and this provided value.}

\item{xc, yc, r}{numeric. Circle centres and radius or radii.}

\item{xmin, ymin, xmax, ymax}{numeric. Coordinates of the rectangles}
}
\description{
Deprecated function replaced by \link{exec} and \link{reader_las}
}
