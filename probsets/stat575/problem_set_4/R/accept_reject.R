# -----------------------------
# Acceptance-rejection sampling
# -----------------------------

# A sampling procedure for random variates in the family GAM(shape=alpha,rate=1)
dgamma1 <- function(x, alpha) {
    dgamma(x, shape = alpha, rate = 1)
}

# A sampling procedure for random variates in the family GAM(shape=alpha,rate=1)
rgamma1 <- function(n, alpha) {
    a <- floor(alpha)
    rate <- a/alpha
    q <- (alpha/exp(1))^(a-alpha)
    ys <- vector(length = n)
    for (i in 1:n) {
        repeat {
            y <- sum(rexp(a, rate)) # draw candidate
            if (runif(1) <= q * y^(alpha-a)*exp(y*a/alpha-y)) {
                ys[i] <- y
                break
            }
        }
    }
    ys
}
