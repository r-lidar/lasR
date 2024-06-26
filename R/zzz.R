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
  set_proj_lib()

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
  library.dynam.unload("lasR", libpath) # nocov
}

# Check if the package has more recent version
check_update = function()
{
  msg <- NULL

  last  <- get_latest_version()
  curr <- utils::packageVersion("lasR")
  new_version = !is.null(last) && last > curr
  dev_version = is_dev_version(curr)

  # nocov start
  if (new_version)
  {
    if (dev_version)
      msg = paste("lasR", last, "is now available. You are using", curr, "(unstable) \ninstall.packages('lasR', repos = 'https://r-lidar.r-universe.dev')")
    else
      msg = paste("lasR", last, "is now available. You are using", curr, "\ninstall.packages('lasR', repos = 'https://r-lidar.r-universe.dev')")
  }
  else if (dev_version)
  {
    msg = paste("lasR", curr, "is an unstable development version")
  }

  if (!is.null(msg) & interactive()) packageStartupMessage(msg)
  # nocov end

  return(NULL)
}

get_latest_version = function()
{
  nullcon = NULL

  ans <- tryCatch(
  {
    nullcon <- file(nullfile(), open = "wb")
    sink(nullcon, type = "message")
    res <- utils::old.packages(repos = "https://r-lidar.r-universe.dev")
    sink(type = "message")
    close(nullcon)
    res
  },
  error = function(e)
  {
    sink(NULL, type = "message") # nocov
    close(nullcon) # nocov
    return(NULL) # nocov
  })

  if (is.null(ans)) return(NULL) # nocov

  ind = which(ans[,1] == "lasR")

  if (length(ind) == 0) return(NULL)

  version <- ans[ind, 5] # nocov
  version <- package_version(version) # nocov
  return(version) # nocov
}

is_dev_version = function(version)
{
  class(version) <- "list"
  version = version[[1]]
  return(length(version) == 4)
}

set_proj_lib <- function()
{
  # should exist on windows
  if (file.exists(prj <- system.file("proj", package = "lasR")[1]))
  {
    Sys.setenv(PROJ_LIB = prj)
  }
}


LASROPTIONS = new.env()

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
