.libPaths(c("~/R/library", .libPaths()))
library(ggplot2)
library(tidyr)
library(dplyr)

datos <- data.frame(
  benchmark = c("vadd.vv","vsub.vv","vand.vv","vor.vv","vxor.vv",
                "vsll.vv","vsrl.vv","AXPY","AXPY-ext",
                "Dot product","FIR","vse32"),
  categoria = c(rep("Memory-bound",7),rep("Mixed",2),
                rep("Compute",2),"Bandwidth"),
  N   = c(128,128,128,128,128,128,128,128,64,32,32,256),
  cpe_esc = c(36,36,44,44,44,41,41,63,144,355,5917,18),
  cpe_vec = c(30,30,30,30,30,30,30,34, 65, 25,2223,18),
  mejora  = c(16,16,31,31,31,26,26,44, 54, 92, 62, 0)
)
datos$speedup     <- datos$cpe_esc / datos$cpe_vec
datos$log_speedup <- log(datos$speedup)
datos$categoria   <- factor(datos$categoria,
                    levels=c("Bandwidth","Memory-bound","Mixed","Compute"))

# Dataset largo para regresion
dl <- reshape(datos[,c("benchmark","categoria","cpe_esc","cpe_vec")],
  varying=list(c("cpe_esc","cpe_vec")), v.names="cpe",
  timevar="impl", times=c("Escalar","Vectorial"), direction="long")
dl$impl <- factor(dl$impl, levels=c("Escalar","Vectorial"))

cat("=== 1. ESTADISTICAS DESCRIPTIVAS ===\n")
for (cn in levels(datos$categoria)) {
  s <- datos[datos$categoria==cn,]
  cat(sprintf("%-15s n=%d  speedup media=%.2f  sd=%.3f  [%.2f, %.2f]\n",
      cn, nrow(s), mean(s$speedup), ifelse(nrow(s)>1,sd(s$speedup),NA),
      min(s$speedup), max(s$speedup)))
}

cat("\n=== 2. WELCH'S T-TEST PAREADO POR CATEGORIA ===\n")
cat(sprintf("%-20s %4s %9s %11s %4s\n","Categoria","n","t","p-valor","Sig"))
cat(strrep("-",52),"\n")
for (cn in levels(datos$categoria)) {
  s <- datos[datos$categoria==cn,]
  if (nrow(s)>=2) {
    tr <- t.test(s$cpe_esc, s$cpe_vec, paired=TRUE, alternative="greater")
    sig <- ifelse(tr$p.value<0.001,"***",ifelse(tr$p.value<0.01,"**",
           ifelse(tr$p.value<0.05,"*","ns")))
    cat(sprintf("%-20s %4d %9.4f %11.4e %4s\n",
        cn, nrow(s), as.numeric(tr$statistic), tr$p.value, sig))
  }
}

cat("\n=== 3. T-TEST GLOBAL CON log(cpe) [elimina dominio FIR] ===\n")
tg <- t.test(log(datos$cpe_esc), log(datos$cpe_vec),
             paired=TRUE, alternative="greater")
cat(sprintf("t = %.4f,  p = %.4e\n",
    as.numeric(tg$statistic), tg$p.value))
cat(sprintf("Conclusion: %s\n",
    ifelse(tg$p.value<0.05,
           "RECHAZO H0 — vectorial significativamente mas rapido ***",
           "No se rechaza H0")))

cat("\n=== 4. ANOVA: log(speedup) ~ categoria ===\n")
mod_aov <- aov(log_speedup ~ categoria, data=datos)
print(summary(mod_aov))
cat("\n--- Tukey HSD ---\n")
print(TukeyHSD(mod_aov))

cat("\n=== 5. REGRESION: log(cpe) ~ impl + categoria ===\n")
mod_log <- lm(log(cpe) ~ impl + categoria, data=dl)
print(summary(mod_log))

cat("\n=== 6. REGRESION: sqrt(cpe) ~ impl + categoria ===\n")
cat("(Transformacion sqrt como en paper del asesor)\n")
mod_sqrt <- lm(sqrt(cpe) ~ impl + categoria, data=dl)
print(summary(mod_sqrt))

cat("\n=== 7. VERIFICACION DE SUPUESTOS ===\n")
sw_log  <- shapiro.test(residuals(mod_log))
sw_sqrt <- shapiro.test(residuals(mod_sqrt))
cat(sprintf("Shapiro-Wilk log(cpe):  W=%.4f p=%.4f — %s\n",
    sw_log$statistic, sw_log$p.value,
    ifelse(sw_log$p.value>0.05,"Normal OK","No normal")))
cat(sprintf("Shapiro-Wilk sqrt(cpe): W=%.4f p=%.4f — %s\n",
    sw_sqrt$statistic, sw_sqrt$p.value,
    ifelse(sw_sqrt$p.value>0.05,"Normal OK","No normal")))

