# GDAL configuration options that reading a remote EPT endpoint tunes
# (FileCollection::add_ept_endpoint)
ept_gdal_keys <- c("GDAL_DISABLE_READDIR_ON_OPEN", "GDAL_HTTP_MULTIPLEX", "VSI_CACHE", "VSI_CACHE_SIZE")

# Unsets these options in the environment and returns their previous values for restore_env()
unset_ept_gdal_env <- function()
{
  old <- Sys.getenv(ept_gdal_keys, unset = NA, names = TRUE)
  Sys.unsetenv(ept_gdal_keys)
  old
}

restore_env <- function(old)
{
  for (k in names(old))
  {
    if (is.na(old[[k]])) Sys.unsetenv(k)
    else do.call(Sys.setenv, as.list(old[k]))
  }
}
