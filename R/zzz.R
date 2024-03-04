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
  set_lasr_strategy(concurrent_points())
}

.onUnload <- function(libpath)
{
  library.dynam.unload("lasR", libpath)
}

# Check if the package has more recent version
check_update = function()
{
  msg <- NULL

  last  <- get_latest_version()
  curr <- utils::packageVersion("lasR")
  new_version = !is.null(last) && last > curr
  dev_version = is_dev_version(curr)

  if (new_version)
  {
    if (dev_version)
      msg = paste("lasR", last, "is now available. You are using", curr, "(unstable) \nremotes::install_github(\"r-lidar/lasR\")")
    else
      msg = paste("lasR", last, "is now available. You are using", curr, "\nremotes::install_github(\"r-lidar/lasR\")")
  }
  else if (dev_version)
  {
    msg = paste("lasR", curr, "is an unstable development version")
  }

  if (!is.null(msg) & interactive()) packageStartupMessage(msg)
  return(NULL)
}

get_latest_version = function()
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
  version <- m[4]
  version <- package_version(version)
  return(version)
}

is_dev_version = function(version)
{
  class(version) <- "list"
  version = version[[1]]
  return(length(version) == 4)
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
