library(fpp2)
library(forecast)
library(TSA)

data <- read.table(file="./acc2",sep=" ",header=TRUE)
data < data[,c("t","accuracy")]
p <- ts(data[2000:30000,1],start=2000)
md <- nls(p ~ I(c)-I(a)*exp(-I(b)*time(p)),
          start=list(b=.005,a=.05,c=.5),trace=T)


fit_fn <- function(t)
{
  a <- coef(md)["a"]
  b <- coef(md)["b"]
  c <- coef(md)["c"]
  c - a * exp(-b*t)
}

pp <- ts(data[1:50000,1],start=1)
tt <- time(pp)

plot(data.frame(tt,pp))
lines(fit_fn(1:50000))

# pi(t) = f(t) + n(t)
# where n(t) ~ ARIMA(p,d,q)
ts <- 1000:10000
p.train <- ts(data[ts[1]:ts[length(ts)],1])
print(p.train)
p.test <- ts(data[50001:100000,1],start=ts[length(ts)]+1)
df <- data.frame(ts,p.train)
head(p.train);

### model trend
md <- nls(p.train ~ I(c) - I(a)*exp(-I(b)*ts),
          data=df,
          start=list(b=.001,a=.135,c=.5),trace=T)

md
#pred <- coef(md)["c"]-coef(md)["a"]*exp(-coef(md)["b"]*(1000:50000))
#plot(pred)

#########################################
#########################################
# differenced process 0
#########################################

# quick inspection for stationarity
plot(p.train);
acf(p.train);
tseries::adf.test(p.train)
eacf(p.train);

#########################################
# differenced process 1
#########################################
# inspect diff proc for stationarity
p.train.diff <- diff(p.train)
plot(p.train.diff);
acf(p.train.diff);
pacf(p.train);
eacf(p.train.diff);
tseries::adf.test(p.train.diff)

# 
# #########################################
# # differenced process 2
# #########################################
# # inspect diff proc for stationarity
# p.train.diff2 <- diff(p.train,2)
# plot(p.train.diff2);
# acf(p.train.diff2);
# tseries::adf.test(p.train.diff2)
# eacf(p.train.diff2);
# 

# #####################################
# # Holt EMA model
# #####################################
# p.train.ema <- holt(p.train,initial="optimal")
# summary(p.train.ema)
# Box.test(p.train.ema$res, lag=10, type="Ljung-Box",fitdf=5)
# qqnorm(p.train.ema$residual);
# hist(p.train.ema$residual);

p.train.arima1 <- Arima(p.train,order=c(1,1,9))
summary(p.train.arima1);
Box.test(p.train.arima1$res, lag=20, type="Ljung-Box",fitdf=10);
qqnorm(p.train.arima1$residual);
hist(p.train.arima1$residual);
plot(p.train.arima1$residual);



p.train.arima0 <- auto.arima(p.train)
summary(p.train.arima0);
Box.test(p.train.arima0$res,
         lag=10, type="Ljung-Box",fitdf=length(coef(p.train.arima0)))

qqnorm(p.train.arima0$residual);
hist(p.train.arima0$residual);
plot(p.train.arima0$residual);


# heavy tail plots. normal in middle, not so normal at extremes.
