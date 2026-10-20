
#We begin by calling the data for handout 6
library("readxl")
setwd("C:/Users/aneath/iCloudDrive/Lexar/stat581 fall2021")

h6.data = read_excel("handout6data.xlsx")
str(h6.data)


#Example 6.1
#A quality control department in a fabric finishing shop looks to investigate the effects of
#cycle time (factorA), temperature (factorB), and operator (factorC) on quality score.
#The experimental units are cloth specimens
time = as.factor(na.omit(h6.data$time))
temp = as.factor(na.omit(h6.data$temp))
operator = as.factor(na.omit(h6.data$operator))
score = na.omit(h6.data$score)

#fit a three factor ANOVA model using the * notation to add all interaction terms to the model
three.mod = aov(score ~ time*temp*operator)

#Note the ANOVA tests indicate evidence of a three factor interaction
summary(three.mod)

#Define the estimated means for the three factor ANOVA model
#Note that, as with all saturated models, the fitted values match the sample means
fits = predict(three.mod)

#Below is the code for creating an interaction plot between time and temperature for each level of operator
#Note how the time*temperature interaction depends on the level of operator
for (i in 1:nlevels(operator))
  interaction.plot(time[operator==levels(operator)[i]],temp[operator==levels(operator)[i]],
                   fits[operator==levels(operator)[i]],
                   xlab = "time",ylab = "score",trace.label = "temp")



#Example 6.2
#A 2^4 design is used to investigate the effects of time (A), concentration (B), pressure (C), temperature (D)
#on yield (y) of a chemical process
A = as.factor(na.omit(h6.data$tm))
B = as.factor(na.omit(h6.data$con))
C = as.factor(na.omit(h6.data$prs))
D = as.factor(na.omit(h6.data$tmp))
yield = na.omit(h6.data$yield)

#fit an ANOVA model with all main effects and two factor interactions using ( )^2 notation
two.mod = aov(yield ~ (A+B+C+D)^2)

#The ANOVA tests suggest a reduced model with factors A,C,D and interactions A:C,A:D
summary(two.mod)

reduced.mod = aov(yield ~ A+C+D + A:C + A:D)
summary(reduced.mod)

#Define the estimated means for the reduced model for plotting (randomness has been smoothed)
fits.r = predict(reduced.mod)

#Let's investigate the interaction effects using interaction plots
interaction.plot(A,C,fits.r)
interaction.plot(A,D,fits.r)

#Note how the reduced model fits show parallel for the interaction not included in the model
interaction.plot(C,D,fits.r)

#Let's investigate main effects using box plots
#The box plots show how a factor level mean depends on the other factor levels
plot(fits.r~A)
plot(fits.r~C)
plot(fits.r~D)


#We can plot all factor level means on a single plot using the following commands
A.C = interaction(A,C)
interaction.plot(D,A.C,fits.r)










