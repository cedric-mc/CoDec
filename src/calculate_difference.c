#include <difimg.h>

void print_binary(unsigned char byte) {
    for (int i = 7; i >= 0; i--) {
        printf("%d", (byte >> i) & 1);
    }
}

// Fonction push_bits
void push_bits(BitStream *curr, unsigned char src, size_t size)
{
    while (size > 0)
    {
        if (size <= curr->cap)
        {                                                // Si tout tient dans l'octet courant
            *(curr->ptr) |= (src << (curr->cap - size)); // Écrire les bits
            curr->cap -= size;                           // Réduire la capacité
            break;
        }
        else
        {                                                // Débordement
            *(curr->ptr) |= (src >> (size - curr->cap)); // Remplir l'octet courant
            size -= curr->cap;                           // Bits restants à écrire
            curr->ptr++;                                 // Avancer au prochain octet
            curr->cap = CHAR_BIT;                        // Réinitialiser la capacité
            src &= (1 << size) - 1;                      // Conserver uniquement les bits restants
        }
    }
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

void calculate_differences(unsigned char *pixels, int *differences, int N) {
    differences[0] = pixels[0]; // Le premier pixel est directement copié
    printf("Différences :\n%d ", differences[0]);
    for (int i = 1; i < N; i++) {
        differences[i] = pixels[i] - pixels[i - 1];
        printf("%d ", differences[i]);
    }
    printf("\n");
}

void encode_differences(BitStream *stream, int *differences, int N) {
    printf("Encodage des différences :\n");
    for (int i = 1; i < N; i++) {
        unsigned char prefix;
        int offset, n_bits;

        int val = abs(differences[i]);
        unsigned char sign = (differences[i] < 0) ? 1 : 0;

        if (val < 2) { // Intervalle [0, 2[
            prefix = 0b0;
            offset = 0;
            n_bits = 1;
        } else if (val < 6) { // Intervalle [2, 6[
            prefix = 0b10;
            offset = 2;
            n_bits = 2;
        } else if (val < 22) { // Intervalle [6, 22[
            prefix = 0b110;
            offset = 6;
            n_bits = 4;
        } else { // Intervalle [22, 256[
            prefix = 0b111;
            offset = 22;
            n_bits = 8;
        }

        int value = val - offset;

        // Encodage du triplet
        push_bits(stream, prefix, (prefix == 0b0) ? 1 : (prefix == 0b10) ? 2 : 3);
        push_bits(stream, value, n_bits);
        push_bits(stream, sign, 1);

        printf("Différence : %d, Préfixe : %d, Valeur : %d, Signe : %d\n", differences[i], prefix, value, sign);
        // Affichage du résultat en binaire

    }
}

void decode_differences(BitStream *stream, int *differences, int N) {
    printf("Décodage des différences :\n");
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

        printf("Décodé : Préfixe : %d, Valeur : %d, Signe : %d, Différence : %d\n", prefix, value, sign, differences[i]);
    }
}

void reconstruct_pixels(int *differences, unsigned char *pixels, int N) {
    pixels[0] = differences[0]; // Premier pixel
    printf("Pixels reconstruits :\n%d ", pixels[0]);
    for (int i = 1; i < N; i++) {
        pixels[i] = pixels[i - 1] + differences[i];
        printf("%d ", pixels[i]);
    }
    printf("\n");
}

int main() {
    unsigned char pixels[] = {90, 91, 93, 94, 92}; // Pixels originaux
    int N = sizeof(pixels) / sizeof(pixels[0]);

    int differences[N];
    calculate_differences(pixels, differences, N);

    unsigned char buffer[16] = {0}; // Buffer compressé
    BitStream stream = {buffer, CHAR_BIT};

    // Encodage
    encode_differences(&stream, differences, N);

    // Affichage du buffer encodé en binaire
    printf("Buffer encodé (en binaire) :\n");
    for (int i = 0; i < 16; i++) {
        print_binary(buffer[i]);
        printf(" ");
    }
    printf("\n");

    // Décodage
    int decoded_differences[N];
    stream.ptr = buffer;
    stream.cap = CHAR_BIT;
    decode_differences(&stream, decoded_differences, N);

    // Affichage des différences décodées
    printf("Différences décodées :\n");
    for (int i = 1; i < N; i++) {
        printf("Différence %d : %d\n", i, decoded_differences[i]);
    }

    // Pixels reconstruits
    unsigned char reconstructed_pixels[N];
    reconstruct_pixels(decoded_differences, reconstructed_pixels, N);

    // Affichage des pixels reconstruits
    printf("Pixels reconstruits :\n");
    for (int i = 0; i < N; i++) {
        printf("%d ", reconstructed_pixels[i]);
    }
    printf("\n");

    return 0;
}
