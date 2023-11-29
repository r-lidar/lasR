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
  # Runs when attached to search() path such as by library() or require()
  if (!interactive()) return(invisible())

  v = utils::packageVersion("lasR") # nocov
  packageStartupMessage("lasR ", v, " is an experimental version") # nocov
}

# nocov start
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
# nocov end

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
