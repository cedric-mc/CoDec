#ifndef HISTOGRAM_H
#define HISTOGRAM_H

#include <g2x.h>
#include <difimg.h>

// Structure pour stocker l'histogramme
typedef struct {
    int hMax; // Valeur maximale de l'histogramme
    int histogram[256]; // Tableau de l'histogramme
} Histogram;


/* fonctions publiques */
void initHistogram(Histogram *histogram);
void createDiffImg(Histogram *histogramDiff, DiffImg *dif);
void createImg(Histogram *histogramImg, G2Xpixmap *img);
void display_histogram(Histogram *histogramObject);

#endif /* HISTOGRAM_H */