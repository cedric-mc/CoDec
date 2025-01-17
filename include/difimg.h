#ifndef _DIFIIMG_H
#define _DIFIIMG_H

#include <g2x.h>
#include <stdio.h>
#include <stdlib.h>

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
void create_histo(void);
void show_histo(void);
int pgmtodif(const char *pgm_filename, const char *diff_filename);
int diftopgm(const char *diff_filename, const char *pgm_filename);


#endif
