hpdata=read.table(file="hp_data.txt")
colnames(hpdata)=c("ID","HP","HOME","LOT")

#### Regression Analysis Using lm() ####
result=lm(HP~HOME+LOT, data=hpdata)		 # fit a multiple regression model
summary(result)	#get details from the regression output
anova(result)		#get the anova table

# residual analysis

# the histogram of the residuals at least suggests zero mean, as it is
# symmetric around 0.
hist(result$residual)

# the following plot of the fitted values vs the residuals suggests
# non-constant variance. the variance seems to be increasing with respect
# to y. the residuals do seem to have a zero mean though.
plot(result$residual,result$fitted)

# variance is also increas with respect to home price.
plot(result$residual,hpdata$HOME)

# i can't really make anything out for the lot.
plot(result$residual,hpdata$LOT)

# the acf of the residuals shows a high degree of autocorrelation. every lag
# is outside of the boundary.
acf(result$residual)








#####
abs_residuals = abs(residuals(result))
#abs_residuals = residuals(result)
residuals_fit=lm(abs_residuals^2~result$fitted)

# weighted least squares
# s(i)^2 = g0 + g1 y_hat(i) + zeta(i), zeta(i) is the error
wts=1/abs(fitted(residuals_fit))
result.weighted=lm(HP~HOME+LOT, data=hpdata,weights=wts)

anova(result.weighted)
summary(result.weighted)

acf(result.weighted$residual)
acf(result$residual)




r = rstudent(result.weighted)	#Get the studentized residual

plot(r,result.weighted$fitted)



