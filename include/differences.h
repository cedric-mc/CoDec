#ifndef DIFFERENCES_H
#define DIFFERENCES_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <g2x.h>

/* types publics */
typedef signed short dword;
typedef struct
{
    int width, height;
    dword *map, *end;
    uchar first;
    dword difmax;
} DiffImg;

typedef struct 
{
    unsigned short width;
    unsigned short height;
    unsigned char first; // Premier pixel brut
} ImgDif;

// Définition de la structure BitStream
typedef struct
{
    unsigned char *ptr; // Pointeur vers l'octet courant
    size_t cap;         // Nombre de bits disponibles dans l'octet courant
} BitStream;

/* fonctions publiques */
bool difalloc(DiffImg *dif, int width, int height);
bool diffree(DiffImg *dif);
bool diftopix(DiffImg *dif, G2Xpixmap *pix);
bool pixtodif(G2Xpixmap *pix, DiffImg *dif);
bool diftovisu(DiffImg *dif, G2Xpixmap *visu);

bool pixtodif_encode(G2Xpixmap *pix, DiffImg *dif);
void push_bits(BitStream *curr, uchar src, size_t size);
void pull_bits(BitStream *curr, uchar *dst, size_t size);
int encode_differences(unsigned char *dest, int *src, int N);
int decode_differences(uchar *dest, uchar *src, int P);

#endif /* DIFFERENCES_H */
