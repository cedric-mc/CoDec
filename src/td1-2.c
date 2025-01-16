#include <stdio.h>
#include <stdlib.h>
#include <limits.h> // Pour CHAR_BIT (valeur 8)

// Définition de la structure BitStream
typedef struct {
    unsigned char *ptr; // Pointeur vers l'octet courant
    size_t cap;         // Nombre de bits disponibles dans l'octet courant
} BitStream;

// Fonction pull_bits
void pull_bits(BitStream *curr, unsigned char *dst, size_t size) {
    *dst = *(curr->ptr); // Copier l'octet courant dans dst
    *dst <<= (CHAR_BIT - curr->cap); // Décaler les bits pour aligner les bits utiles à gauche
    *dst >>= (CHAR_BIT - size); // Décaler à droite pour ne garder que les bits nécessaires

    if (size < curr->cap) { // Cas sans débordement
        curr->cap -= size; // Mettre à jour la capacité restante
        return;
    }

    // Cas avec débordement
    size -= curr->cap; // Calculer les bits restants à lire
    curr->ptr++;       // Avancer au prochain octet
    curr->cap = CHAR_BIT; // Réinitialiser la capacité

    if (size == 0) return; // Si aucun bit restant, on termine

    curr->cap -= size; // Réduire la capacité pour les bits restants
    *dst |= (*(curr->ptr)) >> curr->cap; // Lire les bits restants et les ajouter à dst
}

int main() {
    // Exemple 1 : Lecture sans débordement
    unsigned char buffer1[] = {0b10110000}; // Buffer contenant l'octet
    BitStream stream1 = {buffer1, 5};       // 5 bits disponibles
    unsigned char dest1;

    printf("Exemple 1 : Lecture sans débordement\n");
    pull_bits(&stream1, &dest1, 2); // Lecture de 2 bits
    printf("Bits lus : %02X (attendu : 0b10)\n", dest1);

    // Exemple 2 : Lecture avec débordement
    unsigned char buffer2[] = {0b10111100, 0b11000000}; // Buffer contenant 2 octets
    BitStream stream2 = {buffer2, 3};                   // 3 bits disponibles dans le premier octet
    unsigned char dest2;

    printf("\nExemple 2 : Lecture avec débordement\n");
    pull_bits(&stream2, &dest2, 6); // Lecture de 6 bits
    printf("Bits lus : %02X (attendu : 0b101110)\n", dest2);

    return 0;
}
