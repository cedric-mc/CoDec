#include <difimg.h>

void print_binary(uchar *src, int N)
{
    for (int i = 0; i < N; i++)
    {
        for (int j = 7; j >= 0; j--)
        {
            printf("%d", (src[i] >> j) & 1);
        }
        printf(" ");
    }
    printf("\n");
}

// Fonction push_bits (corrigée)
// void push_bits(BitStream *curr, unsigned char src, size_t size) {
//     while (size > 0) {
//         if (size <= curr->cap) {
//             *(curr->ptr) |= (src & ((1 << size) - 1)) << (curr->cap - size);
//             curr->cap -= size;
//             break;
//         } else {
//             *(curr->ptr) |= (src >> (size - curr->cap));
//             size -= curr->cap;
//             curr->ptr++;
//             curr->cap = CHAR_BIT;
//             src &= (1 << size) - 1;
//         }
//     }
// }

static void push_bits(BitStream *curr, uchar src, size_t size)
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

// Fonction pull_bits
// void pull_bits(BitStream *curr, unsigned char *dst, size_t size)
// {
//     *dst = *(curr->ptr);             // Copier l'octet courant dans dst
//     *dst <<= (CHAR_BIT - curr->cap); // Décaler les bits pour aligner les bits utiles à gauche
//     *dst >>= (CHAR_BIT - size);      // Décaler à droite pour ne garder que les bits nécessaires

//     if (size < curr->cap)
//     {                      // Cas sans débordement
//         curr->cap -= size; // Mettre à jour la capacité restante
//         return;
//     }

//     // Cas avec débordement
//     size -= curr->cap;    // Calculer les bits restants à lire
//     curr->ptr++;          // Avancer au prochain octet
//     curr->cap = CHAR_BIT; // Réinitialiser la capacité

//     if (size == 0)
//         return; // Si aucun bit restant, on termine

//     curr->cap -= size;                   // Réduire la capacité pour les bits restants
//     *dst |= (*(curr->ptr)) >> curr->cap; // Lire les bits restants et les ajouter à dst
// }

static void pull_bits(BitStream *curr, uchar *dst, size_t size)
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

// Calcul des différences entre pixels
void calculate_differences(int *pixels, int *differences, int N)
{
    differences[0] = pixels[0];
    for (int i = 1; i < N; i++)
    {
        differences[i] = pixels[i] - pixels[i - 1];
    }
}

// Encodage des différences (corrigé)
int encode_differences(unsigned char *dest, int *src, int N)
{
    BitStream stream = {dest, CHAR_BIT};
    int total_bits = 0;

    for (int i = 0; i < N; i++)
    {
        int d = src[i];                        // Différence à encoder
        unsigned char sign = (d < 0) ? 1 : 0;  // Signe de la différence
        unsigned int abs_d = (d < 0) ? -d : d; // Valeur absolue de la différence

        if (abs_d < 2)
        {                                 // Intervalle [0, 2[
            push_bits(&stream, 0b0, 1);   // Préfixe : 1 bit
            push_bits(&stream, abs_d, 1); // Valeur : 1 bit
            push_bits(&stream, sign, 1);  // Signe : 1 bit
            // total_bits += 3;             // Total : 3 bits
        }
        else if (abs_d < 6)
        {                                     // Intervalle [2, 6[
            push_bits(&stream, 0b10, 2);      // Préfixe : 2 bits
            push_bits(&stream, abs_d - 2, 2); // Valeur : 2 bits
            push_bits(&stream, sign, 1);      // Signe : 1 bit
            // total_bits += 5;                  // Total : 5 bits
        }
        else if (abs_d < 22)
        {                                     // Intervalle [6, 22[
            push_bits(&stream, 0b110, 3);     // Préfixe : 3 bits
            push_bits(&stream, abs_d - 6, 4); // Valeur : 4 bits
            push_bits(&stream, sign, 1);      // Signe : 1 bit
            // total_bits += 8;                  // Total : 8 bits
        }
        else
        {                                      // Intervalle [22, 256[
            push_bits(&stream, 0b111, 3);      // Préfixe : 3 bits
            push_bits(&stream, abs_d - 22, 8); // Valeur : 8 bits
            push_bits(&stream, sign, 1);       // Signe : 1 bit
            // total_bits += 12;                  // Total : 12 bits
        }
    }

    return (int)(stream.ptr - dest) * CHAR_BIT + (CHAR_BIT - stream.cap); // return total_bits;
}

int decode_differences(uchar *dest, uchar *src, int P)
{
    BitStream stream = {src, CHAR_BIT};
    int N = 0;
    signed char first_pixel;

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
        printf("Prefix: %d\n", prefix);

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

void reconstruct_pixels(int *differences, int *pixels, int N)
{
    pixels[0] = differences[0];
    for (int i = 1; i < N; i++)
    {
        pixels[i] = pixels[i - 1] + differences[i];
    }
}

int main()
{
    int pixels[] = {-1, 0, 2, 21, -40};
    int N = sizeof(pixels) / sizeof(pixels[0]);

    uchar *dest = malloc(N * sizeof(uchar));
    int encode_size = encode_differences(dest, pixels, N);

    printf("Buffer encodé (en binaire) :\n");
    print_binary(dest, N - 1);

    // Décodage
    int decoded_differences[N];
    char *decoded_dest = malloc(N * sizeof(char));
    int decode_size = decode_differences(decoded_dest, dest, encode_size);

    // Pixels reconstruits
    unsigned char reconstructed_pixels[N];
    // reconstruct_pixels(decoded_differences, reconstructed_pixels, N);

    // Affichage des pixels reconstruits
    printf("Pixels reconstruits :\n");
    for (int i = 0; i < N; i++)
    {
        printf("%d ", decoded_dest[i]);
    }
    printf("\n");

    return 0;
}
