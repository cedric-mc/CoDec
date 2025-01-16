#include <stdio.h>
#include <stdlib.h>
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

int main()
{
    unsigned char buffer[4] = {0};         // Buffer pour les données compressées
    BitStream stream = {buffer, CHAR_BIT}; // Initialisation du flux binaire

    // Valeurs à encoder : 3, 51, 0, 18
    unsigned char values[] = {3, 51, 0, 18};

    // Encodage
    // Valeur 3 -> Préfixe 0b0 (1 bit), Valeur 0b11 (2 bits)
    push_bits(&stream, 0b0, 1);
    push_bits(&stream, 0b11, 2);

    // Valeur 51 -> Préfixe 0b110 (3 bits), Valeur (51 - 20) = 31 -> 0b11111 (5 bits)
    push_bits(&stream, 0b110, 3);
    push_bits(&stream, 0b11111, 5);

    // Valeur 0 -> Préfixe 0b0 (1 bit), Valeur 0b00 (2 bits)
    push_bits(&stream, 0b0, 1);
    push_bits(&stream, 0b00, 2);

    // Valeur 18 -> Préfixe 0b10 (2 bits), Valeur (18 - 4) = 14 -> 0b1110 (4 bits)
    push_bits(&stream, 0b10, 2);
    push_bits(&stream, 0b1110, 4);

    // Afficher le contenu du buffer
    printf("Buffer compressé (en hexadécimal) :\n");
    for (int i = 0; i < 4; i++)
    {
        printf("%02X ", buffer[i]);
    }
    printf("\n");

    return 0;
}
