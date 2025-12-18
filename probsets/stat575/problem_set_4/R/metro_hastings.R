# -----------------------------
# Metropolis-hastings alogrithm
# -----------------------------


# A sampling procedure for random variates in the family GAM(shape=alpha,rate=1)
# using Metropolis-Hastings algorithm
metro.rgamma1 <- function(n, alpha) {
    a <- floor(alpha)
    rate <- a/alpha

    # Density for random variates in the family GAM(shape=alpha,rate=1)
    f <- function(x) {
        dgamma(x, shape = alpha, rate = 1)
    }

    g <- function(x) {
        dgamma(x, shape = a, rate = rate)
    }

    ys <- vector(length = n)
    ys[1] <- sum(rexp(a,rate))   

    for (i in 2:n) {
        v <- sum(rexp(a,rate)) # draw candidate
        u <- ys[i-1]
        R <- f(v) * g(u) / (f(u) * g(v))
        if (runif(1) <= R) { ys[i] <- v }
        else { ys[i] <- u }
    }
    ys
}
