% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/stages.R
\name{classify_with_sor}
\alias{classify_with_sor}
\title{Classify noise points}
\usage{
classify_with_sor(k = 8, m = 6, class = 18L)
}
\arguments{
\item{k}{numeric. The number of neighbours}

\item{m}{numeric. Multiplier. The maximum distance will be: ⁠avg distance + m * std deviation⁠}

\item{class}{integer. The class to assign to the points that match the condition.}
}
\value{
This stage transforms the point cloud in the pipeline. It consequently returns nothing.
}
\description{
Classify points using the Statistical Outliers Removal (SOR) methods first described in the PCL
library and also implemented in CloudCompare (see references). For each point, it computes the mean
distance to all its k-nearest neighbors. The points that are farther than the average distance
plus a number of times (multiplier) the standard deviation are considered noise.
}
