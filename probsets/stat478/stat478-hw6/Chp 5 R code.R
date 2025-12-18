###### Simulation MA(1)

mu=0
theta=-0.9
et=rnorm(200,0,1)
yt=mu+et[1:199]-theta*et[2:200]

###### Time plot
plot(yt,type="l")
points(yt)
###### ACF/PACF
acf(yt)
pacf(yt)
###### yt vs yt+1
plot(yt[1:198],yt[2:199])
###### yt vs yt+2
plot(yt[1:197],yt[3:199])


###### Simulation MA(2)
mu=0
theta1=-0.25
theta2=0.25
et=rnorm(500,0,1)
yt=mu+et[1:498]-theta1*et[2:499]-theta2*et[3:500]

###### Time plot
plot(yt,type="l")
points(yt)
###### ACF/PACF
acf(yt)
pacf(yt)
###### yt vs yt+1
plot(yt[1:198],yt[2:199])
###### yt vs yt+2
plot(yt[1:197],yt[3:199])

##### fit a MA model with data
library(TSA)


MA1=arima(deere3,c(0,0,1))


###### Simulation AR(1)
n=200
delta=0
yt=vector(length=n)
yt[1]=0
phi=0.9
for (i in 2:n){
	et=rnorm(1)
	yt[i]=delta+phi*yt[i-1]+et
}

plot(yt,type="l")
points(yt)
acf(yt)
pacf(yt)


###### Simulation AR(2)
n=200
delta=0
yt=vector(length=n)
yt[1]=0
yt[2]=0
phi=c(-0.5,0.25)

for (i in 3:n){
	et=rnorm(1)
	yt[i]=delta+phi[1]*yt[i-1]+phi[2]*yt[i-2]+et
}
plot(yt,type="l")
points(yt)
acf(yt)
pacf(yt)

#calculating theoratical ACF and PACF for ARMA process
ARMAacf(ar=c(-0.5,0.25),lag.max=10,pacf = T)


###### Simulation ARMA process
yt=arima.sim(n=500,list(ar=c(0.9),ma=c(-0.25)),sd=1)
plot(yt,type="l")
points(yt)
acf(yt)
pacf(yt)


                                                                                                                                                                                                                                                                                    
###### Simulation ARIMA process
yt=arima.sim(n=500,list(order = c(0,1,0)),sd=1)
plot(yt,type="l")
points(yt)
acf(yt)
d1y=diff(yt,differences = 1)
plot(d1y,type="l")


yt=arima.sim(n=500,list(order = c(1,1,1),ar=c(0.5),ma=c(-0.3)),sd=1)
plot(yt,type="l")
points(yt)
acf(yt)
d1y=diff(yt,differences = 1)
plot(d1y,type="l")


yt=arima.sim(n=500,list(order = c(0,1,1),ma=c(0.5),sd=1)
plot(yt,type="l")
points(yt)
acf(yt)
d1y=diff(yt,differences = 1)
plot(d1y,type="l")


yt=arima.sim(n=500,list(order = c(0,2,2),ma=c(0.3,-0.3)),sd=1)
plot(yt,type="l")
points(yt)
acf(yt)
d1y=diff(yt,differences = 1)
plot(d1y,type="l")
d2y=diff(yt,differences = 2)
plot(d2y,type="l")


#####Example 
data(AirPassengers)
plot.ts(AirPassengers)
Air.log=log(AirPassengers) #take log transformation to get equal variance
plot.ts(Air.log)
d1AirLog=diff(Air.log)
plot.ts(d1AirLog)



########simulate drift random walk
e=rnorm(1000)
yt=vector(length=1000)
yt[1]=e[1]
delta=0.2

for (t in 2:1000){
	yt[t]=delta+yt[t-1]+e[t]
	}

plot.ts(yt)





