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

/**
 * Lit un entier non signé de 2 octets (big-endian) depuis un fichier
 */
uint16_t read_uint16(FILE *file) {
    uchar high = fgetc(file);
    uchar low = fgetc(file);
    return (high << 8) | low;
}

/**
 * Vérifie la taille d'un fichier
 */
long get_file_size(FILE *file) {
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    return size;
}

/**
 * Décode un fichier .dif et stocke les données dans un objet DiffImg
 */
extern bool decode_dif(DiffImg *dif, const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Erreur : Impossible d'ouvrir %s\n", filename);
        return false;
    }

    // Vérifier la taille du fichier avant allocation
    long file_size = get_file_size(file);
    if (file_size < 12) { // L'en-tête fait 11 octets minimum + 1 octet pour first
        fprintf(stderr, "Erreur : Fichier trop petit pour être valide !\n");
        fclose(file);
        return false;
    }

    // --- LECTURE DE L'EN-TÊTE ---
    uint16_t magic = read_uint16(file);
    if (magic != 0xD1FF) {
        fprintf(stderr, "Erreur : Magic Number incorrect (0x%X)\n", magic);
        fclose(file);
        return false;
    }

    int width = read_uint16(file);
    int height = read_uint16(file);

    uchar num_levels = fgetc(file); // Toujours 4 niveaux
    uchar quant_bits[4];
    for (int i = 0; i < 4; i++) {
        quant_bits[i] = fgetc(file);
    }

    uchar first = fgetc(file); // Premier pixel

    printf("Magic Number: 0x%X\n", magic);
    printf("Taille: %d x %d\n", width, height);
    printf("Quantificateur: %d niveaux, Bits: {%d, %d, %d, %d}\n", num_levels, quant_bits[0], quant_bits[1], quant_bits[2], quant_bits[3]);
    printf("Premier pixel: %d\n", first);

    // --- CHARGEMENT DU BUFFER COMPRESSÉ ---
    size_t N = width * height;
    size_t data_size = N * 12;  // Taille réelle des données compressées
    printf("Taille réelle des données compressées : %ld octets\n", data_size);

    uchar *compressed_data = calloc(data_size, sizeof(uchar)); // Allocation du buffer compressé
    if (!compressed_data) {
        fprintf(stderr, "Erreur d'allocation du buffer compressé.\n");
        fclose(file);
        return false;
    }

    size_t read_size = fread(compressed_data, 1, data_size, file);
    fclose(file);

    if (read_size < data_size) {
        fprintf(stderr, "Erreur : Lecture incomplète (%ld octets lus sur %ld attendus)\n", read_size, data_size);
        free(compressed_data);
        return false;
    }

    // --- DÉCOMPRESSION ---
    dword *decoded_data = calloc(N*12, sizeof(dword)); // Allocation du buffer de décompression
    if (!decoded_data) {
        fprintf(stderr, "Erreur d'allocation du buffer de décompression.\n");
        free(compressed_data);
        return false;
    }

    printf("Décompression des données...\n");
    int num_decoded = decode_differences(decoded_data, compressed_data, data_size);

    // Remplissage de la structure DiffImg
    difalloc(dif, width, height);
    dif->first = first;
    dword difmax = 0;
    for (int i = 0; i < N; i++) {
        dif->map[i] = decoded_data[i];
        if (difmax < abs(decoded_data[i])) difmax = abs(decoded_data[i]);
    }
    dif->difmax = difmax;

    free(compressed_data);
    free(decoded_data);

    return true;
}

extern void save_pgm_file(const char *filename, G2Xpixmap *pix) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Erreur : Impossible d'ouvrir %s\n", filename);
        return;
    }

    fprintf(file, "P5\n%d %d\n255\n", pix->width, pix->height);
    fwrite(pix->map, 1, pix->width * pix->height, file);
    fclose(file);
    printf("Image enregistrée dans '%s'\n", filename);
}
