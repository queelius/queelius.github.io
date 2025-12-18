
#We begin by calling the data for handout 8
library("readxl")
setwd("C:/Users/aneath/iCloudDrive/Lexar/stat581 fall2021")

h8.data = read_excel("handout8data.xlsx")
str(h8.data)


#We will need these packages to work with blocks as random effects
library("lme4")
library("lmerTest")


#Example 8.1
#operator serves as a factor with a fixed number of levels, 
#part serves as a random factor/block with levels selected at random from a larger population. 
#The response variable is m, the measured dimension of a part. 
#We have multiple measurements from each operator on each selected part.

operator = as.factor(na.omit(h8.data$operator))
part = as.factor(na.omit(h8.data$part))
m = na.omit(h8.data$measurement)

#Graph the data. 
#Remember that we are testing whether operator differences are generalizable to a larger population of parts.
interaction.plot(part,operator,m)

#Use contrasts to define parameter restrictions for the fixed effect in the model
contrasts(operator)=contr.sum

#To fit a model with both fixed effects and random effects, we use lmer (linear mixed effects in R).
#The code below defines operator as a fixed effect, part as a random effect selected from a distribution, 
#and operator*part interaction as an effect from its own distribution
mixed.mod = lmer(m ~ operator + (1|part) + (1|operator:part))

anova.mod = lm(m ~ operator + part)

#The anova command is used to compute the test for fixed effects.
#The test here is not quite the same as what we developed in lecture. 
anova(mixed.mod)

#The following code is used for computing the variance components estimates, and fixed effect parameter estimates.
#Because the interaction effect is small, the estimate for the interaction variance is near zero, 
#difficulties with the computing algorithm.
summary(mixed.mod)

#Suppose we want R to give us the results we developed in the notes. 
#Below is a user defined function to perform those computations.

mixed.test = function(A,B,y)
{
  av=anova(lm(y~A*B))
  F.a = av$`Mean Sq`[1]/av$`Mean Sq`[3]
  p.value = pf(F.a,df1=av$Df[1],df2=av$Df[3],lower.tail = FALSE)
  table1 = matrix(c(av$`Sum Sq`[1],av$`Sum Sq`[2],av$`Sum Sq`[3],av$`Sum Sq`[4],
                    av$Df[1],av$Df[2],av$Df[3],av$Df[4],
                    av$`Mean Sq`[1],av$`Mean Sq`[2],av$`Mean Sq`[3],av$`Mean Sq`[4]),nrow = 4)
  dimnames(table1) = list(c("Fixed Effect A","Random Effect B","Interaction AB","Error"),
                          c("SS","df","MS"))
  print(table1)
  
  table2 = matrix(c(F.a,p.value),nrow = 1)
  dimnames(table2) = list(c(""),c("F-test for fixed effect","p-value"))
  print(table2)
  
  a=nlevels(A)
  b=nlevels(B)
  n=length(y) / a / b
  
  var.hat = av$`Mean Sq`[4]
  var.interaction.hat = (av$`Mean Sq`[3]-av$`Mean Sq`[4])/n
  var.block = (av$`Mean Sq`[2]-av$`Mean Sq`[3])/n/a
  
  table3 = matrix(c(var.hat,var.interaction.hat,var.block),nrow=1)
  dimnames(table3) = list(c(""),c("error.var","interaction.var","block.var"))
  print(table3)
}

#Below is the call to perform a test for the factor A effect using the function mixed.test.
#Factor A is the fixed factor (entered first), factor B is the random factor (entered next), in the call.
mixed.test(operator,part,m)


#The variables below represent the same data, only with repeat measurements summarized by their sample mean.
o = as.factor(na.omit(h8.data$o))
p = as.factor(na.omit(h8.data$p))
means = na.omit(h8.data$means)

#Below we run a randomized block design with the sample means as the response variables.
#Note that the test for a fixed factor effect is equivalent to that from the mixed.test function.
rcbd.mod = lmer(means ~ o + (1|p))
anova(rcbd.mod)



#Example 8.2
#An experiment is conducted to investigate the effects of furnace temperature and furnace position.
#The response variable is the baked density of a carbon anode. 
#Furnace temperature (A) is a fixed factor is three levels. 
#Furnace position is a random factor, with two randomly selected levels.
temp = as.factor(na.omit(h8.data$temperature))
pos = as.factor(na.omit(h8.data$position))
density = na.omit(h8.data$density)

#Graph the data
#Note that no matter how many repeat measurements we may take at a specific furnace position, 
#the pertinent sample size is b=2, the number of randomly selected furnace positions.
interaction.plot(pos,temp,density)

#Run mixed.test to test for a temperature effect
mixed.test(temp,pos,density)

#We get an incorrect result if we run the analysis as a two-factor ANOVA
#In this example, the temperature effect is clear.
#But in other cases, the use of an incorrect test statistic may overstate the evidence
aov.mod = aov(density~temp*pos)
anova(aov.mod)










