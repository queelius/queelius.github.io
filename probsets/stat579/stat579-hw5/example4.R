#this program performs Bayesian inference for a binomial probability
#enter data: y is the number of successes, n is the number of trials
y=0
n=25

#define the beta distribution parameters
a=y+1
b=n-y+1

#create a grid of p for plotting. you may change to a smaller range than from 0 to 1 to see the distribution better
p = seq(from=0,to=.15,length.out=100)
#we are computing the beta density at each point in the grid
posterior = dbeta(p,a,b)

#plotting the beta density, higher values of the curve represent stronger data evidence
plot(p,posterior,type = "l")


#computing a confidence interval by taking the upper and lower percentiles of the beta distribution
#computing the median to represent the center of the distribution

lower = qbeta(.025,a,b)
median = qbeta(.5,a,b)
upper = qbeta(.975,a,b)
print(c(lower,median,upper))





#this program performs a Bayesian inference for vaccine efficacy
#enter the number of infections in the vaccine group
#and the number of infections in the control group
v = 8
c = 162

#define the beta distribution parameters
a = v+1
b = c+1

#simulate a very large number of draws from the posterior distribution on p=P(V|I)
#for each simulated p, compute the value for vaccine efficacy VE
p= rbeta(100000,a,b)
efficacy = 1 - p/(1-p)

#create a plot of the posterior distribution on VE
hist(efficacy,probability = TRUE)
points(density(efficacy),type = 'l')

#compute percentiles for a 95% interval estimate, a 50% interval estimate, and a point estimate
quantile(efficacy,c(.025,.25,.5,.75,.975))