# -----------------------------
# argmin and argmax (combined)
# -----------------------------

arg.min.max <- function(xs,f)
{
  y.max <- -Inf
  y.min <- Inf
  x.max <- NULL
  x.min <- NULL
  for (x in xs)
  {
    y <- f(x)
    if (y > y.max)
    {
      x.max <- x
      y.max <- y
    }
    else if (y < y.min)
    {
      x.min <- x
      y.min <- y
    }
  }
  list("x.max"=x.max,"y.max"=y.max,"x.min"=x.min,"y.min"=y.min)
}
