library(astsa)

# problem 1
# part (a)
tsdata <- ts(jj)
n <- length(tsdata)
ys <- exp((1/n)*sum(log(tsdata)))*log(tsdata)
plot(ys)

# part (b)
t <- 1:84
qt <- as.factor(rep(1:4,21))
q1 <- qt==1
q2 <- qt==2
q3 <- qt==3
m <- cbind(t,q1,q2,q3,ys)

# fit regression model to data
fit <- lm(ys~t+q1+q2+q3, data=m)

summary(fit)

plot(ys,col="red")
lines(fitted.values(fit),type="l")

# part (c)
df <- 5
print("mse:")
print(summary(fit)$sigma^2)

plot(fit$residuals)
acf(fit$residuals)
hist(fit$residuals)

# The residuals do not look like white noise. according to the ACF, there
# seems to be some correlation. Particularly, the periods seem to be correlated,
# i.e., lags 4 and 8 are correlated.

# furthermore, when we plot the residuals, time units 40-65 seem to have a
# non-zero positive expectation. they should hover above and below more or
# less equally, but they seem to be above zero there.


# part (d)



# part (e)
library(forecast)
dEMA <- holt(ys,h=1,level=c(95),initial="simple")
summary(dEMA)
plot(ys,col="red")
lines(dEMA$fitted)

#plot(tsdata,col="red")
#lines(exp((1/alpha)*dEMA$fitted))

# part (g)

# from the summary output in party (a), the forecast using holt-winters double
# exponential method is: 8.108907 with a 95% prediction interval (7.131471 9.086344).








# problem 2

# part (b)

# Y(t) = -0.4 Y(t-1) + e(t)
# AR(1), stationary and invertible
# rho(k) = (-0.4)^k
ARMAacf(ar=c(-.4),lag.max=12,pacf=F)

# Y(t) = .9Y(t-1) + e(t) + .5e(t-1)
# ARMA(1,1), stationary and invertible
# rho(t) = .6984(.9)^t
ARMAacf(ar=c(.9),ma=c(-.5),lag.max=12,pacf=F)

# part (c)

#(i) Y(t) = -0.4 Y(t-1) + e(t)
ts1 <- arima.sim(n = 1000,
          list(ar = c(-0.4)),
          sd = 1)

acf(ts1)
# we see that the ACF of AR(1) model:
#     (1) oscillates as expected since it has a negative coefficient -0.4. 
#     (2) exponentially decays, as expected of an AR model

pacf(ts1)
# the partial acf shows that only lag 1 is compatible with a non-zero value.
# specifically, -0.4, as expected.


# 
#(iii) Y(t) = .9Y(t-1) + e(t) - (-.5)e(t-1)
# ARMA(1,1)
ts2 <- arima.sim(n = 1000,
                 list(ar = c(0.9),
                      ma = c(-0.5)),
                 sd = 1)

acf(ts2)
# we see that the ACF of ARMA(1,1) model:
#     (1) exponentially decays, as expected

pacf(ts2)
# the partial acf shows that only lag 1 is compatible with a non-zero value.
# specifically, -0.4, as expected.