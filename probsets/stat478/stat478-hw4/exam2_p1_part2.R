library(astsa)

tsdata <- ts(data=jj)
n <- length(tsdata)
ys <- exp((1/n)*sum(log(tsdata)))*log(tsdata)
plot(ys)
t <- 1:n
qt <- as.factor(rep(1:4,(n/4)))
q1 <- qt==1
q2 <- qt==2
q3 <- qt==3
m <- cbind(t,q1,q2,q3,ys)

# fit regression model to data
fit <- lm(ys~t+q1+q2+q3, data=m)
summary(fit)
library(latex2exp)
plot(ys,col="blue", pch=19,xlab=TeX("quarter ($t$)"),ylab=TeX("$y_t$"))
lines(fitted.values(fit),type="l")
legend(1,8,legend=c("data","model"),col=c("blue","black"),lty=1:2,cex=0.8)
df <- 5
sse <- sum(fit$residuals^2)
mse <- sse/(length(fit$residuals)-df)
mse_alt <-summary(fit)$sigma^2  # agrees with mse calculation above
plot(fit$residuals)
acf(fit$residuals)
hist(fit$residuals)
library(forecast)
seasonal <- ts(data=ys, frequency=4)
holt_model <- hw(seasonal, h=1, seasonal="additive", initial="optimal")
summary(holt_model)
library(latex2exp)
plot(seasonal,col="blue",pch=19,xlab=TeX("year ($t$)"),ylab=TeX("$y_t$"))
lines(holt_model$fitted)
legend(1,8,legend=c("data","fitted"),col=c("blue","black"),lty=1:2,cex=0.8)
