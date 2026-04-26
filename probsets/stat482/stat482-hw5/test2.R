

#We will use the same data as for handout 2 when illustrating contrasts and multiple comparisons
library("readxl")
#We will also need the R package for performing multiple comparisons
library("multcomp")
setwd("C:/Users/aneath/iCloudDrive/Lexar/stat581 fall2021")

h4.data = read_excel("handout2data.xlsx")
str(h4.data)

#Example 4.1 (refer back to Example 2.2 on comparing veneer brands)
brand = as.factor(na.omit(h4.data$brand))
wear = na.omit(h4.data$wear)

#Recall the ANOVA test for equal means. We want to investigate the relationship further.
aov.mod = aov(wear~brand)
summary(aov.mod)

#Here we are using glht to estimate a constrast defined as a comparison between the first 3 groups and the last 2 groups
con.test = glht(aov.mod, linfct = mcp( brand = c(2,2,2,-3,-3) ))

#summary is used to display the test results, confint is used to display the interval estimate
summary(con.test)
confint(con.test)

#The code below is used to define a set of orthogonal contrasts
contrasts(brand) = cbind( c(2,2,2,-3,-3),
                          c(1,1,-2,0,0),
                          c(1,-1,0,0,0),
                          c(0,0,0,1,-1))

#We can check the defined contrasts for brand. Read down each column for the corresponding contrast.
brand

#We will re-fit the ANOVA model, now with our own contrasts defined as above
contr.mod = aov(wear~brand)

#We can get a decomposition of sum squares into specific effects
#The command split is used to specify the decompositon. 
summary(contr.mod,split = list(brand=list("us-f"=1,"a-c"=2,"ac-aj"=3,"t-x"=4)))

#We can also use glht to estimate the specific contrast effects
contr = glht(aov.mod, linfct = mcp( brand = rbind( c(2,2,2,-3,-3),
                                                   c(1,1,-2,0,0),
                                                   c(1,-1,0,0,0),
                                                   c(0,0,0,1,-1)) ))

#adjusted none indicates that we are not adjusting for multiple tests
#univariate_calpha indicates that we are not adjusting for multiple intervals (c in calpha stands for critical value)
summary(contr,test = adjusted("none"))
confint(contr,calpha = univariate_calpha())


#Using the same example, let's investigate some pairwise mulitple comparisons.
#Remember that Tukey is used to define pairwise comparisons, as the Tukey method is best suited for such comparions
comparisons.mod = glht(aov.mod, linfct = mcp( brand = "Tukey"))

#Fisher LSD tests, error probability controlled for each comparison
summary(comparisons.mod,test=univariate())

#Tukey pairwise tests, error probability controlled across all comparisons
summary(comparisons.mod)

#We can summarize the results for Fisher and Tukey with a compact letter display
cld(summary(comparisons.mod,test=univariate()))
cld(summary(comparisons.mod))

#We can also display the data as a boxplot, with the Tukey groupings 
plot(cld(summary(comparisons.mod)))


#We could also use the TukeyHSD command with the model defined through aov (rather than glht)
TukeyHSD(aov.mod)
#A plot of the interval estimates with the Tukey adjustment
plot(TukeyHSD(aov.mod))


#We can use the Tukey Q distribution to check the above calculations
#For example, t = 2.694, p-adj = .1021
t0 = 2.694
a = 5
n = 4
df = a*(n-1)
ptukey(sqrt(2)*t0,a,df,lower.tail=FALSE)

#Here are the calculations for the least significant differences (margin of errors) for Fisher and Tukey
mse = .02083

m_lsd = qt(.025,df,lower.tail = FALSE)*sqrt(2*mse/n)
m_lsd

m_tukey = qtukey(.05,a,df,lower.tail = FALSE)*sqrt(mse/n)
m_tukey

#Here is the calculation for Tukey comparison error rate
2*pt(qtukey(.05,a,df,lower.tail = FALSE)/sqrt(2),df,lower.tail = FALSE)

#Here is the calculation for Fisher family error rate
ptukey(qt(.025,df,lower.tail=FALSE)*sqrt(2),a,df,lower.tail = FALSE)