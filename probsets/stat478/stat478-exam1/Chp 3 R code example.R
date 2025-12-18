#The following dataset is used to predict students' grade-point averages at the end of their freshman year by entrance test scores and high school class rank. 
GPAdata=read.table(file="http://users.stat.ufl.edu/~rrandles/sta4210/Rclassnotes/data/textdatasets/KutnerData/Appendix%20C%20Data%20Sets/APPENC04.txt")
colnames(GPAdata)=c("ID","GPA","Rank","ACT","Year")

pairs(GPA~Rank+ACT,GPAdata, main="Scatterplot Matrix")		#construct scatterplots 

#### Regression Analysis Using lm() ####
result=lm(GPA~Rank+ACT, data=GPAdata)		 # fit a multiple regression model
summary(result)	#get details from the regression output
anova(result)		#get the anova table
aov(result)			#Another slightly different anova table


####Residual Analysis
qqnorm(result$residual)		#construct qq plot and histgram for the residuals
hist(result$residual)
r = rstudent(result)	#Get the studentized residual
plot(r,result$fitted)	#Plot studentized residual vs fitted values
plot(r,GPAdata$Rank)
plot(r,GPAdata$ACT)

####Weighted Least Squares
wts=1/fitted(lm((residuals(result))^2~result$fitted))
result.weighted=lm(GPA~Rank+ACT, data=GPAdata,weights=wts)
r = rstudent(result.weighted)	#Get the studentized residual
plot(r,result.weighted$fitted)

###Regression for Time Series Data #############
dat=read.table("http://www.stat.ufl.edu/~rrandles/sta4210/Rclassnotes/data/textdatasets/KutnerData/Chapter%2012%20Data%20Sets/CH12TA02.txt",col.names=c("CSales","ISales"))

plot.ts(dat)
N=nrow(dat)
t=1:N
ols.fit=lm(CSales~ISales,data=dat)		#run an OLS regression on Airpassenger data
summary(ols.fit)
plot(t,ols.fit$residual)
acf(ols.fit$residual)

install.packages("lmtest")	#download required packages for Durbin-Watson test
library(lmtest)
dwtest(ols.fit)		#Durbin-Watson test for autocorrelation in residuals


phi.hat=lm(ols.fit$residual[2:N]~0+ols.fit$residual[1:N-1])$coeff	#calculte phi fot the Cochrane Method
y.trans=dat$CSales[2:N]-phi.hat*dat$CSales[1:N-1]		#Transform y and x according to the Cochrane Method
x.trans=dat$ISales[2:N]-phi.hat*dat$ISales[1:N-1]

coch.or=lm(y.trans~x.trans)		#Fit OLS regression with transformed data
summary(coch.or)
acf(coch.or$residual)		
dwtest(coch.or)			#Durbin-Watson test for autocorrelation in residuals after the Cochrane Method




