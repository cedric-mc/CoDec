#ifndef ENCODAGE_H
#define ENCODAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <g2x.h>


typedef struct {
    size_t N; // Nombre de pixels
    size_t buffer_size; // Taille du buffer
    uchar *buffer; // Buffer compressé
    float compression_ratio; // Taux de compression
} Encodage;


void initEncodage(Encodage *encodage, DiffImg *dif);


#endif /* ENCODAGE_H */