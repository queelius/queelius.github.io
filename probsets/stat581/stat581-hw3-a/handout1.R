#The data for this class will be provided as Excel files.
#Below is the call for the R package needed to read data from an Excel file. (use install.packages if needed)
library("readxl")

#Save the excel file in a convenient directory. 
#The following command identifies the location of the data, as well as sets the directory where output is written
#(It is easier to set the directory by clicking above on Session - Set Working Directory - Choose Directory)
setwd("C:/Users/aneath/iCloudDrive/Lexar/stat581 fall2021")

#Below is the command for reading in the data, naming the data file in R as h1.data
#The command str returns information on the structure of the data
h1.data = read_excel("handout1data.xlsx")
str(h1.data)

#Example 1.1 from class
#An experiment is conducted to compare formulations of a cement mortar (modified,unmodified)
#with respect to bond strength. The modified formulation adds a polymer latex emulsion during mixture. 
#Because the data sets within the excel file are of different sizes, we need na.omit to remove any unfilled entries
modified = na.omit(h1.data$modified)
unmod = na.omit(h1.data$unmod)

#Create a boxplot of the data as a first method of comparison
boxplot(modified,unmod,names = c("modified","unmodified"),xlab="cement mortar formulation",ylab="bond strength")

#Below is code for an R function to compute a two sample t test
#The data must be unstacked in that each variable contains response values from the respective treatment levels
two.sample.test = function(y1,y2,alpha=.05) 
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
  
  t.cr = qt(alpha/2,lower.tail = FALSE, df=n1+n2-2)
  t.0 = ybar.diff / SE
  p.value = 2*pt(abs(t.0),df=n1+n2-2,lower.tail = FALSE)
  
  table1 = matrix(c(ybar1,ybar2,s1,s2,s.p),nrow = 1)
  dimnames(table1) = list(c(""),c("ybar1","ybar2","s1","s2","Sp"))
  print(table1)
  
  table2 = matrix(c(t.0,t.cr,p.value),nrow = 1)
  dimnames(table2) = list(c(""),c("test statistic","critical point","p-value"))
  print(table2)
}

#Here we call the function using the data for the cement bond strength example
two.sample.test(modified,unmod)

#We could also use the built in R function for a two sample t test
t.test(modified,unmod,var.equal = TRUE)






#Example 1.2 from class
#Two machines are used for filling plastic bottles. A completely randomized design is performed
#by taking a random sample of fill heights from each machine

#The data is stacked in that an input variable corresponds to machine type and a response varible corresponds to heights 
machine = na.omit(h1.data$machine)
output = na.omit(h1.data$output)

#To call our R function, we need data given as heights from each machine

two.sample.test(output[machine==1],output[machine==2])

#Below are the calls for boxplot and t-test using the built-in R functions when the data is stacked
boxplot(output~machine,names = c("m 1","m 2"),xlab="machine",ylab="fill height")
t.test(output~machine,var.equal=TRUE)