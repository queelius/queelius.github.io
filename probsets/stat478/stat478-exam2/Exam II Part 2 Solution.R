### 1.a
log_j=log(jj)
ts.plot(log_j)

### 1.b
t=1:84
qt=as.factor(rep(1:4,21))
lm.res=lm(log_j~t+qt)
summary(lm.res) ##Get the coefficients
ts.plot(log_j,lm.res$fitted,col=c("black","red"))

### 1.c
mse=summary(lm.res)$sigma^2		#0.01571906
plot(lm.res$residuals)
acf(lm.res$residuals)
qqnorm(lm.res$residuals)
hist(lm.res$residuals)


### 1.d
predict(lm.res,newdata=data.frame(t=85,qt="1"), interval = "prediction")
# 2.891687, 95%PI: (2.631515, 3.151859)
# undo the log transformation: 18.02369, 95%PI: (13.8948,23.37949)

### 1.e
library(forecast)
hw.add=hw(log_j, seasonal="additive")
summary(hw.add)	##Get the coefficients
ts.plot(log_j,hw.add$fitted,col=c("black","red"))

### 1.f
mse=0.08625182^2		#0.007439376
ts.plot(hw.add$residuals)
hist(hw.add$residuals)
acf(hw.add$residuals)

### 1.g
hw(log_j, seasonal="additive",h=1)
# 2.917244 95%PI: (2.748193, 3.086294)
# undo the log transformation: 18.49026 95%PI:(15.61439,21.89578)


#################################

### 2.i	Stationary
## ACF_k=(-0.4)^k
ARMAacf(ar=c(-0.4),lag.max=12)  ##look at some of the autocorreltations 
d1=arima.sim(n=250, list(ar = c(-0.4)))
plot.ts(d1)
acf(d1)

### 2.ii	Not Stationary
ARMAacf(ar = c(-0.9,1),ma=c(0.5,-0.4),lag.max=12)  ##look at some of the autocorreltations 
d2=arima.sim(n=250, list(order = c(1,1,2),ar = c(-0.9),ma=c(0.5,-0.4)))
plot.ts(d2)
acf(d2)

### 2.iii	Stationary
## ACF_1=0.944186, ACF_k=(0.9)^(k-1)*0.944186
ARMAacf(ar = c(0.9),ma=c(0.5),lag.max=12)  ##look at some of the autocorreltations 
d3=arima.sim(n=250, list(order = c(1,0,1),ar = c(0.9),ma=c(0.5)))
plot.ts(d3)
acf(d3)
