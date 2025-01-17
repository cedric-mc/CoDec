#include <stdio.h>
#include <g2x.h>
#include <difimg.h>

#define MAGIC_NUMBER 0xD1FF // Identifiant unique pour le format DIFF

/* variables globales */
/* récupère et segmente le chemin vers l'image : pathname/rootname.extname */
static char *rootname = NULL;
static char *pathname = NULL;
static char *extname = NULL;
G2Xpixmap *img = NULL;
static G2Xpixmap *visu = NULL, *orig = NULL;
DiffImg dif = {0};

/* paramètres d'interaction */
static bool SWAP = false; /* affichage : false->original  true->copie */

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

/*
 * Convertit une image normalisée PGM en image différentielle DIFF
 * Écrit un fichier .dif avec les différences entre pixels successifs.
 */
int pgmtodif(const char *pgm_filename, const char *diff_filename) {
    FILE *pgm_file = fopen(pgm_filename, "rb");
    if (!pgm_file) {
        perror("Erreur lors de l'ouverture du fichier PGM");
        return -1;
    }

    // 📌 Lecture de l'en-tête du fichier PGM
    char header[3];
    int width, height, maxval;
    fscanf(pgm_file, "%s\n%d %d\n%d\n", header, &width, &height, &maxval);

    if (strcmp(header, "P5") != 0 || maxval != 255) {
        printf("Format PGM non valide ou non supporté.\n");
        fclose(pgm_file);
        return -1;
    }

    // 📌 Lecture des pixels
    unsigned char *pixels = malloc(width * height);
    if (!pixels) {
        perror("Erreur d'allocation mémoire pour pixels");
        fclose(pgm_file);
        return -1;
    }
    fread(pixels, sizeof(unsigned char), width * height, pgm_file);
    fclose(pgm_file);

    // 📌 Calcul des différences différentielles
    int *differences = malloc((width * height) * sizeof(int));
    if (!differences) {
        perror("Erreur d'allocation mémoire pour differences");
        free(pixels);
        return -1;
    }
    differences[0] = pixels[0]; // Premier pixel stocké directement
    for (int i = 1; i < width * height; i++) {
        differences[i] = (int)pixels[i] - (int)pixels[i - 1];
    }
    free(pixels);

    // 📌 Ouverture du fichier DIFF
    FILE *diff_file = fopen(diff_filename, "wb");
    if (!diff_file) {
        perror("Erreur lors de l'ouverture du fichier DIFF");
        free(differences);
        return -1;
    }

    // 📌 Écriture de l'en-tête
    unsigned short magic = MAGIC_NUMBER;
    fwrite(&magic, sizeof(unsigned short), 1, diff_file);
    fwrite(&width, sizeof(unsigned short), 1, diff_file);
    fwrite(&height, sizeof(unsigned short), 1, diff_file);

    // 📌 Allocation du buffer compressé
    size_t buffer_size = 1.5 * width * height;
    unsigned char *compressed_buffer = malloc(buffer_size);
    if (!compressed_buffer) {
        perror("Erreur d'allocation mémoire pour buffer compressé");
        fclose(diff_file);
        free(differences);
        return -1;
    }

    // 📌 Encodage des différences
    int compressed_bits = encode_differences(compressed_buffer, differences, width * height);
    int compressed_bytes = (compressed_bits + 7) / 8; // Conversion en octets

    // 📌 Écriture des données compressées
    fwrite(compressed_buffer, sizeof(unsigned char), compressed_bytes, diff_file);

    // 📌 Nettoyage et fermeture des fichiers
    fclose(diff_file);
    free(differences);
    free(compressed_buffer);

    return 0;
}

/*! fonction d'initialisation !*/
void init(void)
{
    g2x_PixmapPreload(img); /* Préchargement de l'image */
    int w = img->width, h = img->height;
    difalloc(&dif, w, h); /* Allocation */
    pixtodif(img, &dif); /* Création */
    g2x_PixmapAlloc(&visu, w, h, 1, 255); /* Allocation */
    diftovisu(&dif, visu); /* Création */
    g2x_PixmapAlloc(&orig, w, h, 1, 255); /* Allocation */
    diftopix(&dif, orig); /* Création */
}

static void compress(void)
{
    char *diffname = malloc(strlen(rootname) + 5);
    sprintf(diffname, "DIFF/%s.dif", rootname);
    pgmtodif(pathname, diffname);
    free(diffname);
}

/*! fonction de contrôle      !*/
void ctrl(void)
{
    // selection de la fonte : ('n':normal,'l':large,'L':LARGE),('n':normal,'b':bold),('l':left, 'c':center, 'r':right)
    g2x_SetFontAttributes('l', 'b', 'c');
    g2x_CreateSwitch("O/DIF", &SWAP, "affiche l'original ou la visuelle");
    g2x_CreatePopUp("COMPRESSION", compress, "convertit en DIFF");
}

void evts(void)
{

}

/*! fonction de dessin        !*/
void draw(void)
{
    switch (SWAP)
    {
    case false:
        g2x_PixmapRecall(img, true); /* rappel de l'image originale */
        break;
    case true:
        g2x_PixmapShow(visu, true); /* affiche la copie de travail */
        break;
    }
}

/*! fonction de sortie        !*/
static void quit(void)
{
    /* rien à faire ici, l'image <img> est libérée automatiquement */
}

/*!***************************!*/
/*! fonction principale       !*/
/*!***************************!*/
int main(int argc, char *argv[]) {
    if (argc < 2)
    {
        fprintf(stderr, "\e[1musage\e[0m : %s <path_to_image>\n", argv[0]);
        fprintf(stderr, "Image must be in PGM format.\n");
        return 1;
    }

    // Si l'extension du fichier est différente de .pgm, alors on affiche un message d'erreur
    if (strstr(argv[1], ".pgm") == NULL)
    {
        fprintf(stderr, "Image must be in PGM format.\n");
        return 1;
    }

    /* cas particulier :
     * ouvre directement sur une image
     * -> le pixmap est allouée par la lib. (et libéré également).
     */
    if (!(img = g2x_InitImage(argv[1], &pathname, &rootname, &extname)))
    {
        fprintf(stderr, "\e[1m%s\e[0m : cannot read %s \n", argv[0], argv[1]);
        return 1;
    }

    g2x_SetInitFunction(init); /*! fonction d'initialisation !*/
    g2x_SetCtrlFunction(ctrl); /*! fonction de controle      !*/
    g2x_SetEvtsFunction(evts); /*! fonction d'événements     !*/
    g2x_SetDrawFunction(draw); /*! fonction de dessin        !*/
    g2x_SetExitFunction(quit); /*! fonction de sortie        !*/

    return g2x_MainStart();
}