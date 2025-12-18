
#We begin by calling the data for handout 5
library("readxl")
setwd("C:/Users/aneath/iCloudDrive/Lexar/stat581 fall2021")

h5.data = read_excel("handout5data.xlsx")
str(h5.data)

#We will also be performing multiple comparisons
library("multcomp")

#Example 5.1
#An engineer is designing a battery for use in a device that will be subjected to extreme variations in temperature. 
#An experiment is designed to investigate the effects of temperature and plate material on battery lifetime.
material = as.factor(na.omit(h5.data$plate))
temperature = as.factor(na.omit(h5.data$temp))
lifetime = na.omit(h5.data$life)

#Xtabs is used to tally the sample size for each treatment combination
xtabs(~ material+temperature)

#Use contrasts to define parameter restrictions for the two way ANOVA model
contrasts(material)=contr.sum
contrasts(temperature)=contr.sum

#We are still using aov to perform ANOVA computations.
#For two way ANOVA, we include two factors plus an interaction in the model statement
two.way.mod = aov(lifetime ~ material+temperature + material:temperature)
summary(two.way.mod)

#The command below is used to print the model parameter estimates
dummy.coef(two.way.mod)

#The command below is used to create an interaction plot
#The x.factor will be plotted on the horizontal axis. The trace.factor will be plotted as separate curves
interaction.plot(x.factor = temperature,trace.factor = material,response = lifetime)

#Because of the interaction effect, we will need to perform comparisons on treatment combinations
#Comparions on factor effects may be misleading since a factor effect depends on the level of the other factor

#The call combined is used to create a level for each treatment combination
combined = interaction(material,temperature)
levels(combined)

#Below is the code for computing Fisher comparisons between treatment combinations
#We are using glht and the accompanying commands in the same way we used them before
comb.mod = aov(lifetime~combined)
comparisons = glht(comb.mod, linfct = mcp( combined = "Tukey"))
summary(comparisons,test=univariate())
cld(summary(comparisons,test=univariate()))



#Example 5.2
#The yield of a chemical process is being studied.
#The two most important variables are thought to be pressure and temperature
P = as.factor(na.omit(h5.data$pressure))
T = as.factor(na.omit(h5.data$temperature))
yield = na.omit(h5.data$yield)

#Fit the interaction model. Note that P*T will also include the main effect terms.
x.mod = aov(yield ~ P*T)
summary(x.mod)

#Because the interaction effect does not seem important, we will fit an additive model
a.mod = aov(yield ~ P+T)
summary(a.mod)

#Below we define the estimated means under the additive model
a.means = predict(a.mod)

#Compare the interaction plots for the interaction model and the additive model
interaction.plot(T,P,yield)
interaction.plot(T,P,a.means)

#Below is code for computing Fisher comparisons separately for each factor
compare.P = glht(a.mod, linfct = mcp( P = "Tukey"))
compare.T = glht(a.mod, linfct = mcp( T = "Tukey"))
summary(compare.P,test=univariate())
cld(summary(compare.P,test=univariate()))
summary(compare.T,test=univariate())
cld(summary(compare.T,test=univariate()))

plot(cld(summary(compare.P,test=univariate())))
plot(cld(summary(compare.T,test=univariate())))

confint(compare.P,calpha = univariate_calpha())
confint(compare.T,calpha = univariate_calpha())

#Below is the code for computing Tukey comparisons for each factor 
#Note that Tukey comparisons are the default option
compare.P = glht(a.mod, linfct = mcp( P = "Tukey"))
compare.T = glht(a.mod, linfct = mcp( T = "Tukey"))
summary(compare.P)
cld(summary(compare.P))
summary(compare.T)
cld(summary(compare.T))

plot(cld(summary(compare.P)))
plot(cld(summary(compare.T)))

confint(compare.P)
confint(compare.T)



#Example 5.3
#An experiment is designed to investigate the effects of pouring temperature and titanium amount
#on the the breaking strength of titanium rods.

#define the variables
pour.temp = as.factor(na.omit(h5.data$pour.temp))
titanium = as.factor(na.omit(h5.data$titanium))
brk.strength = na.omit(h5.data$brk.strn)


#compute the two-way interaction ANOVA model 
model3 = aov(brk.strength ~ pour.temp * titanium)
summary(model3)

#create an interaction plot
interaction.plot(titanium, pour.temp, brk.strength)


