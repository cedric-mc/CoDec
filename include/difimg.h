#ifndef _DIFIIMG_H
#define _DIFIIMG_H

#include <g2x.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

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


/* fonctions publiques */
bool difalloc(DiffImg *dif, int width, int height);
bool diffree(DiffImg *dif);
bool diftopix(DiffImg *dif, G2Xpixmap *pix);
bool pixtodif(G2Xpixmap *pix, DiffImg *dif);
bool diftovisu(DiffImg *dif, G2Xpixmap *visu);


#endif
