#include <g2x.h>
#include <difimg.h>
#include <differences.h>
#include <string.h>

/* l'image elle-même : créée/libérée automatiquement par <g2x> */
static G2Xpixmap *img = NULL, *copie = NULL, *visu = NULL, *orig = NULL;

/**
 * Écrit un entier non signé de 2 octets (big-endian) dans un fichier
 */
void write_uint16(FILE *file, uint16_t value) {
    fputc((value >> 8) & 0xFF, file); // Octet de poids fort
    fputc(value & 0xFF, file);        // Octet de poids faible
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

extern int encode_differences(unsigned char *dest, dword *src, int N) {
    BitStream stream = {dest, CHAR_BIT};
    int total_bits = 0;

    for (int i = 0; i < N; i++) {
        dword d = src[i];                        // Différence à encoder
        char sign = (d < 0) ? 1 : 0;  // Signe de la différence
        dword abs_d = (d < 0) ? -d : d; // Valeur absolue de la différence

        if (abs_d < 2) { // Intervalle [0, 2[
            push_bits(&stream, 0b0, 1);   // Préfixe : 1 bit
            push_bits(&stream, abs_d, 1); // Valeur : 1 bit
            push_bits(&stream, sign, 1);  // Signe : 1 bit
            total_bits += 3;             // Total : 3 bits
        } else if (abs_d < 6) { // Intervalle [2, 6[
            push_bits(&stream, 0b10, 2);      // Préfixe : 2 bits
            push_bits(&stream, abs_d - 2, 2); // Valeur : 2 bits
            push_bits(&stream, sign, 1);      // Signe : 1 bit
            total_bits += 5;                  // Total : 5 bits
        } else if (abs_d < 22) { // Intervalle [6, 22[
            push_bits(&stream, 0b110, 3);     // Préfixe : 3 bits
            push_bits(&stream, abs_d - 6, 4); // Valeur : 4 bits
            push_bits(&stream, sign, 1);      // Signe : 1 bit
            total_bits += 8;                  // Total : 8 bits
        } else { // Intervalle [22, 256[
            push_bits(&stream, 0b111, 3);      // Préfixe : 3 bits
            push_bits(&stream, abs_d - 22, 8); // Valeur : 8 bits
            push_bits(&stream, sign, 1);       // Signe : 1 bit
            total_bits += 12;                  // Total : 12 bits
        }
    }

    return total_bits;
}

int decode_differences(dword *dest, uchar *src, int P) {
    BitStream stream = {src, CHAR_BIT};
    int N = 0;

    for (int i = 0; i < P; i++) {
        uchar prefix = 0;
        dword value = 0;
        char sign = 0;
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

        // char decoded_value = (sign == 1) ? -value : value;
        value = (sign == 1) ? -value : value;
        dest[N++] = value;
    }

    return N;
}
