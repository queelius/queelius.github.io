attach(mtcars) 
######## a ##########
pairs(mpg~hp+wt, main="Scatterplot Matrix")
# mpg is linearly negatively related to hp and wt.
	
######## b ##########
fit=lm(mpg~hp+wt)
summary(fit)
anova(fit) 
######## c ##########
#F test has p-value: 9.109e-12, the overall model is significant
######## d ##########
# t-tests on individual beta both have small p-values. The individual betas are different from 0, and thus cannot be dropped.

######## e ##########
qqnorm(fit$residuals)
hist(fit$residuals)
#the residuals have a roughly normal distrubution
######## f ##########
fit.reduced=lm(mpg~hp)
anova(fit.reduced) 	#SSR(x1)=678.37
#SSR(x2 | x1)= 252.63 from part b
#SSR(x1, x2) = 931
######## g ##########
predict(fit,newdata=data.frame(hp=100,wt=4),se.fit=T,interval = "confidence")  
# We are 96% confident, the mean mpg for cars with hp 100 and weight 4 tons is between 16.58947 and 20.48784 miles.