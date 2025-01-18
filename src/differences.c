#include <g2x.h>
#include <differences.h>
#include <string.h>

#define BUFFER_FACTOR 1.5 // Facteur pour la taille du buffer compressé

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

/**
 * Écrit un entier non signé de 2 octets (big-endian) dans un fichier
 */
void write_uint16(FILE *file, uint16_t value) {
    fputc((value >> 8) & 0xFF, file); // Octet de poids fort
    fputc(value & 0xFF, file);        // Octet de poids faible
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

    // --- ALLOCATION DU BUFFER ---
    int buffer_size = (int)(BUFFER_FACTOR * N); 
    uchar buffer[buffer_size]; // Allocation statique du buffer

    // --- ENCODAGE DANS LE BUFFER ---
    int bits_used = encode_differences(buffer, (int*)dif->map, N);

    // --- OUVERTURE DU FICHIER .dif ---
    FILE *file = fopen("./DIFF/truc.dif", "wb");
    if (!file) {
        fprintf(stderr, "Erreur d'ouverture du fichier.\n");
        return false;
    }

    // --- ÉCRITURE DANS LE FICHIER ---

    // --- EN-TÊTE ---
    // Magic Number
    write_uint16(file, 0xD1FF);

    // Taille de l'image (Largeur, Hauteur)
    write_uint16(file, (uint16_t)pix->width);
    write_uint16(file, (uint16_t)pix->height);

    // Quantificateur : 1 octet pour le nombre de niveaux, puis 4 octets pour les bits
    fputc(0x04, file); // Toujours 4 niveaux
    fputc(0x01, file);
    fputc(0x02, file);
    fputc(0x04, file);
    fputc(0x08, file);
    // --- FIN DE L'EN-TÊTE ---

    // Premier pixel de l'image (first)
    fputc(dif->first, file);

    // --- ÉCRITURE DES DONNÉES COMPRESSÉES ---
    fwrite(buffer, 1, (bits_used + 7) / 8, file);

    fclose(file);
    printf("Encodage terminé. %d bits écrits dans './DIFF/truc.dif'\n", bits_used);

    return true;
}

extern void push_bits(BitStream *curr, uchar src, size_t size)
{
    if (size <= curr->cap)                  /* si on peut tout écrire */
    {                                       /* ex. cur->ptr:[xxx-----] et src:[-----abc] */
        curr->cap -= size;                  /* cur->cap:5, size:3 -> src:[---abc--] */
        *(curr->ptr) |= (src << curr->cap); /* buf:[xxxabc--] */
        return;
    }
    /* sinon : pas assez de place ex buf:[xxxabc--] et src:[--defghi] */
    size -= curr->cap;             /* nbre de bits restant: 4 [fghi] */
    *(curr->ptr) |= (src >> size); /* on copie ce qu’on peut buf:[xxxabcde] */
    /* => là, il faut passer à l’octet suivant, en pleine capacité */
    curr->ptr++;          /* on passe a l’octet suivant */
    curr->cap = CHAR_BIT; /* capacité : totale */
    /* */
    if (size == 0)
        return; /* si il ne reste rien : c’est fini */
    /* */
    curr->cap -= size;                  /* réduction de ce qu’il reste : 4 */
    *(curr->ptr) |= (src << curr->cap); /* cur->ptr:[fghi----] */
    /* (cur-1)->ptr : [xxxabcde] >> cur->ptr : [fghi----] cur->cap : 4 bits */
}

extern void pull_bits(BitStream *curr, uchar *dst, size_t size)
{ /* ex. on veut extraire nbit=2 et rbit=5 */                                      /* ex. on veut extraire nbit=8 mais rbit=5 */
    /* [xxxab***] (x déjà lu, * pas encore lu) */                                  /* [xxxabcde][fgh-----] (x : déjà lu) */
    *dst = *(curr->ptr); /* dst:[xxxab***] rbit=5 */                               /* dst:[xxxabcde] rbit=5 */
    *dst <<= (CHAR_BIT - curr->cap); /* dst:[xxxab***] -> dst<<(8-5):[ab***---] */ /* dst:[xxxabcde] -> dst<<(8-5):[abcde---] */
    *dst >>= (CHAR_BIT - size); /* dst:[ab***---] -> dst>>(8-2):[------ab] */      /* dst:[abcde---] -> dst<<(8-8):[abcde---] */
    /* -> il reste [------ab] */                                                   /* -> il reste [abcde---] */
    if (size < curr->cap)                                                          /* si on a tout lu */
    {                                                                              /* on ajuste la capacité */
        curr->cap -= size;                                                         /* rbit : 5-2=3 */
        return;                                                                    /* et c’est fini */
    } /* ---------------------------------------- */                               /* ----------------------------------------------- */
    size -= curr->cap;                                                             /* sinon : il reste des bits à lire (ici 8-5=3) */
    (curr->ptr)++;                                                                 /* => il faut aller dans l’octet suivant */
    curr->cap = CHAR_BIT;                                                          /* capacité de lecture maximale buf:[fgh******] */
    if (size == 0)
        return;                          /* si nbit=0, fini, on sort */
    curr->cap -= size;                   /* sinon, il faut lire les nbit (ici 3) restant */
    *dst |= (*(curr->ptr)) >> curr->cap; /* (dst>>(8-3) : [-----fgh] et capacité rbit:8-3=5 */
} /* => dst:[abcde---]|[-----fgh]->[abcdefgh] */

