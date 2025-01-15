/*!=================================================================!*/
/*!= E.Incerti - eric.incerti@univ-eiffel.fr                       =!*/
/*!= Université Gustave Eiffel                                     =!*/
/*!= Code exemple pour prototypage avec libg2x.6e (2023)           =!*/
/*!=================================================================!*/

#include <g2x.h>
#include <difimg.h>
#include <string.h>

#define MAGIC_NUMBER 0xD1FF

/* l'image elle-même : créée/libérée automatiquement par <g2x> */
static G2Xpixmap *img = NULL, *copie = NULL, *visu = NULL, *orig = NULL;

/* paramètres d'interaction */
static bool SWAP = true; /* affichage : false->original  true->copie */
static int bitmask = 7;  /* nombre de plans de bits affichés         */

/* type "pixel différenciel" */
typedef signed short dword; /* au moins 2 octets donc short */

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
        if (abs(*d) > max)
            max = abs(*d);
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

// Fonction pour encoder une image PGM en DIFF
int pgmtodif(const char *pgm_filename, const char *diff_filename)
{
    FILE *pgm_file = fopen(pgm_filename, "rb");
    if (!pgm_file)
    {
        perror("Erreur lors de l'ouverture du fichier PGM");
        return -1;
    }

    // Lire l'en-tête PGM
    char header[3];
    int width, height, maxval;
    fscanf(pgm_file, "%s\n%d %d\n%d\n", header, &width, &height, &maxval);

    if (strcmp(header, "P5") != 0 || maxval != 255)
    {
        printf("Format PGM non valide ou non supporté.\n");
        fclose(pgm_file);
        return -1;
    }

    // Allouer la mémoire pour les pixels
    unsigned char *pixels = malloc(width * height);
    fread(pixels, sizeof(unsigned char), width * height, pgm_file);
    fclose(pgm_file);

    // Calcul des différences
    ImgDif img = {width, height, pixels[0]};
    signed short *differences = malloc((width * height - 1) * sizeof(signed short));
    for (int i = 1; i < width * height; i++)
    {
        differences[i - 1] = pixels[i] - pixels[i - 1];
    }
    free(pixels);

    // Ouvrir le fichier DIFF en écriture
    FILE *diff_file = fopen(diff_filename, "wb");
    if (!diff_file)
    {
        perror("Erreur lors de l'ouverture du fichier DIFF");
        free(differences);
        return -1;
    }

    // Écriture de l'en-tête
    unsigned short magic = MAGIC_NUMBER;
    fwrite(&magic, sizeof(unsigned short), 1, diff_file);
    fwrite(&img.width, sizeof(unsigned short), 1, diff_file);
    fwrite(&img.height, sizeof(unsigned short), 1, diff_file);

    unsigned char quantifier[5] = {4, 1, 2, 4, 8};
    fwrite(quantifier, sizeof(unsigned char), 5, diff_file);
    fwrite(&img.first, sizeof(unsigned char), 1, diff_file);

    // Compression et écriture des différences
    unsigned char *compressed_buffer = malloc(1.5 * width * height);
    BitStream stream = {compressed_buffer, CHAR_BIT};
    for (int i = 0; i < width * height - 1; i++)
    {
        unsigned char prefix;
        int offset, n_bits;

        // Déterminer l'intervalle
        if (differences[i] < 2)
        { // Intervalle [0, 2[
            prefix = 0b0;
            offset = 0;
            n_bits = 1;
        }
        else if (differences[i] < 6)
        { // Intervalle [2, 6[
            prefix = 0b10;
            offset = 2;
            n_bits = 2;
        }
        else if (differences[i] < 22)
        { // Intervalle [6, 22[
            prefix = 0b110;
            offset = 6;
            n_bits = 4;
        }
        else
        { // Intervalle [22, 256[
            prefix = 0b111;
            offset = 22;
            n_bits = 8;
        }

        // Calculer la valeur décalée et le signe
        unsigned char sign = (differences[i] < 0) ? 1 : 0;
        int value = abs(differences[i]) - offset;

        // Encoder les triplets dans le flux binaire
        push_bits(&stream, prefix, strlen(prefix));
        push_bits(&stream, value, n_bits);
        push_bits(&stream, sign, 1);
    }

    int compressed_size = (stream.ptr - compressed_buffer) + (CHAR_BIT - stream.cap) / 8;
    fwrite(compressed_buffer, sizeof(unsigned char), compressed_size, diff_file);

    // Libération des ressources
    fclose(diff_file);
    free(differences);
    free(compressed_buffer);

    return 0;
}

// Fonction pour décoder un fichier DIFF en image PGM
int diftopgm(const char *diff_filename, const char *pgm_filename)
{
    FILE *diff_file = fopen(diff_filename, "rb");
    if (!diff_file)
    {
        perror("Erreur lors de l'ouverture du fichier DIFF");
        return -1;
    }

    // Lecture de l'en-tête
    unsigned short magic, width, height;
    fread(&magic, sizeof(unsigned short), 1, diff_file);
    if (magic != MAGIC_NUMBER)
    {
        printf("Fichier DIFF non valide.\n");
        fclose(diff_file);
        return -1;
    }

    fread(&width, sizeof(unsigned short), 1, diff_file);
    fread(&height, sizeof(unsigned short), 1, diff_file);

    unsigned char quantifier[5];
    fread(quantifier, sizeof(unsigned char), 5, diff_file);

    unsigned char first;
    fread(&first, sizeof(unsigned char), 1, diff_file);

    // Lecture du buffer compressé
    int buffer_size = 1.5 * width * height;
    unsigned char *compressed_buffer = malloc(buffer_size);
    fread(compressed_buffer, sizeof(unsigned char), buffer_size, diff_file);
    fclose(diff_file);

    // Décompression
    unsigned char *pixels = malloc(width * height);
    pixels[0] = first;

    BitStream stream = {compressed_buffer, CHAR_BIT};
    for (int i = 1; i < width * height; i++)
    {
        unsigned char prefix, sign;
        int offset, value;

        // Lire le préfixe pour déterminer l'intervalle
        pull_bits(&stream, &prefix, 1);
        if (prefix == 0b0)
        {
            pull_bits(&stream, &value, 1);
            offset = 0;
        }
        else
        {
            pull_bits(&stream, &prefix, 1);
            if (prefix == 0b0)
            {
                pull_bits(&stream, &value, 2);
                offset = 2;
            }
            else
            {
                pull_bits(&stream, &prefix, 1);
                if (prefix == 0b0)
                {
                    pull_bits(&stream, &value, 4);
                    offset = 6;
                }
                else
                {
                    pull_bits(&stream, &value, 8);
                    offset = 22;
                }
            }
        }

        // Lire le signe
        pull_bits(&stream, &sign, 1);

        // Reconstruire la valeur différentielle
        int d = (sign == 1 ? -value : value) + offset;

        // Reconstruire le pixel à partir du pixel précédent
        pixels[i] = pixels[i - 1] + d;
    }

    // Écriture du fichier PGM
    FILE *pgm_file = fopen(pgm_filename, "wb");
    fprintf(pgm_file, "P5\n%d %d\n255\n", width, height);
    fwrite(pixels, sizeof(unsigned char), width * height, pgm_file);
    fclose(pgm_file);

    free(pixels);
    free(compressed_buffer);

    return 0;
}
