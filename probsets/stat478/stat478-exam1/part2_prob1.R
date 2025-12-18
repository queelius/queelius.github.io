# Regression for Time Series Data
emp_data <- read.table("EmployeeData.txt", header=TRUE)

plot.ts(emp_data)
N=nrow(emp_data)
t=1:N

# fit a multiple regression model
ols.fit <- lm(metal~vendor,data=emp_data)
summary(ols.fit)
plot(t,ols.fit$residual)
acf(ols.fit$residual)

install.packages("lmtest")	#download required packages for Durbin-Watson test
library(lmtest)
dwtest(ols.fit)		#Durbin-Watson test for autocorrelation in residuals

phi.hat=lm(ols.fit$residual[2:N]~0+ols.fit$residual[1:N-1])$coeff	#calculte phi fot the Cochrane Method
y.trans=emp_data$metal[2:N]-phi.hat*emp_data$metal[1:N-1]		#Transform y and x according to the Cochrane Method
x.trans=emp_data$vendor[2:N]-phi.hat*emp_data$vendor[1:N-1]

coch.or=lm(y.trans~x.trans)		#Fit OLS regression with transformed data
summary(coch.or)
acf(coch.or$residual)
dwtest(coch.or)			#Durbin-Watson test for autocorrelation in residuals after the Cochrane Method

