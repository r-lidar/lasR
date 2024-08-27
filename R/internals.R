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

  f <- system.file("lasr.R", package = "lasR")
  d <- system.file("", package = "lasR")

  if (o == "linux") {
    cmd = paste("ln -s", f, "~/.local/bin/lasr")
    system(cmd)
    return(invisible())
  }

  if (os == "windows") {
    bat = substr(f, 1, nchar(f)-1)
    batch_file_path = paste0(bat, "bat")
    batch_content <- c("@echo off", paste("Rscript", f, "%*"))
    writeLines(batch_content, con = batch_file_path)

    cat("To run the 'lasr' command from anywhere in the Command Prompt, you need to add its location to your system's PATH:

  1. Right-click on This PC or My Computer and select Properties.
  2. Click on Advanced system settings and then Environment Variables.
  3. In the System variables section, find the Path variable, select it, and click Edit.
  4. Add this path:", d)

    return(invisible())
  }

  if (os == "osx") stop("MacOS not supported")

  stop("We failed to detect your OS")
}
# nocov end
