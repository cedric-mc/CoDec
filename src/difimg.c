/*!=================================================================!*/
/*!= E.Incerti - eric.incerti@univ-eiffel.fr                       =!*/
/*!= Université Gustave Eiffel                                     =!*/
/*!= Code exemple pour prototypage avec libg2x.6e (2023)           =!*/
/*!=================================================================!*/

#include <g2x.h>
#include <difimg.h>
#include <differences.h>
#include <string.h>

#define MAGIC_NUMBER 0xD1FF // Identifiant unique pour le format DIFF

/* l'image elle-même : créée/libérée automatiquement par <g2x> */
static G2Xpixmap *img = NULL, *copie = NULL, *visu = NULL, *orig = NULL;

/* type "pixel différenciel" */
typedef signed short dword; /* au moins 2 octets donc short */

static DiffImg dif; // Structure contenant les données différentielles

/*
 * Alloue la mémoire pour une image différentielle (DiffImg)
 * Retourne true si l'allocation est réussie, false sinon.
 */
extern bool difalloc(DiffImg *dif, int width, int height)
{
    // Allocation de la mémoire pour la carte des différences
    dif->map = (dword *)calloc(width * height, sizeof(dword));
    if (dif->map == NULL) return false; // Si l'allocation a échoué, alors on retourne false
    dif->end = dif->map + width * height; // Pointeur vers la fin de la carte des différences
    dif->width = width; // Largeur de l'image
    dif->height = height; // Hauteur de l'image
    dif->first = 0; /* champ rempli plus tard (pixtodif) */
    return true;
}

/*
 * Libère la mémoire allouée pour une image différentielle
 */
extern bool diffree(DiffImg *dif)
{
    if (!dif || !dif->map) return false; // Si la structure est NULL ou si la carte des différences est NULL, alors on retourne false
    free(dif->map); // Libération de la mémoire allouée pour la carte des différences
    dif->map = dif->end = NULL; // Réinitialisation des pointeurs
    return true;
}

/*
 * Convertit une image différentielle en une image normale
 * Utilise l'encodage différentiel stocké dans DiffImg pour reconstruire l'image originale.
 */
extern bool diftopix(DiffImg *dif, G2Xpixmap *orig)
{
    if (dif == NULL || orig == NULL) return false; /* simple sécurité */ // Si la structure DiffImg ou l'image originale est NULL, alors on retourne false
    uchar *p = orig->map + 1; // Pointeur vers le deuxième pixel de l'image originale
    dword *d = dif->map + 1; // Pointeur vers le deuxième pixel de la carte des différences
    *p = dif->first; // Premier pixel de l'image originale
    while (d < dif->end) { // Tant qu'on n'a pas atteint la fin de la carte des différences
        *p = *(p - 1) + *d; // Reconstruction du pixel à partir du pixel précédent et de la différence
        // Incrémentation des pointeurs vers les deuxième pixels de l'image originale et de la carte des différences
        p++;
        d++;
    }
    return true;
}

/*
 * Convertit une image normale en image différentielle
 * Stocke la première valeur brute puis calcule les différences entre pixels successifs.
 */
extern bool pixtodif(G2Xpixmap *pix, DiffImg *dif)
{
    if (pix == NULL || dif == NULL) return false; /* simple sécurité */
    dif->first = *pix->map; /* 1er pixel, traité à part (non différentiel) */
    uchar *p = pix->map + 1; // positionnement des pointeurs pour les pixels
    dword *d = dif->map + 1; /* positionnement des pointeurs */
    dword max = 0;
    while (p < pix->end)
    {
        *d = *p - *(p - 1);
        if (abs(*d) > max)
            max = abs(*d);
        p++;
        d++;
    }
    dif->difmax = max;
    return true;
}

/*
 * Génère une image de visualisation des différences en échelle de gris
 * Chaque pixel représente l'amplitude de la différence relative au max trouvé.
 */
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

/*
 * Convertit une image normale en image différentielle
 * Stocke la première valeur brute puis calcule les différences entre pixels successifs.
 */
bool pixtodif_encode(G2Xpixmap *pix, DiffImg *dif)
{
    if (pix == NULL || dif == NULL) return false; /* simple sécurité */
    dif->first = *pix->map; /* 1er pixel, traité à part (non différentiel) */
    uchar *p = pix->map + 1; // positionnement des pointeurs pour les pixels
    dword *d = dif->map + 1; /* positionnement des pointeurs */
    dword max = 0;

    int N = (pix->end - pix->map); // Nombre de différences à encoder

    // printf("Premier pixel : %d\n", dif->first);
    while (p < pix->end)
    {
        *d = *p - *(p - 1);
        if (abs(*d) > max) max = abs(*d);

        // Affichage de la valeur différentielle
        // printf("Diff[%ld] = %d\n", d - dif->map, *d);

        p++;
        d++;
    }
    dif->difmax = max;

    // --- ENCODAGE DES DIFFÉRENCES ---
    unsigned char *encoded_data = (unsigned char *)malloc(N * sizeof(unsigned char) * 2); // Allocation mémoire pour le résultat
    if (!encoded_data) {
        fprintf(stderr, "Erreur d'allocation mémoire pour l'encodage.\n");
        return false;
    }

    int bits_used = encode_differences(encoded_data, (int*)dif->map, N); // Encodage

    printf("Encodage terminé. %d bits utilisés.\n", bits_used);

    // (Optionnel) Écriture des données encodées dans un fichier
    FILE *file = fopen("encoded_diff.bin", "wb");
    if (file) {
        fwrite(encoded_data, 1, (bits_used + 7) / 8, file);
        fclose(file);
        printf("Données encodées enregistrées dans 'encoded_diff.bin'.\n");
    } else {
        fprintf(stderr, "Erreur lors de l'écriture du fichier.\n");
    }

    free(encoded_data); // Libération de la mémoire allouée pour l'encodage

    return true;
}
