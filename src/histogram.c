#include <g2x.h>
#include <histogram.h>
#include <difimg.h>


extern void initHistogram(Histogram *histogram) {
    histogram->hMax = 0;
    for (int i = 0; i < 256; i++) {
        histogram->histogram[i] = 0;
    }
}

extern void createDiffImg(Histogram *histogramDiff, DiffImg *dif) {
    if (!dif->map || !dif->end) {
        fprintf(stderr, "Erreur : données de l'image non valides, impossible de créer l'histogramme de l'image différentielle\n");
        return;
    }

    if (dif->width <= 0 || dif->height <= 0) {
        fprintf(stderr, "Erreur : dimensions de l'image incorrectes [%dx%d]\n", dif->width, dif->height);
        return;
    }

    for (uchar *p = dif->map; p < dif->end; p++) {
        if (*p >= 0 && *p < 256) {
            histogramDiff->histogram[*p]++;
            if (histogramDiff->hMax < histogramDiff->histogram[*p]) {
                histogramDiff->hMax = histogramDiff->histogram[*p];
            }
        } else {
            fprintf(stderr, "Valeur pixel invalide : %f\n", *p);
        }
    }
}

extern void createImg(Histogram *histogramImg, G2Xpixmap *img) {
    if (!img || !img->map || !img->end) {
        if (!img) {
            fprintf(stderr, "Erreur : img est NULL, impossible de créer l'histogramme de l'image\n");
        } else if (!img->map) {
            fprintf(stderr, "Erreur : img->map est NULL, impossible de créer l'histogramme de l'image\n");
        } else if (!img->end) {
            fprintf(stderr, "Erreur : img->end est NULL, impossible de créer l'histogramme de l'image\n");
        }
        fprintf(stderr, "Erreur : données de l'image non valides, impossible de créer l'histogramme de l'image\n");
        return;
    }

    if (img->width <= 0 || img->height <= 0) {
        fprintf(stderr, "Erreur : dimensions de l'image incorrectes [%fx%f]\n", img->width, img->height);
        return;
    }

    for (uchar *p = img->map; p < img->end; p++) {
        if (*p >= 0 && *p < 256) {
            histogramImg->histogram[*p]++;
            if (histogramImg->hMax < histogramImg->histogram[*p]) {
                histogramImg->hMax = histogramImg->histogram[*p];
            }
        } else {
            fprintf(stderr, "Valeur pixel invalide : %f\n", *p);
        }
    }
}

/*
* Affiche l'histogramme de l'image
*/
extern void display_histogram(Histogram *histogramObject) {
    double x = g2x_GetXMin(); // Bord gauche de la fenêtre
    double y = g2x_GetYMin(); // Bord inférieur de la fenêtre
    double wtdh = (g2x_GetXMax() - g2x_GetXMin()) / 256; // Largeur de chaque barre

    double maxHeight = g2x_GetYMax() - g2x_GetYMin(); // Hauteur maximale dans l'espace G2X
    double coef = maxHeight / histogramObject->hMax; // Mise à l'échelle basée sur la hauteur disponible

    for (int elt = 0; elt < 256; elt++) {
        double barHeight = histogramObject->histogram[elt] * coef; // Hauteur proportionnelle dans l'espace G2X
        g2x_FillRectangle(x, y, x + wtdh, y + barHeight, G2Xr);
        x += wtdh;
    }
}