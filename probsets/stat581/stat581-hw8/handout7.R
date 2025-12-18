
#We begin by calling the data for handout 7
library("readxl")
setwd("C:/Users/aneath/iCloudDrive/Lexar/stat581 fall2021")

h7.data = read_excel("handout7data.xlsx")
str(h7.data)


#We will need these packages to work with blocks as random effects
library("lme4")
library("lmerTest")

#We will need this package for performing Fisher comparisons and groupings
library("multcomp")


#Example 7.1 
#An experiment is conducted to compare two tips used on a hardness testing machine
#The experimental units are the metal specimens. Each tip gives a hardness measurement for each specimen. 
#Thus, the data is from a paired comparisons design
tip1 = na.omit(h7.data$tip1)
tip2 = na.omit(h7.data$tip2)

#The built-in function t.test can be used to compute a paired comparisons analysis. 
t.test(tip1,tip2,paired = TRUE)

#Below is code for our own function to compute a paired comparisons analysis.
#Note that the data must be unstacked. (Each group has its own column of measurements.)
paired.test = function(y1,y2,alpha=.05) 
{
  d = y1 - y2
  
  n = length(d)
  d.bar = mean(d)
  s.d = sd(d)
  SE = s.d/sqrt(n)
  
  t.0 = d.bar / SE
  p.value = 2*pt(abs(t.0),df=n-1,lower.tail = FALSE)
  
  t.mult = qt(alpha/2,lower.tail = FALSE, df=n-1)
  lower.est = d.bar - t.mult*SE
  upper.est = d.bar + t.mult*SE
  
  table1 = matrix(c(n,d.bar,s.d),nrow = 1)
  dimnames(table1) = list(c(""),c("sample.size","mean.diff","sd.diff"))
  print(table1)
  
  table2 = matrix(c(t.0,p.value),nrow = 1)
  dimnames(table2) = list(c(""),c("test statistic","p-value"))
  print(table2)
  
  table3 = matrix(c(d.bar,lower.est,upper.est),nrow = 1)
  dimnames(table3) = list(c(""),c("estimated difference","lower limit","upper limit"))
  print(table3,digits = 3)
  
}

paired.test(tip1,tip2)

#The variables below define the same data, only with responses stacked in one column, 
#identified by treatment (machine tip) and block (specimen).
trtmnt = as.factor(na.omit(h7.data$t))
block = as.factor(na.omit(h7.data$s))
y = na.omit(h7.data$h)

#We could use aov to compute F for a randomized block design instead of computing t for a paired comparisons. 
#Note that t^2=F, so the p-values are the same. 
rcbd.mod = aov(y ~ block + trtmnt)
summary(rcbd.mod)

interaction.plot(block,trtmnt,y)


#Example 5.2
#A medical device manufacturer produces vascular grafts (artificial veins). 
#An experiment is planned to investigate the effect of extrusion pressure on yield (proportion of acceptable grafts).
#The resin used in the production is from an external supplier and may differ from batch to batch.

#Each batch is tested at each of the extrusion pressures. 
#The batches are randomly selected, and have no identifiable features that can be used for modeling purposes.
pressure = as.factor(na.omit(h7.data$pressure))
batch = as.factor(na.omit(h7.data$batch))
success = na.omit(h7.data$yield)

#Graph the data. 
#Remember that we are testing whether pressure differences are generalizable to a larger population of batches. 
#Thus, we are testing how the pressure effect depends on the batch.
interaction.plot(batch,pressure,success)

#Use contrasts to define parameter restrictions for the fixed effect in the model
contrasts(pressure)=contr.sum

#This is how the lme4 package defines a model with random effects. 
#The 1 in front of batch signifies that batch levels are randomly selected from a common distribution.
random.mod = lmer(success ~ (1|batch) + pressure)

#The anova command is used to compute the test for fixed effects.
anova(random.mod)

#We can perform pairwise comparisons, here using the Fisher LSD method
comps = glht(random.mod,linfct = mcp(pressure="Tukey"))
summary(comps,test=univariate())
cld(summary(comps,test=univariate()))
plot(cld(summary(comps,test=univariate())))

#The following code is used for computing the variance components estimates, and fixed effect parameter estimates.
summary(random.mod)

#Since the fixed effect estimates must sum to 0, the estimate at level a=4 is the negative of the sum of the
#fixed effect estimates at levels 1 through a-1. (The first parameter estimate is for the overall mean.)
estimates = summary(random.mod)
estimates.pressure = c(estimates$coefficients[1:4,1],0-sum(estimates$coefficients[2:4,1]))
estimates.pressure