# Bartlett excluyendo Bandwidth (n=1)
datos_bt <- datos[datos$categoria!="Bandwidth",]
bt <- bartlett.test(log_speedup ~ categoria, data=datos_bt)
cat(sprintf("Bartlett (sin Bandwidth): K2=%.4f p=%.4f — %s\n",
    bt$statistic, bt$p.value,
    ifelse(bt$p.value>0.05,"Varianzas homogeneas OK","Heterogeneas")))

cat("\n=== 8. GRAFICOS ===\n")
g1 <- ggplot(datos,
       aes(x=reorder(benchmark,mejora), y=mejora, fill=categoria)) +
  geom_col(alpha=0.85) +
  geom_hline(yintercept=30, linetype="dashed", color="red", linewidth=0.8) +
  annotate("text",x=2,y=33,label="Hipotesis >= 30%",color="red",size=3.2) +
  coord_flip() +
  scale_fill_manual(values=c("Bandwidth"="#95A5A6","Memory-bound"="#3498DB",
                              "Mixed"="#F39C12","Compute"="#E74C3C")) +
  labs(title="Mejora vectorial por benchmark",
       subtitle="TFG RVV-lite sobre PicoRV32 — Hardware: Nexys A7-100T 100 MHz",
       x=NULL, y="Mejora (%)", fill="Categoria") +
  theme_minimal(base_size=11) + theme(legend.position="bottom")
ggsave("fig1_mejora.png", g1, width=9, height=7, dpi=150)

g2 <- ggplot(datos,
       aes(x=categoria, y=speedup, color=categoria)) +
  geom_jitter(width=0.12, size=4, alpha=0.9) +
  stat_summary(fun=mean, geom="crossbar", width=0.4, color="black", linewidth=0.5) +
  geom_hline(yintercept=1, linetype="dashed", color="gray50") +
  scale_y_log10(breaks=c(1,1.5,2,3,5,10,15)) +
  scale_color_manual(values=c("Bandwidth"="#95A5A6","Memory-bound"="#3498DB",
                               "Mixed"="#F39C12","Compute"="#E74C3C")) +
  labs(title="Speedup vectorial vs escalar por categoria",
       subtitle="Escala logaritmica. Barra = media de categoria.",
       x=NULL, y="Speedup = cpe_escalar / cpe_vectorial (log)") +
  theme_minimal(base_size=11) + theme(legend.position="none")
ggsave("fig2_speedup_categoria.png", g2, width=7, height=6, dpi=150)

png("fig3_qqplot_log.png",  width=600, height=500, res=100)
qqnorm(residuals(mod_log),  main="QQ-plot: residuos modelo log(cpe)")
qqline(residuals(mod_log),  col="red", lwd=2); dev.off()

png("fig4_qqplot_sqrt.png", width=600, height=500, res=100)
qqnorm(residuals(mod_sqrt), main="QQ-plot: residuos modelo sqrt(cpe)")
qqline(residuals(mod_sqrt), col="red", lwd=2); dev.off()

g5 <- ggplot(dl, aes(x=impl, y=log(cpe), fill=impl)) +
  geom_boxplot(alpha=0.7) +
  facet_wrap(~categoria, scales="free_y") +
  scale_fill_manual(values=c("Escalar"="#E74C3C","Vectorial"="#27AE60")) +
  labs(title="log(ciclos/elemento) por implementacion y categoria",
       x=NULL, y="log(cpe)", fill=NULL) +
  theme_minimal(base_size=11) + theme(legend.position="bottom")
ggsave("fig5_boxplot_impl_categoria.png", g5, width=10, height=7, dpi=150)

cat("Graficos guardados: fig1 fig2 fig3 fig4 fig5\n")
cat("\n=== ANALISIS FASE A COMPLETO ===\n")

cat("\n=== 9. WILCOXON SIGNED-RANK (no parametrico) ===\n")
cat("Alternativa valida cuando Shapiro-Wilk falla\n")
cat("No requiere supuesto de normalidad\n\n")

# Global
wt <- wilcox.test(datos$cpe_esc, datos$cpe_vec,
                  paired=TRUE, alternative="greater")
cat(sprintf("Wilcoxon global: V=%.0f, p=%.4e — %s\n",
    wt$statistic, wt$p.value,
    ifelse(wt$p.value<0.05,
           "RECHAZO H0 — vectorial significativamente mas rapido ***",
           "No se rechaza H0")))

# Por categoria
cat("\nPor categoria:\n")
cat(sprintf("%-20s %8s %11s %4s\n","Categoria","V","p-valor","Sig"))
cat(strrep("-",47),"\n")
for (cn in levels(datos$categoria)) {
  s <- datos[datos$categoria==cn,]
  if (nrow(s)>=2) {
    wc <- wilcox.test(s$cpe_esc, s$cpe_vec,
                      paired=TRUE, alternative="greater",
                      exact=FALSE)
    sig <- ifelse(wc$p.value<0.001,"***",
           ifelse(wc$p.value<0.01,"**",
           ifelse(wc$p.value<0.05,"*","ns")))
    cat(sprintf("%-20s %8.0f %11.4e %4s\n",
        cn, wc$statistic, wc$p.value, sig))
  }
}
cat("\n=== Fase A con Wilcoxon completa ===\n")
