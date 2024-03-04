eval_number_of_metrics = function(call, env = globalenv())
{
  ans = tryCatch(
  {
    symbols = lapply(call, function(x) x)
    nsymbols = 0
    csymbols = c()
    for (i in seq_along(symbols))
    {
      symbol = symbols[i]
      if (is.symbol(symbol[[1]]) & (is.null(names(symbol)) || names(symbol) == ""))
      {
        nsymbols = nsymbols + 1
        csymbols = c(csymbols, as.character(symbol[[1]]))
      }
    }

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
