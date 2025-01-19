#ifndef DIFFERENCES_H
#define DIFFERENCES_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <g2x.h>
#include <difimg.h>

// Définition de la structure BitStream
typedef struct
{
    unsigned char *ptr; // Pointeur vers l'octet courant
    size_t cap;         // Nombre de bits disponibles dans l'octet courant
} BitStream;


void push_bits(BitStream *curr, uchar src, size_t size);
void pull_bits(BitStream *curr, uchar *dst, size_t size);
int encode_differences(unsigned char *dest, dword *src, int N);
int decode_differences(dword *dest, uchar *src, int P);

#endif /* DIFFERENCES_H */
