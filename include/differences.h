#ifndef DIFFERENCES_H
#define DIFFERENCES_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <g2x.h>
#include <difimg.h>

typedef signed short dword; /* Type pour stocker les différences */

/* Encodage et sauvegarde */
int encode_differences(unsigned char *dest, dword *src, int N);
void save_dif_file(const char *filename, G2Xpixmap *img, DiffImg *dif);

#endif /* DIFFERENCES_H */
