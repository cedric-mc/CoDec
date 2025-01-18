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
