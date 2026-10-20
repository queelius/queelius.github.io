
#We begin by calling the data for handout 9
library("readxl")
setwd("C:/Users/aneath/iCloudDrive/Lexar/stat581 fall2021")

h9.data = read_excel("handout9data.xlsx")
str(h9.data)

#We will need these packages to model random effects
library("lme4")
library("lmerTest")

#We will need these packages for performing multiple comparisons
library("multcomp")
library("lsmeans")

#Example 9.1
#A manufacturing engineer is studying the dimensional variability of a particular component.
#The component is produced on three machines. Each machine has two spindles. 
#A sample of n=4 is selected from each of the spindles.
machine = as.factor(na.omit(h9.data$machine))
spindle = as.factor(na.omit(h9.data$spindle))
dimension = na.omit(h9.data$dimension)

#Use contrasts to define parameter restrictions for the fixed effects in the model
contrasts(machine)=contr.sum
contrasts(spindle)=contr.sum

#To fit a model with a nested fixed effect, we use the / notation within the aov command.
nested.mod = aov(dimension ~ machine/spindle)
summary(nested.mod)

#The following command is used to compute parameter estimates
dummy.coef(nested.mod)

#We can organize the parameter estimates with the following commands
estimates = dummy.coef(nested.mod)
estimates$machine
estimates$`machine:spindle`[c(1,4)]
estimates$`machine:spindle`[c(2,5)]
estimates$`machine:spindle`[c(3,6)]


#The following commands are used to perform pairwise comparisons between machines.
#The glht command should be familiar from previous sections on multiple comparisons.
compare.machine = glht(nested.mod,linfct = mcp(machine="Tukey"))
c.m = summary(compare.machine,test=adjusted("none"))
c.m
cld(c.m)

#We can use the multcomp package to perform multiple comparisons on the machine*spindle treatment combinations,
#as we did for pairwise comparisons in a two-way ANOVA interaction model.
#Instead, we will use the lsmeans package
#lsmeans is an R package that is new to us. A related version is called emmeans, for estimated marginal means.

#For simplicity, let's only focus on those comparisons involving spindles within the same machine
interaction.mod = aov(dimension~machine*spindle)
lsmeans(interaction.mod, pairwise ~ machine:spindle, adjust = "none")


#Example 9.2
#We wish to investigate whether the variability in purity is attributable to differences in suppliers. 
#Four batches of raw material are selected from each supplier (fixed factor A), 
#and three measurements of purity are made on each batch (nested random factor B(A)).
supplier = as.factor(na.omit(h9.data$supplier))
batch = as.factor(na.omit(h9.data$batch))
purity = na.omit(h9.data$purity)

#options define the model parameters. lmer fits the model; supplier is a fixed effect, 
#batch within supplier is a random effect.  
contrasts(supplier)=contr.sum
random.mod = lmer(purity ~ supplier + (1|batch:supplier))

#summary prints the parameter estimates, including the estimates of batch variance and measurement variance.
summary(random.mod)

#anova performs the test for a fixed effect. 
#R performs testing in models with random effects slightly different from our development in the notes.
#But in the example here, the computation of the F statistics matches that from our notes.
anova(random.mod)

#Suppose we want R to give us the results we developed in the notes. 
#Below is a user defined function to perform those computations.
nested.test = function(A,B,y)
{
  av=anova(lm(y~A/B))
  ss.A = av$`Sum Sq`[1]
  ss.B = av$`Sum Sq`[2]
  ss.error = av$`Sum Sq`[3]
  df.A = av$Df[1]
  df.B = av$Df[2]
  df.error = av$Df[3]
  ms.A = ss.A / df.A
  ms.B = ss.B / df.B
  ms.error = ss.error / df.error
  F.a = ms.A / ms.B
  p.value = pf(F.a,df1=df.A,df2=df.B,lower.tail = FALSE)
  table1 = matrix(c(ss.A,ss.B,ss.error,
                    df.A,df.B,df.error,
                    ms.A,ms.B,ms.error),nrow = 3)
  dimnames(table1) = list(c("Fixed Effect A","Random Effect B(A)","Error"),
                          c("SS","df","MS"))
  print(table1)
  
  table2 = matrix(c(F.a,p.value),nrow = 1)
  dimnames(table2) = list(c(""),c("F-test for fixed effect","p-value"))
  print(table2)
  
  a=nlevels(A)
  b=nlevels(B)
  n=length(y) / a / b
  
  var.hat = ms.error
  var.B.hat = (ms.B - ms.error) / n
  
  table3 = matrix(c(var.hat,var.B.hat),nrow=1)
  dimnames(table3) = list(c(""),c("error.var","B.var"))
  print(table3)
  
}

#Below is the call to perform a test for the factor A effect using the function nested.test.
#Factor A is the fixed factor (entered first), factor B is the nested random factor (entered next), in the call.
nested.test(supplier,batch,purity)





