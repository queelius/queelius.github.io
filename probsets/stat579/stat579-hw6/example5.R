#relative.risk is a function for computing an interval estimate of the relative risk
#the function takes an input of cell counts, then outputs interval estimates for both log(RR) and RR
#the data table may be from a cross-sectional study or a prospective study
relative.risk = function(counts,conf.level=.95){
  alpha = 1-conf.level
  n11 = counts[1,1]
  n1 = sum(counts[1,])
  n21 = counts[2,1]
  n2 = sum(counts[2,])
 
  rr.hat = (n11/n1) / (n21/n2)
  sig.hat = sqrt(1/n11 - 1/n1 + 1/n21 - 1/n2)
  lower.log = log(rr.hat) - qnorm(alpha/2,lower.tail = FALSE)*sig.hat
  upper.log = log(rr.hat) + qnorm(alpha/2,lower.tail = FALSE)*sig.hat
  lower.RR = exp(lower.log)
  upper.RR = exp(upper.log)
  
  RR.table = matrix(c(log(rr.hat),sig.hat,lower.log,upper.log,
                      rr.hat,NA,lower.RR,upper.RR),byrow=TRUE,nrow = 2)
  dimnames(RR.table) = 
    list(c("Log.RelativeRisk","RelativeRisk"),c("estimate","ASE","lower.limit","upper.limit"))
  print(RR.table,digits = 4)
}


#data for the aspirin / heart attach prospective study
prospec = matrix(c(189,10845,104,10933),nrow = 2,byrow = TRUE)
dimnames(prospec) = list(treatment = c("placebo","aspirin"),heart.attack = c("attack.yes","attack.no"))

#data as a table of counts
print(prospec)

#compute interval estimate for log(RR) and relative risk RR
relative.risk(prospec)


#odds.ratio is a function for computing an interval estimate of the odds ratio
#the function takes an input of cell counts, then outputs interval estimates for both log(theta) and theta
#the data table may be from a cross-sectional study, a prospective study, or a retrospective study
odds.ratio = function(counts,conf.level=.95){
  alpha = 1-conf.level
  n11 = counts[1,1]
  n12 = counts[1,2]
  n21 = counts[2,1]
  n22 = counts[2,2]
  
  theta.hat = n11*n22/n12/n21
  sig.hat = sqrt(1/n11 + 1/n12 + 1/n21 + 1/n22)
  lower.log = log(theta.hat) - qnorm(alpha/2,lower.tail = FALSE)*sig.hat
  upper.log = log(theta.hat) + qnorm(alpha/2,lower.tail = FALSE)*sig.hat
  lower.theta = exp(lower.log)
  upper.theta = exp(upper.log)
  
  theta.table = matrix(c(log(theta.hat),sig.hat,lower.log,upper.log,
                      theta.hat,NA,lower.theta,upper.theta),byrow=TRUE,nrow = 2)
  dimnames(theta.table) = 
    list(c("Log.OddsRatio","OddsRatio"),c("estimate","ASE","lower.limit","upper.limit"))
  print(theta.table,digits = 4)
}


#data for the oc/mi retrospective study
retro = matrix(c(23,34,35,132),nrow = 2,byrow = TRUE)
dimnames(retro) = list(c("o.c.used","o.c.never"),c("m.i.yes","m.i.no"))
names(dimnames(retro)) = c("contraceptives","myocardial infarction")

#data as a table of counts
print(retro)

#compute interval estimate for log(theta) and odds ratio theta
odds.ratio(retro)


#we will use the R package MESS to compute an interval estimate for gamma
#if you have not already done so, you will need run install.packages("MESS")

#you only need to install an R package once.
#you do need to load the package for each new session
library("MESS")

#enter the data as a table of counts from cross sectional sampling
counts = matrix(c(20,24,80,82,22,38,104,125,13,28,81,113,7,18,54,92),nrow = 4,byrow = TRUE)

#perform the calculations for estimating the gamma correlation coefficient
#you can access the computing results with gk.result$
gk.result = gkgamma(counts)

#get the estimate (gamma.hat)
gamma.hat = gk.result$estimate
gamma.hat

#get the standard error. ASE for gamma.hat is computed using the Delta Method
ASE = gk.result$se1
ASE

#get the interval estimate
gk.result$conf.int

#the above interval estimate matches the equation based on the normal distribution
gamma.lower = gamma.hat - 1.96*ASE
gamma.upper = gamma.hat + 1.96*ASE
print(c(gamma.lower,gamma.upper))
