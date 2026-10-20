av = anova(lm(y~A+B))
ms.A = av$`Mean Sq`[1]
ms.AB = av$`Mean Sq`[3]
ms.E = ms.AB
F.A = ms.A/ms.AB
df.A = av$Df[1]
df.AB = av$Df[3]

c("F.A"=F.A,"df.A"=df.A,"df.AB"=df.AB,"ms.A"=ms.A,"ms.AB"=ms.AB)