
#We will use the same data as for handouts 1 and 2 when illustrating confidence intervals
library("readxl")
setwd("C:/Users/aneath/iCloudDrive/Lexar/stat581 fall2021")

h3a.data = read_excel("handout1data.xlsx")
str(h3a.data)
h3b.data = read_excel("handout2data.xlsx")
str(h3b.data)




#Example 3.1 (refer back to Example 1.1 on comparing modified and unmodified cement mortar)
modified = na.omit(h3a.data$modified)
unmod = na.omit(h3a.data$unmod)

#Below is code for an R function to compute a two sample confidence interval for the difference in means
two.sample.interval = function(y1,y2,alpha=.05) 
{
  n1 = length(y1)
  n2 = length(y2)
  ybar1 = mean(y1)
  ybar2 = mean(y2)
  s1 = sd(y1)
  s2 = sd(y2)
  ybar.diff = ybar1-ybar2
  s.p = sqrt( ((n1-1)*s1^2+(n2-1)*s2^2) / (n1+n2-2) )
  SE = s.p*sqrt(1/n1 + 1/n2)
  
  t.mult = qt(alpha/2,lower.tail = FALSE, df=n1+n2-2)
  lower.est = ybar.diff - t.mult*SE
  upper.est = ybar.diff + t.mult*SE
  
  table1 = matrix(c(ybar1,ybar2,s1,s2,s.p),nrow = 1)
  dimnames(table1) = list(c(""),c("ybar1","ybar2","s1","s2","Sp"))
  print(table1)
  
  table2 = matrix(c(ybar.diff,lower.est,upper.est),nrow = 1)
  dimnames(table2) = list(c(""),c("estimated difference","lower limit","upper limit"))
  print(table2,digits = 3)
  
}

#Apply the function to the cement data
two.sample.interval(modified,unmod)

#We could also use the built-in R function
t.test(modified,unmod,var.equal = TRUE)





#Example 3.2 (refer back to Example 1.2 on comparing two filling machines)
#Remember that the data here is stacked, so we have a factor input and a continuous response
machine = as.factor(na.omit(h3a.data$machine))
output = na.omit(h3a.data$output)

two.sample.interval(output[machine==1],output[machine==2])
t.test(output~machine,var.equal=TRUE)





#Example 3.3 
#An experiment is conducted to investigate the relationship between baking temperature and wafer thickness
#(photoresist is a light-sensitive material applied to semiconductor wafers so that a circuit pattern can be imaged)
#The data is unstacked

thickness.lowtemp = na.omit(h3a.data$`95C`)
thickness.hightemp = na.omit(h3a.data$`100C`)

boxplot(thickness.lowtemp,thickness.hightemp,
        names = c("95 C","100 C"),xlab="baking temperature",ylab="wafer thickness")
#Note how the interval estimate is wide, due to the high variance in the data
t.test(thickness.lowtemp,thickness.hightemp,var.equal = TRUE)
two.sample.interval(thickness.lowtemp,thickness.hightemp)





#Example 3.4
#An experiment is conducted to investigate the relationship between flow rate and etch uniformity
#(the experimental unit is silicon wafers used in integrated circuit manufacturing)
#The data is stacked

etch = na.omit(h3a.data$observed)
flow = as.factor(na.omit(h3a.data$flow))

boxplot(etch~flow,
        names = c("125","200"),xlab="flow rate",ylab="etch uniformity")
#Note how the interval estimate includes zero, yet favors greater etch uniformity for higher flow rate
t.test(etch~flow,var.equal = TRUE)
two.sample.interval(etch[flow==125],etch[flow==200])






#Example 3.5 (refer back to Example 2.2 on comparing veneer brands)
brand = as.factor(na.omit(h3b.data$brand))
wear = na.omit(h3b.data$wear)

#define and compute the ANOVA model
model5 = aov(wear~brand)
summary(model5)

#We will use the multcomp package for computing confidence intervals comparing brand effects on wear
library("multcomp")

#Use glht, general linear hypothesis test for making pairwise comparisons
#Here we are comparing brand levels 1 and 2
lsd = glht(model5, linfct = mcp( brand = c(1,-1,0,0,0) ))
confint(lsd)

#Now we are using Tukey as the option for comparing all brand levels
comps = glht(model5,linfct = mcp(brand="Tukey"))
ci_lsd = confint(comps,calpha = univariate_calpha())

#display computed confidence intervals graphically and numerically, as well as t-stats and p-values
plot(ci_lsd)
ci_lsd
summary(ci_lsd)

#Here is some rough code for creating the Confidence interval plot from scratch
means = by(wear,brand,mean)
variances = by(wear,brand,var)
sample.size = by(wear,brand,length)

