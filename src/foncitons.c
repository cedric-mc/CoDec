#include <difimg.h>

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
            // Refaire un pull_bits pour connaître le signe de la valeur
            dest[total_values++] = value;
            continue;
        }

        pull_bits(&stream, &prefix, 1); // Lire le 2e bit
        if (prefix == 0b0) { // Préfixe : Intervalle [4, 20[
            pull_bits(&stream, &value, 4); // Lire 4 bits
            // Refaire un pull_bits pour connaître le signe de la valeur
            dest[total_values++] = value + 4;
            continue;
        }

        pull_bits(&stream, &prefix, 1); // Lire le 3e bit
        if (prefix == 0b0) { // Préfixe : Intervalle [20, 52[
            pull_bits(&stream, &value, 5); // Lire 5 bits
            // Refaire un pull_bits pour connaître le signe de la valeur
            dest[total_values++] = value + 20;
        } else { // Préfixe : Intervalle [52, 256[
            pull_bits(&stream, &value, 8); // Lire 8 bits
            // Refaire un pull_bits pour connaître le signe de la valeur
            dest[total_values++] = value + 52;
        }
    }

    return total_values; // Retourne le nombre de valeurs décodées
}