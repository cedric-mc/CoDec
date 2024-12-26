/*!=================================================================!*/
/*!= E.Incerti - eric.incerti@univ-eiffel.fr                       =!*/
/*!= Université Gustave Eiffel                                     =!*/
/*!= Code exemple pour prototypage avec libg2x.6e (2023)           =!*/
/*!=================================================================!*/

#include <g2x.h>
#include <difimg.h>

/* l'image elle-même : créée/libérée automatiquement par <g2x> */
static G2Xpixmap *img = NULL, *copie = NULL, *visu = NULL, *orig = NULL;

/* paramètres d'interaction */
static bool SWAP = true; /* affichage : false->original  true->copie */
static int bitmask = 7;  /* nombre de plans de bits affichés         */

/* type "pixel différenciel" */
typedef signed short dword; /* au moins 2 octets donc short */

/* type image différentielle */
typedef struct
{
    int width, height;
    dword *map, *end;
    uchar first;
    dword difmax; /* valeur absolue du max de l'image dif. -> se calcule dans pixtodif */
} DiffImg;

static DiffImg dif;

extern bool difalloc(DiffImg *dif, int width, int height)
{
    dif->map = (dword *)calloc(width * height, sizeof(dword));
    if (dif->map == NULL)
        return false;
    dif->end = dif->map + width * height;
    dif->width = width;
    dif->height = height;
    dif->first = 0; /* champ rempli plus tard (pixtodif) */
    return true;
}

extern bool diffree(DiffImg *dif)
{
    if (!dif || !dif->map)
        return false;
    free(dif->map);
    dif->map = dif->end = NULL;
    return true;
}

extern bool diftopix(DiffImg *dif, G2Xpixmap *orig)
{
    if (dif == NULL || orig == NULL)
        return false; /* simple sécurité */
    uchar *p = orig->map + 1;
    dword *d = dif->map + 1;
    *p = dif->first;
    while (d < dif->end)
    {
        *p = *(p - 1) + *d;
        p++;
        d++;
    }
    return true;
}

extern bool pixtodif(G2Xpixmap *pix, DiffImg *dif)
{
    if (pix == NULL || dif == NULL)
        return false;       /* simple sécurité */
    dif->first = *pix->map; /* 1er pixel, traité à part (non différentiel) */
    uchar *p = pix->map + 1;
    dword *d = dif->map + 1; /* positionnement des pointeurs */
    dword max = 0;
    while (p < pix->end)
    {
        *d = *p - *(p - 1);
        if (abs(*d) > max) max = abs(*d);
        p++;
        d++;
    }
    dif->difmax = max;
    return true;
}

extern bool diftovisu(DiffImg *dif, G2Xpixmap *visu)
{
    if (dif == NULL || visu == NULL)
        return false; /* simple sécurité */
    double calib = 1.0 / dif->difmax;
    uchar *p = visu->map + 1;
    dword *d = dif->map + 1;
    while (d < dif->end)
    {

        *p = (uchar)(255 * abs(*d) * calib); 
        p++;
        d++;
    }
    return true;
}
