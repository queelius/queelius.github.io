library(forecast)

arima_fitter <- function(ts,P,D,Q,n=1,ic="aic",include.drift=FALSE)
{
  models <- matrix(nrow=length(P)*length(D)*length(Q),ncol=4)
  colnames(models) <- c("p","d","q",ic)
  i <- 1
  for (p in P)
  {
    for (d in D)
    {
      for (q in Q)
      {
        model <- Arima(ts,order=c(p,d,q),include.drift=include.drift)
        models[i,1] <- p
        models[i,2] <- d
        models[i,3] <- q
        if (ic == "aic")
          models[i,4] <- model$aic
        if (ic == "aicc")
          models[i,4] <- model$aicc
        if (ic == "bic")
          models[i,4] <- model$bic
        i <- i + 1
      }
    }
  }
  models <- models[order(models[,4],decreasing=FALSE),]
  models[1:n,]
}
