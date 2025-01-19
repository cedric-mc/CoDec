#include <stdlib.h>
#include <g2x.h>
#include <difimg.h>
#include <differences.h>
#include <encodage.h>

#define BUFFER_FACTOR 12 // Facteur pour la taille du buffer compressé

extern void initEncodage(Encodage *encodage, DiffImg *dif) {
    size_t N = dif->width * dif->height;
    encodage->N = N;
    encodage->buffer_size = BUFFER_FACTOR * N;
    uchar *buffer = calloc(encodage->buffer_size, sizeof(uchar));

    float bits_used = encode_differences(buffer, dif->map, N);
    float data_size = N * 8;
    float compression_ratio = (bits_used / data_size) * 100;
    encodage->compression_ratio = compression_ratio;
    encodage->buffer = buffer;
}