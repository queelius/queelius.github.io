
###### Q3

## a ######
DJ=read.table(file="DJI_yearly.txt",header=T)

DJ=DJ[1:36,]
attach(DJ)

plot(DJIndex,type="l")
points(DJIndex)

## b ######
EMA <-function(tsdata, start, discount){
N=length(tsdata)
ema=vector(length=N)
ema[1]=start
 for (i in 2:N){
 	ema[i]=ema[i-1]*(1-discount)+tsdata[i]*discount
 }
 return(ema)	
}

EMA1=EMA(DJIndex,DJIndex[1],0.1)

plot(DJIndex,type="l")
points(DJIndex)
lines(EMA1,col="red")

## c ######
sse1=sum((DJIndex-EMA1)^2)		#sse=588756025

## d #####
library("forecast")
sEMA=ses(DJIndex, h = 1, level = c(95), initial = "simple", alpha = 0.1)
#forecast=13733.58, PI=(5046.53, 22420.63)

## e #####
t=1:length(DJIndex)
lm(DJIndex~t)	# beta0=-1620.7, beta1=527.4  

## f #####
s1=-1620.7-(1-0.1)/0.1*527.4		
s2=-1620.7-2*(1-0.1)/0.1*527.4		

## g #####
EMA2 <-function(tsdata, start1, start2, discount1,discount2){
ema1=EMA(tsdata, start1, discount1)
ema2=EMA(ema1, start2, discount2)
return(list(ema1=ema1,ema2=ema2,yhat=2*ema1-ema2))	
}

dEma=EMA2(DJIndex,s1,s2,0.1,0.1)
plot(DJIndex,type="l")
points(DJIndex)
lines(EMA1,col="red")
lines(dEma$yhat, col="blue")

## h #####
sse2=sum((DJIndex-dEma$yhat)^2)		#sse=117132166

## i #####
holt(DJIndex, h = 1, level = c(95), initial = "simple", alpha = 0.1, beta=0.1)
#forecast=18369.85, PI=(13558.89, 23180.81)

## j #####
#Improved



