#### You may need to change the data file directory accordingly.
######## Q1

#a 
Employee=read.table(file="EmployeeData.txt",header=T)

attach(Employee)

lm.res=lm(metal~vendor)
summary(lm.res)
anova(lm.res)

#b
plot(vendor,metal)
lines(vendor,fitted(lm.res))

#c
plot(time,resid(lm.res))

#d
library(lmtest)
dwtest(metal~vendor)
#p-value < 2.2e-16, residuals are correlated

#e
N=nrow(Employee)
phi.hat=lm(lm.res$residual[1:N-1]~0+lm.res$residual[2:N])$coeff	
y.trans=metal[2:N]-phi.hat*metal[1:N-1]		
x.trans=vendor[2:N]-phi.hat*vendor[1:N-1]

coch=lm(y.trans~x.trans)	
summary(coch)
# Larger. The standard error of the estimate of beta1 from Cochrane procedure is 0.01297 while the one from OLS is 0.009423.   

########### Q2

#a

price=read.table(file="HomePrice.txt",header=T)

attach(price)

ols.res=lm(SalesPrice~homeft2+lotft2)
summary(ols.res)
anova(ols.res)

#b
plot(fitted(ols.res),resid(ols.res))

#c
r.abs=abs(resid(ols.res))
r.fitted=lm(r.abs~fitted(ols.res))$fitted.values

#d
w=1/(r.fitted^2)

wls.res=lm(SalesPrice~homeft2+lotft2,weights=w)
summary(wls.res)
anova(wls.res)

#e
plot(fitted(wls.res),resid(wls.res))







