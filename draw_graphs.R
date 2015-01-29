draw_hits_vs_levels <- function() {
  hits = read.table("hits_vs_levels.dat")
  par(mfrow=c(2,4))
  plot(hits[,2], pch="+")
  plot(hits[,3], pch="+")
  plot(hits[,4], pch="+")
  plot(hits[,5], pch="+")
  plot(hits[,6], pch="+")
  plot(hits[,7], pch="+")
  plot(hits[,8], pch="+")
  plot(hits[,9], pch="+")
}

