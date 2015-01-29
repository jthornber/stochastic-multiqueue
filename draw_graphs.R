draw_pdfs <- function() {
  pdfs = read.table("pdf.dat");
  par(mfrow=c(1,2))
  plot(pdfs[,1], type="lines")
  plot(pdfs[,2], type="lines")
}

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

draw_ha_with_changing_pdf_vs_adjustments <- function() {
  has = read.table("ha_with_changing_pdf_vs_adjustments.dat")
  par(mfrow=c(3,2))
  plot(has[,2], pch="+")
  plot(has[,3], pch="+")
  plot(has[,4], pch="+")
  plot(has[,5], pch="+")
  plot(has[,6], pch="+")
  plot(has[,7], pch="+")
}

draw_ha_with_changing_pdf_vs_adjustments_hist <- function() {
  has = read.table("ha_with_changing_pdf_vs_adjustments.dat")
  par(mfrow=c(3,2))
  plot(density(has[,2], breaks=100))
  plot(density(has[,3], breaks=100))
  plot(density(has[,4], breaks=100))
  plot(density(has[,5], breaks=100))
  plot(density(has[,6], breaks=100))
  plot(density(has[,7], breaks=100))
}
