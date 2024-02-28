#' lasR: airborne LiDAR for forestry applications
#'
#' lasR provides a set of tools to process efficiently airborne LiDAR data in forestry contexts.
#' The package works with .las or .laz files. The toolbox includes algorithms for DSM, CHM, DTM, ABA,
#' normalisation, tree detection, tree segmentation, tree delineation, colourization, validation and
#' other tools, as well as a processing engine to process broad LiDAR coverage split into many files
#' efficiently.
#'
#' @useDynLib lasR, .registration = TRUE
"_PACKAGE"

.onAttach <- function(libname, pkgname)
{
  check_update()
}

.onLoad <- function(libname, pkgname)
{
  # installed <- row.names(installed.packages())
  # nothing <- function(x) { return(x)}
  # lasR.read_vector = if ("sf" %in% installed) sf::read_sf else nothing
  # lasR.read_raster = if ("terra" %in% installed) terra::rast else nothing
  #
  # op <- options()
  # op.lidR <- list(
  #   lasR.read_vector = lasR.read_vector,
  #   lasR.read_raster = lasR.read_raster,
  #   lasR.read_las = nothing)
  #
  # toset <- !(names(op.lidR) %in% names(op))
  # if (any(toset)) options(op.lidR[toset])
  #
  # invisible()
}

.onUnload <- function(libpath)
{
  library.dynam.unload("lasR", libpath)
}

# Check if the package has more recent version
check_update = function()
{
  f <- paste0(tempdir(), "/LASRDESCRIPTION")

  ans <- tryCatch(
  {
    nullcon <- file(nullfile(), open = "wb")
    sink(nullcon, type = "message")
    suppressWarnings(utils::download.file("https://raw.githubusercontent.com/r-lidar/lasR/main/DESCRIPTION", f))
    sink(type = "message")
    close(nullcon)
    TRUE
  },
  error = function(e)
  {
    sink(type = "message")
    return(FALSE)
  })

  if (isFALSE(ans))
    return(NULL)

  m <- read.dcf(f)
  dev <- m[4]
  dev <- package_version(dev)
  curr <- utils::packageVersion("lasR")
  if (dev > curr)
  {
    packageStartupMessage(paste("lasR", dev, "is now available. You are using", curr, "\nremotes::install_github(\"r-lidar/lasR\")"))
  }

  return(NULL)
}

# ,@@@
#  @@@@@@.@                                                           ,#*
#   @@@@@....*&                                                 @(@@@@@@%
#    &@@@......./(                                         .&.....@@@@@
#      @@%.........@                                    ,*........@@@@
#       @@...........@                                @..........@@@@
#         @...........*.                            /............@@
#           @...........@                         *.............@           *@@(@
#             @..........(  %,................@ &............#.       (@*.......@
#               *.........................................*(     %@.............%
#                  /,..&...............................@    ,@..................
#                     ...............................*  #&.....................&
#                    .................................&........................&
#                   @.........................................................,
#                   @.....@@@.................,@@%.....#......................@
#                   &...@@@   *..............@  %@@....&......................
#                   @...@&@@@@*..............@@@@@&....@..................*@%
#                   ......@@&......../&........@@/.....(...........@&
#                  @@##@.............................,*.@....@
#                  %(((((@......#...#%,@..........#(((((@.....,
#                  @((((((%......................%((((((@.......@
#                   @(((((.......................@(((((#..........@
#                    (((&.........................&(((@  @..........&
#                     @.@...........................@.    /,........,@
#                     /....&.....................(,... @........
#                    /......................./*.......&.....@
#                    ..............******..............%..(,/
#                  /....................................%((((&
#                 @...............#.......,..............@
#                &.......................@................&
#               %..................@...../............@....@
#              &...................&....*.................../
#         @....@.....@............./....@............@......@ @/%
#         @(.@...@.................*....@...........#......@..../#
#          ,*****...../............*****@..........,...../.,**%,
#            @*****................&****@........./......*****&
#              /*********@.........@****/........%***,.*****@
#                /*********........%(**(@.......&*********@
#                  @**&     .&.@#        %#.,.. .@(@***@