extern int encode_differences(unsigned char *dest, int *src, int N) {
    BitStream stream = {dest, CHAR_BIT};
    // int total_bits = 0;

    for (int i = 0; i < N; i++) {
        int d = src[i];                        // Différence à encoder
        unsigned char sign = (d < 0) ? 1 : 0;  // Signe de la différence
        unsigned int abs_d = (d < 0) ? -d : d; // Valeur absolue de la différence

        if (abs_d < 2) { // Intervalle [0, 2[
            push_bits(&stream, 0b0, 1);   // Préfixe : 1 bit
            push_bits(&stream, abs_d, 1); // Valeur : 1 bit
            push_bits(&stream, sign, 1);  // Signe : 1 bit
            // total_bits += 3;             // Total : 3 bits
        } else if (abs_d < 6) { // Intervalle [2, 6[
            push_bits(&stream, 0b10, 2);      // Préfixe : 2 bits
            push_bits(&stream, abs_d - 2, 2); // Valeur : 2 bits
            push_bits(&stream, sign, 1);      // Signe : 1 bit
            // total_bits += 5;                  // Total : 5 bits
        } else if (abs_d < 22) { // Intervalle [6, 22[
            push_bits(&stream, 0b110, 3);     // Préfixe : 3 bits
            push_bits(&stream, abs_d - 6, 4); // Valeur : 4 bits
            push_bits(&stream, sign, 1);      // Signe : 1 bit
            // total_bits += 8;                  // Total : 8 bits
        } else { // Intervalle [22, 256[
            push_bits(&stream, 0b111, 3);      // Préfixe : 3 bits
            push_bits(&stream, abs_d - 22, 8); // Valeur : 8 bits
            push_bits(&stream, sign, 1);       // Signe : 1 bit
            // total_bits += 12;                  // Total : 12 bits
        }
    }

    return (int)(stream.ptr - dest) * CHAR_BIT + (CHAR_BIT - stream.cap); // return total_bits;
}

int decode_differences(uchar *dest, uchar *src, int P) {
    BitStream stream = {src, CHAR_BIT};
    int N = 0;

    for (int i = 0; i < P; i++) {
        uchar prefix = 0;
        char value, sign;
        for (int j = 0; j < 3; j++) {
            uchar bit;
            pull_bits(&stream, &bit, 1);
            prefix = (prefix << 1) | bit;
            i++;
            if (bit == 0) {
                break;
            }
        }
        // printf("Prefix: %d\n", prefix);

        if (prefix == 0b0) {
            pull_bits(&stream, &value, 1);
            pull_bits(&stream, &sign, 1);
            i += 2;
        } else {
            if (prefix == 0b10) {
                pull_bits(&stream, &value, 2);
                pull_bits(&stream, &sign, 1);
                value += 2;
                i += 3;
            } else {
                if (prefix == 0b110) {
                    pull_bits(&stream, &value, 4);
                    pull_bits(&stream, &sign, 1);
                    value += 6;
                    i += 5;
                } else {
                    pull_bits(&stream, &value, 8);
                    pull_bits(&stream, &sign, 1);
                    value += 22;
                    i += 9;
                }
            }
        }

        char decoded_value = (sign == 1) ? -value : value;
        dest[N] = decoded_value;
        N++;
    }

    return N;
}

void save_dif_file(const char *filename, G2Xpixmap *img, DiffImg *dif) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Erreur d'ouverture du fichier .dif");
        return;
    }

    int N = img->width * img->height;
    unsigned char *buffer = malloc(1.5 * N);
    int encoded_size = encode_differences(buffer, dif->map, N);

    unsigned short magic = 0xD1FF;
    fwrite(&magic, sizeof(unsigned short), 1, file);
    fwrite(&img->width, sizeof(unsigned short), 1, file);
    fwrite(&img->height, sizeof(unsigned short), 1, file);
    unsigned char quant[4] = {0x01, 0x02, 0x04, 0x08};
    fwrite(quant, sizeof(unsigned char), 4, file);
    fwrite(&dif->first, sizeof(unsigned char), 1, file);
    fwrite(buffer, sizeof(unsigned char), (encoded_size + 7) / 8, file);

    free(buffer);
    fclose(file);
    printf("✅ Image enregistrée sous : %s\n", filename);
}
