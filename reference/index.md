# Package index

## All functions

- [`add_extrabytes()`](https://r-lidar.github.io/lasR/reference/add_attribute.md)
  [`remove_attribute()`](https://r-lidar.github.io/lasR/reference/add_attribute.md)
  : Add attributes to a point cloud
- [`add_rgb()`](https://r-lidar.github.io/lasR/reference/add_rgb.md) :
  Add RGB attributes to a LAS file
- [`callback()`](https://r-lidar.github.io/lasR/reference/callback.md) :
  Call a user-defined function on the point cloud
- [`classify_with_csf()`](https://r-lidar.github.io/lasR/reference/classify_with_csf.md)
  : Classify ground points
- [`classify_with_ivf()`](https://r-lidar.github.io/lasR/reference/classify_with_ivf.md)
  : Classify noise points
- [`classify_with_sor()`](https://r-lidar.github.io/lasR/reference/classify_with_sor.md)
  : Classify noise points
- [`delete_points()`](https://r-lidar.github.io/lasR/reference/delete_points.md)
  [`delete_noise()`](https://r-lidar.github.io/lasR/reference/delete_points.md)
  [`delete_ground()`](https://r-lidar.github.io/lasR/reference/delete_points.md)
  : Filter and delete points
- [`reader_las()`](https://r-lidar.github.io/lasR/reference/deprecated.md)
  [`reader_las_coverage()`](https://r-lidar.github.io/lasR/reference/deprecated.md)
  [`reader_las_circles()`](https://r-lidar.github.io/lasR/reference/deprecated.md)
  [`reader_las_rectangles()`](https://r-lidar.github.io/lasR/reference/deprecated.md)
  : Deprecated
- [`dsm()`](https://r-lidar.github.io/lasR/reference/dsm.md)
  [`chm()`](https://r-lidar.github.io/lasR/reference/dsm.md) : Digital
  Surface Model
- [`dtm()`](https://r-lidar.github.io/lasR/reference/dtm.md) : Digital
  Terrain Model
- [`edit_attribute()`](https://r-lidar.github.io/lasR/reference/edit_attribute.md)
  : Edit an attribute of the points
- [`exec()`](https://r-lidar.github.io/lasR/reference/exec.md) : Process
  the pipeline
- [`filter_with_grid()`](https://r-lidar.github.io/lasR/reference/filter_with_grid.md)
  : Select highest or lowest points
- [`keep_class()`](https://r-lidar.github.io/lasR/reference/filters.md)
  [`drop_class()`](https://r-lidar.github.io/lasR/reference/filters.md)
  [`keep_first()`](https://r-lidar.github.io/lasR/reference/filters.md)
  [`drop_first()`](https://r-lidar.github.io/lasR/reference/filters.md)
  [`keep_ground()`](https://r-lidar.github.io/lasR/reference/filters.md)
  [`keep_ground_and_water()`](https://r-lidar.github.io/lasR/reference/filters.md)
  [`drop_ground()`](https://r-lidar.github.io/lasR/reference/filters.md)
  [`keep_noise()`](https://r-lidar.github.io/lasR/reference/filters.md)
  [`drop_noise()`](https://r-lidar.github.io/lasR/reference/filters.md)
  [`keep_z_above()`](https://r-lidar.github.io/lasR/reference/filters.md)
  [`drop_z_above()`](https://r-lidar.github.io/lasR/reference/filters.md)
  [`keep_z_below()`](https://r-lidar.github.io/lasR/reference/filters.md)
  [`drop_z_below()`](https://r-lidar.github.io/lasR/reference/filters.md)
  [`keep_z_between()`](https://r-lidar.github.io/lasR/reference/filters.md)
  [`drop_z_between()`](https://r-lidar.github.io/lasR/reference/filters.md)
  [`drop_duplicates()`](https://r-lidar.github.io/lasR/reference/filters.md)
  [`print(`*`<laslibfilter>`*`)`](https://r-lidar.github.io/lasR/reference/filters.md)
  [`` `+`( ``*`<laslibfilter>`*`)`](https://r-lidar.github.io/lasR/reference/filters.md)
  : Point Filters
- [`focal()`](https://r-lidar.github.io/lasR/reference/focal.md) :
  Calculate focal ("moving window") values for each cell of a raster
- [`geometry_features()`](https://r-lidar.github.io/lasR/reference/geometry_features.md)
  : Compute pointwise geometry features
- [`normalize()`](https://r-lidar.github.io/lasR/reference/hag.md)
  [`hag()`](https://r-lidar.github.io/lasR/reference/hag.md) : Height
  Above Ground (HAG)
- [`hulls()`](https://r-lidar.github.io/lasR/reference/hulls.md) :
  Contour of a point cloud
- [`info()`](https://r-lidar.github.io/lasR/reference/info.md) : Print
  Information about the Point Cloud
- [`install_cmd_tools()`](https://r-lidar.github.io/lasR/reference/install_cmd_tools.md)
  : Use some lasR features from a terminal
- [`lasR`](https://r-lidar.github.io/lasR/reference/lasR-package.md)
  [`lasR-package`](https://r-lidar.github.io/lasR/reference/lasR-package.md)
  : lasR: airborne LiDAR for forestry applications
- [`load_matrix()`](https://r-lidar.github.io/lasR/reference/load_matrix.md)
  : Load a matrix for later use
- [`load_raster()`](https://r-lidar.github.io/lasR/reference/load_raster.md)
  : Load a raster for later use
- [`local_maximum()`](https://r-lidar.github.io/lasR/reference/local_maximum.md)
  [`local_maximum_raster()`](https://r-lidar.github.io/lasR/reference/local_maximum.md)
  : Local Maximum
- [`metric_engine`](https://r-lidar.github.io/lasR/reference/metric_engine.md)
  : Metric engine
- [`set_parallel_strategy()`](https://r-lidar.github.io/lasR/reference/multithreading.md)
  [`unset_parallel_strategy()`](https://r-lidar.github.io/lasR/reference/multithreading.md)
  [`get_parallel_strategy()`](https://r-lidar.github.io/lasR/reference/multithreading.md)
  [`ncores()`](https://r-lidar.github.io/lasR/reference/multithreading.md)
  [`half_cores()`](https://r-lidar.github.io/lasR/reference/multithreading.md)
  [`sequential()`](https://r-lidar.github.io/lasR/reference/multithreading.md)
  [`concurrent_files()`](https://r-lidar.github.io/lasR/reference/multithreading.md)
  [`concurrent_points()`](https://r-lidar.github.io/lasR/reference/multithreading.md)
  [`nested()`](https://r-lidar.github.io/lasR/reference/multithreading.md)
  [`has_omp_support()`](https://r-lidar.github.io/lasR/reference/multithreading.md)
  : Parallel processing tools
- [`pit_fill()`](https://r-lidar.github.io/lasR/reference/pit_fill.md) :
  Pits and spikes filling
- [`print(`*`<lasrcloud>`*`)`](https://r-lidar.github.io/lasR/reference/print.lasrcloud.md)
  : Print Method for 'lasrcloud' Objects
- [`rasterize()`](https://r-lidar.github.io/lasR/reference/rasterize.md)
  : Rasterize a point cloud
- [`read_cloud()`](https://r-lidar.github.io/lasR/reference/read_cloud.md)
  : Read a point cloud in memory
- [`reader()`](https://r-lidar.github.io/lasR/reference/reader.md)
  [`reader_coverage()`](https://r-lidar.github.io/lasR/reference/reader.md)
  [`reader_circles()`](https://r-lidar.github.io/lasR/reference/reader.md)
  [`reader_rectangles()`](https://r-lidar.github.io/lasR/reference/reader.md)
  : Initialize the pipeline
- [`region_growing()`](https://r-lidar.github.io/lasR/reference/region_growing.md)
  : Region growing
- [`sampling_voxel()`](https://r-lidar.github.io/lasR/reference/sampling.md)
  [`sampling_pixel()`](https://r-lidar.github.io/lasR/reference/sampling.md)
  [`sampling_poisson()`](https://r-lidar.github.io/lasR/reference/sampling.md)
  : Sample the point cloud
- [`set_crs()`](https://r-lidar.github.io/lasR/reference/set_crs.md) :
  Set the CRS of the pipeline
- [`set_exec_options()`](https://r-lidar.github.io/lasR/reference/set_exec_options.md)
  [`unset_exec_option()`](https://r-lidar.github.io/lasR/reference/set_exec_options.md)
  : Set global processing options
- [`sort_points()`](https://r-lidar.github.io/lasR/reference/sort_points.md)
  : Sort points in the point cloud
- [`stop_if_outside()`](https://r-lidar.github.io/lasR/reference/stop_if_outside.md)
  : Stop the pipeline conditionally
- [`summarise()`](https://r-lidar.github.io/lasR/reference/summarise.md)
  : Summary
- [`temptif()`](https://r-lidar.github.io/lasR/reference/temporary_files.md)
  [`tempgpkg()`](https://r-lidar.github.io/lasR/reference/temporary_files.md)
  [`tempshp()`](https://r-lidar.github.io/lasR/reference/temporary_files.md)
  [`templas()`](https://r-lidar.github.io/lasR/reference/temporary_files.md)
  [`templaz()`](https://r-lidar.github.io/lasR/reference/temporary_files.md)
  : Temporary files
- [`print(`*`<PipelinePtr>`*`)`](https://r-lidar.github.io/lasR/reference/tools.md)
  [`` `+`( ``*`<PipelinePtr>`*`)`](https://r-lidar.github.io/lasR/reference/tools.md)
  [`` `[[`( ``*`<PipelinePtr>`*`)`](https://r-lidar.github.io/lasR/reference/tools.md)
  : Tools inherited from base R
- [`transform_with()`](https://r-lidar.github.io/lasR/reference/transform_with.md)
  : Transform a Point Cloud Using Another Stage
- [`triangulate()`](https://r-lidar.github.io/lasR/reference/triangulate.md)
  : Delaunay triangulation
- [`write_las()`](https://r-lidar.github.io/lasR/reference/write.md)
  [`write_copc()`](https://r-lidar.github.io/lasR/reference/write.md)
  [`write_pcd()`](https://r-lidar.github.io/lasR/reference/write.md) :
  Write point clouds
- [`write_lax()`](https://r-lidar.github.io/lasR/reference/write_lax.md)
  : Write spatial indexing .lax files
- [`write_vpc()`](https://r-lidar.github.io/lasR/reference/write_vpc.md)
  : Write a Virtual Point Cloud
