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
