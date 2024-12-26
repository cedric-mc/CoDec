#ifndef _DIFIIMG_H
#define _DIFIIMG_H

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

/* fonctions publiques */
bool difalloc(DiffImg *dif, int width, int height);
bool diffree(DiffImg *dif);
bool diftopix(DiffImg *dif, G2Xpixmap *pix);
bool pixtodif(G2Xpixmap *pix, DiffImg *dif);
bool diftovisu(DiffImg *dif, G2Xpixmap *visu);

#endif