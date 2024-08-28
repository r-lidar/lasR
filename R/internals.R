eval_number_of_metrics = function(call, env = globalenv())
{
  ans = tryCatch(
  {
    csymbols = get_symbols(call)
    nsymbols = length(csymbols)

    data = matrix(stats::runif(10*nsymbols), nrow = 10, ncol = nsymbols)
    data = as.data.frame(data)
    names(data) <- csymbols
    res = eval(call, envir = data, enclos = env)
    length(res)
  },
  error = function(e)
  {
    print(e)
    return(NA_integer_)
  })

  return(ans)
}

get_symbols = function(call)
{
  symbols = lapply(call, function(x) x)
  csymbols = c()
  for (i in seq_along(symbols))
  {
    symbol = symbols[i]
    if (is.symbol(symbol[[1]]) & (is.null(names(symbol)) || names(symbol) == ""))
    {
      csymbols = c(csymbols, as.character(symbol[[1]]))
    }
    else if (is.call(symbol[[1]]))
    {
      csymbols = c(csymbols, get_symbols(symbol[[1]]))
    }
  }

  return(unique(csymbols))
}

# nocov start

#' Use some lasR features from a terminal
#'
#' Install the required files to be able to run some simple lasR commands from a terminal.
#' Working in a terminal is easier for simple tasks but it is not possible to build
#' complex pipelines this way. Examples of some possible commands:
#' ```
#' lasr help
#' lasr info -i /path/to/file.las
#' lasr chm -i /path/to/folder -o /path/to/chm.tif -res 1 -ncores 8
#' lasr dtm -i /path/to/folder -o /path/to/dtm.tif -res 0.5 -ncores 8
#' ````
#'
#' @export
#' @md
install_cmd_tools = function()
{
  get_os <- function(){
    sysinf <- Sys.info()
    if (!is.null(sysinf)){
      os <- sysinf['sysname']
      if (os == 'Darwin')
        os <- "osx"
    } else { ## mystery machine
      os <- .Platform$OS.type
      if (grepl("^darwin", R.version$os))
        os <- "osx"
      if (grepl("linux-gnu", R.version$os))
        os <- "linux"
    }
    tolower(os)
  }

  f <-  normalizePath(system.file("lasr.R", package = "lasR"))
  d <-  normalizePath(system.file("", package = "lasR"))

  os = get_os()

  if (os == "linux") {
    cmd = paste("ln -s", f, "~/.local/bin/lasr")
    system(cmd)
    return(invisible())
  }

  if (os == "windows") {
    Rscript = Sys.which("Rscript")
    if (Rscript == "") stop("Cannot find the path to Rscript.exe")
    bat = substr(f, 1, nchar(f)-1)
    batch_file_path = paste0(bat, "bat")
    batch_content <- c("@echo off", paste0('"',Rscript, '" "', f, '" ', "%*"))
    writeLines(batch_content, con = batch_file_path)

    cat("To run the 'lasr' command from anywhere in the Command Prompt, you need to add its location to your system's PATH:

  1. In the windows search bar type: 'env'
  2. Click on: Edit the system environment variables
  2. Go to the 'Advanced' tab and click on 'Environment variables'
  3. Click on 'Path' and edit the settings.
  4. Add this path:", d, "

For more details see https://www.eukhost.com/kb/how-to-add-to-the-path-on-windows-10-and-windows-11/")

    return(invisible())
  }

  if (os == "osx") {
    cmd = paste("ln -s", f, "/usr/local/bin/lasr")
    system(cmd)
    return(invisible())
  }

  stop("We failed to detect your OS")
}
# nocov end