cbind(means,variances,sample.size)

MSe = mean(variances)

n = 4
a = nrow(means)
dfe = a*(n-1)

m.error = qt(.025,lower.tail = FALSE, df = dfe)*sqrt(MSe*2/n)

diff = rep(NA,times=a*(a-1)/2)
count = 1
for (i in 1:(nlevels(brand)-1)){
  for (j in (i+1):nlevels(brand))
  {diff[count]=(mean(wear[brand==levels(brand)[j]])-mean(wear[brand==levels(brand)[i]]))
  count = count+1}
}

CIs = as.matrix(cbind(diff,m.error))
Lower = CIs[,1]-CIs[,2]
Upper = CIs[,1]+CIs[,2]

matplot(rbind(Lower,Upper),rbind(1:10,1:10),type="l",lty=1, 
        xlab = "CI for difference in means",ylab = "pairwise comparison",col = "black" )
abline(v=0,col="red")








#Example 3.6 (refer again to the example comparing modified cement with unmodified cement)
#A difference in mean strength of 0.5 kgf/cm^2 has a practical impact on the use of the mortar.
#On the basis of prior experience, take sd=0.25. We want power of .90 for a level .05 test

#use built-in R function power.t.test (set the variable to solve for as NULL)
power.t.test(n=NULL,delta=0.5,sd=0.25,sig.level = .05,power = .90,type = "two.sample")

#Let's verify the power calculation, using the noncentral t distribution function
#qt computes quantiles, pt computes cumulative probabilities
n = c(6,7)
df = 2*(n-1)
sd = 0.25
delta = 0.5
ncp = sqrt(n/2)*(delta/sd)
alpha = .05
power = .90
1 - pt(qt(1-alpha/2,df),df,ncp) 



#Here is code for creating the power curve for the chosen sample size.
#You will need to enter the sample size as n, standard deviation as sd, level as alpha.
#Enter the power as h, a horizontal line. Enter the specific alternative as v, a vertical line
n=7
df = 2*(n-1)
sd = 0.25
alpha = .05
delta = seq(from=0,to=5*sd/sqrt(n/2),length.out = 1000)
power = 1 - pt(qt(1-alpha/2,df),df,ncp = sqrt(n/2)*(delta/sd))

plot(delta,power,type = "l",lwd=2,col="blue")
abline(h=.90,      
       col="red",  
       lwd=2)  
abline(v=0.5,
       col="green",                              
       lwd=2) 


#Example 3.7 (refer back to Example 1.2 on comparing filling machines)

machine = as.factor(na.omit(h3a.data$machine))
output = na.omit(h3a.data$output)
t.test(output~machine,var.equal=TRUE)
two.sample.interval(output[machine==1],output[machine==2])

power.t.test(n=NULL,delta=0.025,sd=0.028,sig.level = .05,power = .90,type = "two.sample")




#Example 3.8
#The goal of an experiment is to investigate the relationship between fluid type and lifetime
fluid = as.factor(na.omit(h3b.data$fluid))
life = na.omit(h3b.data$life)

#compute the ANOVA table for the observed data
summary(aov(life~fluid))
means = by(life,fluid,mean)
means

#Let's run a power analysis, based on specifying the maximum difference in the means
a=4
max.D = 3
s2 = 3.3

power.anova.test(groups=a,between.var = max.D^2/2/(a-1),within.var = s2,power = .9,sig.level = .05,n=NULL)


#Let's repeat the power analysis, this time based on specifying the group means

power.anova.test(groups=a,between.var = var(means),within.var = s2,power = .9,sig.level = .05,n=NULL)


#Let's verify the above calculation by demonstrating a simulation approach to computing power
n = 11
sim.size = 10000
decide.Ha = rep(NA,sim.size)
for (k in 1:sim.size){
  y1 = rnorm(n,means[1],sqrt(s2))
  y2 = rnorm(n,means[2],sqrt(s2))
  y3 = rnorm(n,means[3],sqrt(s2))
  y4 = rnorm(n,means[4],sqrt(s2))
  ybar1 = mean(y1)
  ybar2 = mean(y2)
  ybar3 = mean(y3)
  ybar4 = mean(y4)
  var1 = var(y1)
  var2 = var(y2)
  var3 = var(y3)
  var4 = var(y4)
  F.stat = n*var(c(ybar1,ybar2,ybar3,ybar4)) / mean(c(var1,var2,var3,var4))
  decide.Ha[k] = (F.stat>qf(.95,a-1,a*(n-1)))
  k = k+1
}
power = mean(decide.Ha)
power