# function for performing a multinomial goodness of fit hypothesis test
# with a specified null model pi.0 in which pi.0 is the result of t parameters.
#
# input:
#     obs: vector of counts
#     pi.0: vector of hypothesized probabilites,
#     t: dimension of the parameter space under the null hypothesis.
# 
# outputs: two lists, one for X2 and one for G2. each list has:
#     X2: list relevant results for X^2 test
#     G2: list of relevant results for G^2 test, e.g.:
#       list(stat,p,df,reject) where
#         p: p-value of the obs under the null model
#         df: degrees of freedom of the reference distribution (chi-square)
multinomial_goodness_of_fit <- function(obs, pi.0, t=0)
{
  n <- sum(obs)                 # number of observations
  t.full <- length(obs)-1       # dim of parameter space for the full model.
  df <- t.full-t                # degrees of freedom for the ref distribution
  m.0 <- n*pi.0                 # expected observations (for given n)
  
  G2 <- 2*sum(obs*log(obs/m.0)) # log-likelihood ratio test statistic
  G2.p <- pchisq(q=G2,df=df,lower.tail=FALSE)

  X2 <- sum((obs-m.0)^2/m.0)   # test statistic
  X2.p <- pchisq(q=X2,df=df,lower.tail=FALSE)

  # return relevant test statistics
  list(m.0=m.0,pi.0=pi.0,
       X2=list(stat=X2,p=X2.p,df=df),
       G2=list(stat=G2,p=G2.p,df=df))
}

# obs is an IxJ matrix representing cross-sectional data (observed counts).
# tests for independence P(X,Y) = P(X)P(Y).
multinomial_goodness_of_fit_independence_test <- function(obs)
{
  I <- dim(obs)[1]
  J <- dim(obs)[2]
  t <- (I-1)+(J-1)
  
  n <- sum(obs)  
  row.sum <- apply(obs,1,sum)
  col.sum <- apply(obs,2,sum)
  m.0 <- matrix(row.sum) %*% t(matrix(col.sum))/n
  
  multinomial_goodness_of_fit(obs=obs,pi.0=m.0/n,t=t)
}

independence_test_monotonic_association <- function(obs)
{
  # to compute the estimate of gamma and the standard error, we need MESS
  library("MESS")
  
  # gkgamma takes an IxJ matrix as an input
  gk.result <- gkgamma(obs)
  
  # compute gamma.hat and its standard error
  gamma.hat <- gk.result$estimate
  se <- gk.result$se1
  
  # compute the z statistic and p-value for testing gamma=0 (independence)
  z <- gamma.hat / se
  p.value <- 2*pnorm(abs(z),lower.tail = FALSE)
  
  list(gamma=gamma.hat,ase=se,stat=z,p=p.value)
}
