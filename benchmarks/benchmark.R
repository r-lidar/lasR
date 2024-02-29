#!/usr/bin/env Rscript
args = commandArgs(trailingOnly=TRUE)

if (length(args) != 2) {
  stop("At least 2 argument must be supplied", call.=FALSE)
} else {
  lasR = args[1] == "lasR"
  test = as.integer(args[2])
}

library(lidR)
library(lasR)

set_lidr_threads(half_cores())

f = c("/home/jr/Documents/Ulaval/ALS data/BCTS//092L072244_BCTS_2.laz",
      "/home/jr/Documents/Ulaval/ALS data/BCTS//092L072422_BCTS_2.laz",
      "/home/jr/Documents/Ulaval/ALS data/BCTS//092L073133_BCTS_2.laz",
      "/home/jr/Documents/Ulaval/ALS data/BCTS//092L073311_BCTS_2.laz")

#f = system.file("extdata", "bcts/", package = "lasR")

ti = Sys.time()

if (test == 1)
{
  if (lasR)
  {
    pipeline = reader(f) + rasterize(1, "max")
    processor(pipeline, progress = TRUE, noread = T)
  }
  else
  {
    ctg = readLAScatalog(f)
    chm = rasterize_canopy(ctg, 1, p2r())
  }
}

if (test == 2)
{
  if (lasR)
  {
    tri = triangulate()
    pipeline = reader(f, filter = keep_ground()) + tri + rasterize(1, tri)
    processor(pipeline)
  }
  else
  {
    ctg = readLAScatalog(f)
    dtm = rasterize_terrain(ctg, 1, tin())
  }
}

if (test == 3)
{
  if (lasR)
  {
    custom_function = function(z,i) { list(avgz = mean(z), avgi = mean(i)) }
    read = reader(f)
    chm = rasterize(1, "max")
    met = rasterize(20, custom_function(Z, Intensity))
    den = rasterize(5, "count")
    pipeline = read + chm + met + den
    processor(pipeline)
  }
  else
  {
    custom_function = function(z,i) { list(avgz = mean(z), avgi = mean(i)) }
    ctg = readLAScatalog(f)
    chm = rasterize_canopy(ctg, 1, p2r())
    met = pixel_metrics(ctg, ~custom_function(Z, Intensity), 20)
    den = rasterize_density(ctg, 5)
  }
}

if (test == 4)
{
  if (lasR)
  {
    read = reader(f)
    del = triangulate(filter = keep_ground())
    norm = transform_with_triangulation(del)
    dtm = rasterize(1, del)
    chm = rasterize(1, "max")
    seed = local_maximum(3)
    tree = region_growing(chm, seed)
    write = write_las()
    pipeline = read + del + norm + write + dtm + chm + seed + tree
    ans = processor(pipeline, progress = TRUE)
  }
  else
  {
    ctg = readLAScatalog(f)

    opt_output_files(ctg) <- paste0(tempdir(), "/*_dtm")
    dtm = rasterize_terrain(ctg, 1, tin())

    opt_output_files(ctg) <- paste0(tempdir(), "/*_norm")
    nctg = normalize_height(ctg, tin())

    opt_output_files(nctg) <- paste0(tempdir(), "/chm_*")
    chm = rasterize_canopy(nctg, 1, p2r())
    chm = chm*1

    opt_output_files(nctg) <- ""
    seed = locate_trees(nctg, lmf(3), uniqueness = "gpstime")
    seed$treeID = 1:nrow(seed)

    tree = dalponte2016(chm, seed)()
  }
}

tf = Sys.time()
tf-ti

q("no")

m_lasr = c(0,191.41, 362.51, 593.35, 896.25, 1194.72, 1818.00, 2323.06, 2323.06, 2323.06, 2323.06, 2323.06, 2323.06, 2323.06, 2323.06, 2323.06, 2323.06, 2323.06, 2247.46, 2122.34,
           2192.39, 2192.39, 2192.39, 2197.03, 2197.80, 2198.83, 2199.87,2181.38, 2181.38, 2181.38, 2221.91, 1523.74, 1741.71, 1823.69, 1823.69, 1823.69, 1823.69, 1741.83, 1741.83, 1741.83,
           1741.83, 1741.83, 1454.66, 1703.53, 1928.16, 2152.36, 2276.37,2506.08, 2506.08, 2506.08, 2506.08, 2506.08, 2506.08, 2506.08,2506.08, 2506.08, 2506.08, 2506.08, 2277.12, 2324.82,
           2324.82, 2324.82, 2324.82, 2324.82, 2324.82, 2324.82, 2324.82, 2296.59, 2296.59, 2296.59, 2323.40, 1519.98, 1738.68, 1900.92, 2009.98, 2009.98, 2009.98, 2009.98, 2009.98, 1901.16,
           1901.16, 1901.16, 1901.16, 1901.16, 1901.16, 765.81, 768.27)

m_lidr = c(191.54,379.01, 1481.95, 1781.54, 1633.96, 1537.73, 1696.50, 1679.94, 2216.42, 2040.62, 2017.32,2344.53, 3169.67, 4292.79, 4777.30, 4777.30, 6607.64, 2052.25, 2558.57,
           4997.12, 3933.93, 5600.17, 5695.82, 5695.82, 6298.47, 6699.40, 3047.77, 4923.14, 3304.03, 3579.05, 4430.32, 4469.81, 4469.81,4469.81, 4469.81, 4469.81, 4469.81, 4043.73, 4255.07,
           4833.98, 3449.89, 3449.89, 3449.89, 3449.89, 3449.89, 3449.89, 3296.96, 3532.51, 3198.07, 2747.25)

t_lasr = 1:length(m_lasr)*5
t_lidr = 1:length(m_lidr)*15

lasr = data.frame(t = t_lasr, m = m_lasr, pkg = "lasR")
lidr = data.frame(t = t_lidr, m = m_lidr, pkg = "lidR")
data = rbind(lasr, lidr)
data$pkg = factor(data$pkg, level = c("lidR", "lasR"))

library(ggplot2)
ggplot(data) +
  aes(x = t/60, y = m, fill = pkg) +
  geom_area(stat = "identity", color = "white", size = 0.2, lwd = 0.5) + # Adding white borders to areas
  scale_fill_viridis_d() + # Using a viridis color scale for better color gradients
  theme_minimal() + # Using a minimal theme for simplicity
  labs(x = "Time (min)", y = "RAM Memory (MB)", title = "Memory Usage Over Time - 76 Million Points") +
  theme(legend.background = element_rect(colour = "black", fill = "white"),
        legend.position = c(0.8, 0.85),
        axis.text = element_text(size = 10), # Adjusting axis text size
        plot.title = element_text(hjust = 0.5, size = 14),
        legend.text = element_text(size = 10), # Adjusting legend text size
        legend.title = element_text(size = 12)) # Adjusting legend title size and making it bold
