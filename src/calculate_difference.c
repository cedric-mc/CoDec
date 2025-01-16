#include <difimg.h>

void print_binary(unsigned char byte) {
    for (int i = 7; i >= 0; i--) {
        printf("%d", (byte >> i) & 1);
    }
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
    if (size == 0) return; /* si il ne reste rien : c’est fini */
    /* */
    curr->cap -= size;                  /* réduction de ce qu’il reste : 4 */
    *(curr->ptr) |= (src << curr->cap); /* cur->ptr:[fghi----] */
    /* (cur-1)->ptr : [xxxabcde] >> cur->ptr : [fghi----] cur->cap : 4 bits */
}

// Fonction pull_bits
void pull_bits(BitStream *curr, unsigned char *dst, size_t size)
{
    *dst = *(curr->ptr);             // Copier l'octet courant dans dst
    *dst <<= (CHAR_BIT - curr->cap); // Décaler les bits pour aligner les bits utiles à gauche
    *dst >>= (CHAR_BIT - size);      // Décaler à droite pour ne garder que les bits nécessaires

    if (size < curr->cap)
    {                      // Cas sans débordement
        curr->cap -= size; // Mettre à jour la capacité restante
        return;
    }

    // Cas avec débordement
    size -= curr->cap;    // Calculer les bits restants à lire
    curr->ptr++;          // Avancer au prochain octet
    curr->cap = CHAR_BIT; // Réinitialiser la capacité

    if (size == 0)
        return; // Si aucun bit restant, on termine

    curr->cap -= size;                   // Réduire la capacité pour les bits restants
    *dst |= (*(curr->ptr)) >> curr->cap; // Lire les bits restants et les ajouter à dst
}

// Calcul des différences entre pixels
void calculate_differences(int *pixels, int *differences, int N) {
    differences[0] = pixels[0]; 
    for (int i = 1; i < N; i++) {
        differences[i] = pixels[i] - pixels[i - 1];
    }
}

// Encodage des différences (corrigé)
int encode_differences(unsigned char *dest, int *src, int N) {
    BitStream stream = {dest, CHAR_BIT}; 
    int total_bits = 0;

    push_bits(&stream, (unsigned char)src[0], CHAR_BIT);
    total_bits += CHAR_BIT;

    // Afficher le premier pixel encodé
    // printf("Premier pixel encodé : ");
    // print_binary(*(stream.ptr));
    // printf("\n");

    for (int i = 0; i < N; i++) {
        int d = src[i]; // Différence à encoder
        printf("Différence à encoder : %d\n", d);
        unsigned char sign = (d < 0) ? 1 : 0; // Signe de la différence
        unsigned int abs_d = (d < 0) ? -d : d; // Valeur absolue de la différence

        if (abs_d < 2) { // Intervalle [0, 2[
            printf("1 : %d\n", abs_d);
            push_bits(&stream, 0b0, 1); // Préfixe : 1 bit
            push_bits(&stream, abs_d, 1); // Valeur : 1 bit
            push_bits(&stream, sign, 1); // Signe : 1 bit
            total_bits += 3;
        } else if (abs_d < 6) { // Intervalle [2, 6[
            printf("2 : %d\n", abs_d);
            push_bits(&stream, 0b10, 2); // Préfixe : 2 bits
            push_bits(&stream, abs_d - 2, 2); // Valeur : 2 bits
            push_bits(&stream, sign, 1); // Signe : 1 bit
            total_bits += 5;
        } else if (abs_d < 22) { // Intervalle [6, 22[
            printf("3 : %d\n", abs_d);
            push_bits(&stream, 0b110, 3); // Préfixe : 3 bits
            push_bits(&stream, abs_d - 6, 4); // Valeur : 4 bits
            push_bits(&stream, sign, 1); // Signe : 1 bit
            total_bits += 8;
        } else { // Intervalle [22, 256[
            printf("4 : %d\n", abs_d);
            push_bits(&stream, 0b111, 3); // Préfixe : 3 bits
            push_bits(&stream, abs_d - 22, 8); // Valeur : 8 bits
            push_bits(&stream, sign, 1); // Signe : 1 bit
            total_bits += 12;
        }

        printf("Écriture dans buffer : ");
        print_binary(*(stream.ptr)); // Afficher l'octet actuel après l'écriture
        printf("\n");

        printf("Pointeur avancé : %p\n", (void *)stream.ptr); // Vérifier si le pointeur change
    }

    return total_bits;
}

void decode_differences(BitStream *stream, int *differences, int N) {
    for (int i = 1; i < N; i++) {
        unsigned char prefix, sign;
        int value = 0, offset;

        // Lecture du préfixe (1 bit au début)
        pull_bits(stream, &prefix, 1);

        if (prefix == 0b0) { // Intervalle [0, 2[
            pull_bits(stream, (unsigned char *)&value, 1); // Valeur : 1 bit
            offset = 0;
        } else {
            // Si le préfixe initial est `1`, lire un bit supplémentaire pour déterminer l'intervalle
            pull_bits(stream, &prefix, 1);
            if (prefix == 0b0) { // Intervalle [2, 6[
                pull_bits(stream, (unsigned char *)&value, 2); // Valeur : 2 bits
                offset = 2;
            } else {
                // Lire un bit supplémentaire pour distinguer les intervalles [6, 22[ et [22, 256[
                pull_bits(stream, &prefix, 1);
                if (prefix == 0b0) { // Intervalle [6, 22[
                    pull_bits(stream, (unsigned char *)&value, 4); // Valeur : 4 bits
                    offset = 6;
                } else { // Intervalle [22, 256[
                    pull_bits(stream, &value, 8); // Valeur : 8 bits
                    offset = 22;
                }
            }
        }

        // Lecture du signe
        pull_bits(stream, &sign, 1);

        // Reconstruction de la valeur différentielle
        differences[i] = (sign == 1 ? -(value + offset) : (value + offset));
    }
}

void reconstruct_pixels(int *differences, unsigned char *pixels, int N) {
    pixels[0] = differences[0]; // Premier pixel
    for (int i = 1; i < N; i++) {
        pixels[i] = pixels[i - 1] + differences[i];
    }
}

int main() {
    int pixels[] = {-1, 0, 2, 21, -40}; 
    int N = sizeof(pixels) / sizeof(pixels[0]);

    int differences[N];
    calculate_differences(pixels, differences, N);
    
    printf("Différences calculées :\n");
    for (int i = 0; i < N; i++) {
        printf("%d ", differences[i]);
    }
    printf("\n");

    unsigned char buffer[16] = {0}; 

    encode_differences(buffer, pixels, N);

    printf("Buffer encodé (en hexadécimal) :\n");
    for (int i = 0; i < 5; i++) {
        printf("%02X ", buffer[i]);
    }
    printf("\n");

    printf("Buffer encodé (en binaire) :\n");
    for (int i = 0; i < 16; i++) {
        print_binary(buffer[i]);
        printf(" ");
    }
    printf("\n");

    // // Décodage
    // int decoded_differences[N];
    // stream.ptr = buffer;
    // stream.cap = CHAR_BIT;
    // decode_differences(&stream, decoded_differences, N);

    // // Pixels reconstruits
    // unsigned char reconstructed_pixels[N];
    // reconstruct_pixels(decoded_differences, reconstructed_pixels, N);

    // // Affichage des pixels reconstruits
    // printf("Pixels reconstruits :\n");
    // for (int i = 0; i < N; i++) {
    //     printf("%d ", reconstructed_pixels[i]);
    // }
    // printf("\n");

    return 0;
}
