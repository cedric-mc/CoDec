#include <stdlib.h>
#include <g2x.h>
#include <difimg.h>
#include <differences.h>
#include <encodage.h>

#define BUFFER_FACTOR 12 // Facteur pour la taille du buffer compressé

extern void initEncodage(Encodage *encodage, DiffImg *dif) {
    size_t N = dif->width * dif->height;
    encodage->N = N;
    encodage->buffer_size = BUFFER_FACTOR * N;
    uchar *buffer = calloc(encodage->buffer_size, sizeof(uchar));

    float bits_used = encode_differences(buffer, dif->map, N);
    float data_size = N * 8;
    float compression_ratio = (bits_used / data_size) * 100;
    encodage->compression_ratio = compression_ratio;
    encodage->buffer = buffer;
}

extern void save_dif_file(const char *filename, G2Xpixmap *pix, DiffImg *dif, Encodage *encodage) {
    size_t N = dif->width * dif->height;

    // --- OUVERTURE DU FICHIER .dif ---
    FILE *file = fopen(filename, "wb");
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
    fwrite(encodage->buffer, sizeof(uchar), encodage->buffer_size, file);

    fclose(file);
    printf("Encodage terminé. Écriture dans '%s'\n", filename);
}
