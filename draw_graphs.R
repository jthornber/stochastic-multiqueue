draw_pdfs <- function() {
  pdfs = read.table("pdf.dat");
  pdf("pdfs.pdf")
  par(mfrow=c(1,2))
  plot(pdfs[,1], type="lines")
  plot(pdfs[,2], type="lines")
  dev.off()
}

draw_hits_vs_levels <- function() {
  hits = read.table("hits_vs_levels.dat")
  pdf("hits_vs_levels.pdf")  
  par(mfrow=c(2,4))
  plot(hits[,2], pch=".")
  plot(hits[,3], pch=".")
  plot(hits[,4], pch=".")
  plot(hits[,5], pch=".")
  plot(hits[,6], pch=".")
  plot(hits[,7], pch=".")
  plot(hits[,8], pch=".")
  plot(hits[,9], pch=".")
  dev.off()
}

draw_ha_with_changing_pdf_vs_adjustments <- function() {
  has = read.table("ha_with_changing_pdf_vs_adjustments.dat")
  pdf("ha_with_changing_pdf_vs_adjustments.pdf")
  par(mfrow=c(3,2))
  plot(has[,2], pch=".")
  plot(has[,3], pch=".")
  plot(has[,4], pch=".")
  plot(has[,5], pch=".")
  plot(has[,6], pch=".")
  plot(has[,7], pch=".")
  dev.off()
}

draw_ha_with_changing_pdf_vs_adjustments_hist <- function() {
  has = read.table("ha_with_changing_pdf_vs_adjustments.dat")
  pdf("ha_with_changing_pdf_vs_adjustments_hist.pdf")
  par(mfrow=c(3,2))
  plot(density(has[,2], breaks=100))
  plot(density(has[,3], breaks=100))
  plot(density(has[,4], breaks=100))
  plot(density(has[,5], breaks=100))
  plot(density(has[,6], breaks=100))
  plot(density(has[,7], breaks=100))
  dev.off()
}

draw_ha_with_changing_pdf_with_autotune <- function() {
  has = read.table("ha_with_changing_pdf_and_autotune.dat")
  pdf("ha_with_changing_pdf_with_autotune.pdf")
  par(mfrow=c(2,2))
  plot(has[,2], pch=".")
  plot(density(has[,2], breaks=100))
  plot(has[,3], pch=".")
  dev.off()
}

draw_pdfs()
draw_hits_vs_levels()
draw_ha_with_changing_pdf_vs_adjustments()
draw_ha_with_changing_pdf_vs_adjustments_hist()
draw_ha_with_changing_pdf_with_autotune()