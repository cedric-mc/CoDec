#include <stdio.h>
#include <string.h>
#include <limits.h> // Pour CHAR_BIT (valeur 8)
#include <g2x/g2x_types.h>

// Définition de la structure BitStream
typedef struct
{
    unsigned char *ptr; // Pointeur vers l'octet courant
    size_t cap;         // Nombre de bits disponibles dans l'octet courant
} BitStream;

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

int encode(unsigned char *dest, unsigned char *src, int N)
{
    BitStream stream = {dest, CHAR_BIT}; // Initialisation du flux binaire
    int total_bits = 0;                  // Nombre total de bits écrits

    for (int i = 0; i < N; i++)
    {
        unsigned char value = src[i];

        // Encodage selon les intervalles
        if (value < 4)
        {                                 // Intervalle [0, 4[
            push_bits(&stream, 0b0, 1);   // Préfixe : 1 bit
            push_bits(&stream, value, 2); // Valeur : 2 bits
            total_bits += 3;              // Total : 3 bits
        }
        else if (value < 20)
        {                                     // Intervalle [4, 20[
            push_bits(&stream, 0b10, 2);      // Préfixe : 2 bits
            push_bits(&stream, value - 4, 4); // Valeur : 4 bits
            total_bits += 6;                  // Total : 6 bits
        }
        else if (value < 52)
        {                                      // Intervalle [20, 52[
            push_bits(&stream, 0b110, 3);      // Préfixe : 3 bits
            push_bits(&stream, value - 20, 5); // Valeur : 5 bits
            total_bits += 8;                   // Total : 8 bits
        }
        else
        {                                      // Intervalle [52, 256[
            push_bits(&stream, 0b111, 3);      // Préfixe : 3 bits
            push_bits(&stream, value - 52, 8); // Valeur : 8 bits
            total_bits += 11;                  // Total : 11 bits
        }
    }

    return total_bits; // Retourne la taille en bits
}

int decode(unsigned char *dest, unsigned char *src, int P)
{
    BitStream stream = {src, CHAR_BIT}; // Initialisation du flux binaire
    int total_values = 0; // Nombre total de valeurs décodées

    for (int i = 0; i < P; i++)
    {
        unsigned char prefix, value;

        // Lecture du préfixe
        pull_bits(&stream, &prefix, 1); // Lire 1 bit
        if (prefix == 0b0) { // Préfixe : Intervalle [0, 4[
            pull_bits(&stream, &value, 2); // Lire 2 bits
            dest[total_values++] = value;
            continue;
        }

        pull_bits(&stream, &prefix, 1); // Lire le 2e bit
        if (prefix == 0b0) { // Préfixe : Intervalle [4, 20[
            pull_bits(&stream, &value, 4); // Lire 4 bits
            dest[total_values++] = value + 4;
            continue;
        }

        pull_bits(&stream, &prefix, 1); // Lire le 3e bit
        if (prefix == 0b0)
        {                                  // Préfixe : Intervalle [20, 52[
            pull_bits(&stream, &value, 5); // Lire 5 bits
            dest[total_values++] = value + 20;
        }
        else
        {                                  // Préfixe : Intervalle [52, 256[
            pull_bits(&stream, &value, 8); // Lire 8 bits
            dest[total_values++] = value + 52;
        }
    }

    return total_values; // Retourne le nombre de valeurs décodées
}

int main()
{
    // Données initiales
    unsigned char raw_data[] = {3, 51, 0, 18}; // Données brutes
    unsigned char compressed[16] = {0};        // Buffer pour les données compressées
    unsigned char decompressed[4] = {0};       // Buffer pour les données décompressées

    int raw_size = sizeof(raw_data); // Taille des données brutes

    // Encodage
    int compressed_size = encode(compressed, raw_data, raw_size); // Compression
    printf("Taille compressée (bits) : %d\n", compressed_size);

    // Décodage
    int decompressed_size = decode(decompressed, compressed, raw_size); // Décompression

    // Afficher les résultats
    printf("Données initiales : ");
    for (int i = 0; i < raw_size; i++)
        printf("%d ", raw_data[i]);
    printf("\n");

    printf("Données compressées (en hexadécimal) : ");
    for (int i = 0; i < (compressed_size + 7) / 8; i++)
        printf("%02X ", compressed[i]);
    printf("\n");

    printf("Données décompressées : ");
    for (int i = 0; i < decompressed_size; i++)
        printf("%d ", decompressed[i]);
    printf("\n");

    // Vérification
    if (memcmp(raw_data, decompressed, raw_size) == 0)
    {
        printf("Le décodage est correct !\n");
    }
    else
    {
        printf("Erreur dans le décodage.\n");
    }

    return 0;
}
