library(fpp2)
library(forecast)
library(TSA)

data <- read.table(file="./acc2",sep=" ",nrows=100000,header=TRUE)

# pi(t) = f(t) + n(t)
# where n(t) ~ ARIMA(p,d,q)
p.train <- ts(data[1000:50000,1])
p.test <- ts(data[50001:100000,1],start=50001)

head(p.train);

p.train <- log(p.train)
p.test <- log(p.test)

# quick inspection for stationarity
plot(p.train);
acf(p.train);
tseries::adf.test(p.train)
# not stationary. acf doesn't exponentially decay/zero out

#########################################
#########################################
# differenced process 1
#########################################
# inspect diff proc for stationarity
p.train.diff <- diff(p.train)
plot(p.train.diff);
acf(p.train.diff);
eacf(p.train.diff);
tseries::adf.test(p.train.diff)

#####################################
# Arima(p,1,q) models
#####################################
p.train.arima1 <- Arima(p.train,order=c(1,1,1))
summary(p.train.arima1);
Box.test(p.train.arima1$res, lag=10, type="Ljung-Box",fitdf=0);
qqnorm(p.train.arima1$residual);
hist(p.train.arima1$residual);

# arima(1,1,3)
p.train.arima2 <- Arima(p.train,order=c(1,1,3))
summary(p.train.arima2);
Box.test(p.train.arima2$res, lag=10, type="Ljung-Box",fitdf=4);
qqnorm(p.train.arima2$residual);
hist(p.train.arima2$residual);

# arima(2,1,4)
p.train.arima3 <- Arima(p.train,order=c(2,1,4))
summary(p.train.arima3);
Box.test(p.train.arima3$res, lag=10, type="Ljung-Box",fitdf=6);
qqnorm(p.train.arima3$residual);
hist(p.train.arima3$residual);

# arima(0,1,5)
p.train.arima4 <- Arima(p.train,order=c(0,1,5))
summary(p.train.arima4)
Box.test(p.train.arima4$res, lag=10, type="Ljung-Box",fitdf=5)
qqnorm(p.train.arima4$residual);
hist(p.train.arima4$residual);

# arima(2,1,3)
p.train.arima5 <- Arima(p.train,order=c(2,1,3))
summary(p.train.arima5);
Box.test(p.train.arima5$res, lag=10, type="Ljung-Box",fitdf=5);
qqnorm(p.train.arima5$residual);
hist(p.train.arima5$residual);

# arima(13,1,13)
p.train.arima6 <- Arima(p.train,order=c(13,1,13))
summary(p.train.arima6);
Box.test(p.train.arima6$res, lag=10, type="Ljung-Box",fitdf=26);
qqnorm(p.train.arima6$residual);
hist(p.train.arima6$residual);


#########################################
#########################################
# differenced process 2
#########################################
# inspect diff proc for stationarity
p.train.diff2 <- diff(p.train,2)
plot(p.train.diff2);
acf(p.train.diff2);
tseries::adf.test(p.train.diff2)
eacf(p.train.diff2);

#####################################
# Arima(p,2,q) models
#####################################
# arima(1,2,4)
p.train.arimaA <- Arima(p.train,order=c(1,2,4))
summary(p.train.arimaA);
Box.test(p.train.arimaA$res, lag=10, type="Ljung-Box",fitdf=5);
qqnorm(p.train.arimaA$residual);
hist(p.train.arimaA$residual);

# arima(0,2,9)
p.train.arimaB <- Arima(p.train,order=c(0,2,9))
summary(p.train.arimaB);
Box.test(p.train.arimaB$res, lag=10, type="Ljung-Box",fitdf=9);
qqnorm(p.train.arimaB$residual);
hist(p.train.arimaB$residual);

# arima(2,2,4)
p.train.arimaC <- Arima(p.train,order=c(2,2,4))
summary(p.train.arimaC)
Box.test(p.train.arimaC$res, lag=10, type="Ljung-Box",fitdf=6)
qqnorm(p.train.arimaC$residual);
hist(p.train.arimaC$residual);


# arima(2,2,3)
p.train.arimaD <- Arima(p.train,order=c(2,2,3))
summary(p.train.arimaD)
Box.test(p.train.arimaD$res, lag=10, type="Ljung-Box",fitdf=5)
qqnorm(p.train.arimaD$residual);
hist(p.train.arimaD$residual);

# arima(3,2,5)
p.train.arimaD <- Arima(p.train,order=c(3,2,5))
summary(p.train.arimaD)
Box.test(p.train.arimaD$res, lag=10, type="Ljung-Box",fitdf=5)
qqnorm(p.train.arimaD$residual);
hist(p.train.arimaD$residual);


#####################################
# Holt EMA model
#####################################
p.train.ema <- holt(p.train,initial="optimal")
summary(p.train.ema)
Box.test(p.train.ema$res, lag=10, type="Ljung-Box",fitdf=5)
qqnorm(p.train.ema$residual);
hist(p.train.ema$residual);


#########################
# best model forecast
#########################

################################
# autoarima
################################
p.train.arimaauto <- auto.arima(p.train)
p.train.arimaauto.df <- length(coef(p.train.arimaauto))
summary(p.train.arimaauto)
Box.test(p.train.arimaauto$res, lag=10, type="Ljung-Box",fitdf=p.train.arimaauto.df);
qqnorm(p.train.arimaauto$residual);
hist(p.train.arimaauto$residual);

print(p.train.arimaauto$aic)
print(p.train.arima1$aic)
print(p.train.arima2$aic)
print(p.train.arima3$aic)
print(p.train.arima4$aic)
print(p.train.arimaA$aic)
print(p.train.arimaB$aic)
print(p.train.arimaC$aic)

p.best_model <- p.train.arima3
p.best_model.df <- 3

p.test.fc <- forecast(p.train,model=p.best_model,h=length(p.test))
plot(p.test.fc)
