#below is code for investigating a general association effect by comparing
#expected cell counts under the independence model to observed cell count

#enter the data for the vaccine/antibody example
obs = matrix(c(25,8,5,6,18,11),nrow = 2,byrow = TRUE)
dimnames(obs) = list(treatment=c("placebo","vaccine"),
                     hia.level=c("small","moderate","large"))
print(obs)

#define the row size and the column size for the table
I = dim(obs)[1]
J = dim(obs)[2]

#compute sample proportions for column response given row input
row.sum = apply(obs,1,sum)
row.prob = sweep(obs,1,row.sum,"/")
print(row.prob,digits = 3)

#we will use the R function chisq.test to compute the X2 statistic, 
#and compute the expected cell counts
result = chisq.test(obs,correct = FALSE)
result

expected = result$expected

#compute G2 from the observed counts and the expected counts
G2 = 2*sum(obs*log(obs/expected))
df = (I-1)*(J-1)
p.value = pchisq(G2,df,lower.tail = FALSE)

print(G2)
print(df)
print(p.value)

#display the expected cell counts, and a table comparing expected and observed
# 1 if obs>expected,  -1 if obs<expected
print(expected,digits = 5)

sgn = sign(obs-expected)
print(sgn)




######################

#here is the code for the schooling / opinion example
obs = matrix(c(209,101,237,151,126,426,16,21,138),nrow = 3,byrow = TRUE)
dimnames(obs) = list(yrs.school=c("less than HS","high school","more than HS"),
                     opinion=c("disapprove","middle","approve"))
print(obs)

I = dim(obs)[1]
J = dim(obs)[2]

result = chisq.test(obs)
result

expected = result$expected

G2 = 2*sum(obs*log(obs/expected))
df = (I-1)*(J-1)
p.value = pchisq(G2,df,lower.tail = FALSE)

print(G2)
print(df)
print(p.value)

print(expected,digits = 4)

#notice how the positive signs indicate a positive association
sgn = sign(obs-expected)
print(sgn)

#an estimate for gamma can be used to measure the effect size
library("MESS")

gk.result = gkgamma(obs)
print(gk.result$conf.int,digits = 3)



################################


#here is the code for the living arrangement / cancer detection exampl
obs = matrix(c(59,85,109,100,53,36),nrow = 3,byrow = TRUE)
dimnames(obs) = list(living.arrangement=c("alone","spouse","others"),
                     cancer.stage=c("advanced","local"))

print(obs)

#because the data is from a retrospective study, we are interested in the 
#conditional probability of input row given response column
I = dim(obs)[1]
J = dim(obs)[2]

col.sum = apply(obs,2,sum)
col.prob = sweep(obs,2,col.sum,"/")
print(col.prob,digits = 3)

result = chisq.test(obs)
result

expected = result$expected

G2 = 2*sum(obs*log(obs/expected))
df = (I-1)*(J-1)
p.value = pchisq(G2,df,lower.tail = FALSE)

print(G2)
print(df)
print(p.value)

print(expected,digits = 5)


#from comparing observed to expected, we see that those living alone are more likely to detect
#cancer while it is still at the local stage, and those living with a spouse or others are more likely to detect
#cancer only after it has reached an advanced stage
sgn = sign(obs-expected)
print(sgn)



















