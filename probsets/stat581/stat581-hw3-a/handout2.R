#We will use the same approach for reading in the data
library("readxl")
setwd("C:/Users/aneath/iCloudDrive/Lexar/stat581 fall2021")

h2.data = read_excel("handout2data.xlsx")
str(h2.data)


#Example 2.1 from class
#An engineer wishes to investigate the relationship between RFpower setting and etch rate
#for a tool used in creating circuit patterns

#The data is stacked, so we define a variable for the response (rate) and a factor variable for the input (rfp)
rate = na.omit(h2.data$rate)
rf.power = na.omit(h2.data$`rf power`)
rfp=as.factor(rf.power)

#Create a boxplot of the data as a first method of comparison
boxplot(rate ~ rfp)

#We can compute the response variable summary statistics for each group using the following code
means = by(rate,rfp,mean)
variances = by(rate,rfp,var)
sample.size = by(rate,rfp,length)

#The following is used to combine columns in R. Here we do so for a more convenient display
cbind(means,variances,sample.size)

#We can use computations in R to demonstrate the ANOVA computing formulas
#The sample size from each factor level is n=5 for this example
MStr = 5*var(means)
MSe = mean(variances)

F.star = MStr / MSe
p.value = pf(F.star,4,16,lower.tail = FALSE)

table = matrix(c(MStr,MSe,F.star,p.value),nrow = 1)
dimnames(table) = list(c(""),c("MStr","MSE","F statistic","p value"))
print(table)



#Example 2.2 from class
#Five brands of a synthetic wood veneer are to be investigated using a friction test.
#The response variable is a measure of wear.

#We cam use as.factor and na.omit in the same command
brand = as.factor(na.omit(h2.data$brand))
wear = na.omit(h2.data$wear)

#Below is the call for computing the ANOVA table using built-in R functions
model2 = aov(wear~brand)
summary(model2)

#Let's investigate the relationship between brand and wear graphically
boxplot(wear~brand)

#To perform multiple comparisons between groups, we will use the multcomp package in R (use install.packages if needed)
library("multcomp")

#glht is an abbreviation for general linear hypothesis test
#linfct defines the test we wish to perform 
#by selecting Tukey, we indicate we want to perform pairwise comparisons (the reason for calling this Tukey will come later)
#summary gives a table of t statistics and p-values for the pairwise comparison tests

model2.lsd = glht(model2, linfct = mcp( brand = "Tukey"))
summary(model2.lsd,test=univariate())

#cld is an abbreviation for compact letter display, and may be used to give a summary of the multiple comparison
cld(summary(model2.lsd,test=univariate()))

#A table of p-values may also be used as a summary of multiple comparison results
pairwise.t.test(wear, brand, p.adjust.method = "none")




#Example 2.3 from class
#A pharmaceutical manufacturer wants to investigate the bioactivity of a new drug.
#A CRD is conducted, usign three dosage levels

#Read in the data (note that the data is unstacked)
low.dose = na.omit(h2.data$`20g`)
mid.dose = na.omit(h2.data$`30g`)
high.dose = na.omit(h2.data$`40g`)

#To use built-in R commands, we need to create a data frame containing stacked data
dose.data = data.frame(cbind(low.dose,mid.dose,high.dose))
dose.data

#For dose.stacked, the response variable is called values, the factor variable is called ind
dose.stacked = stack(dose.data)
dose.stacked

#Compute the ANOVA table, specifying the use of dose.stacked as the data frame
model3 = aov(values~ind, data=dose.stacked)
summary(model3)

#Suppose we are interested in estimating ANOVA model parameters.The specific estimates depend on the model form
#The default is to set the first treatment level effect parameter equal to zero
dummy.coef(model3)

#The following commands give the parameter estimates for the model definition we gave in class
#contr.sum sets the sum of the treatment level effects equal to zero
contrasts(dose.stacked$ind)=contr.sum
model3b = aov(values~ind,data=dose.stacked)
dummy.coef(model3b)
