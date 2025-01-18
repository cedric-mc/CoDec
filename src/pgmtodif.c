#include <stdio.h>
#include <g2x.h>
#include <difimg.h>
#include <differences.h> 

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
static bool SAVE_DIF = false; // Flag pour déclencher la sauvegarde de l'image compressée

static char *dif_filename[256]; // Stocke le nom du fichier .dif

void generate_dif_filename(const char *pgm_filename) {
    // Extraction du nom de fichier sans extension
    const char *basename = strrchr(pgm_filename, '/'); // Trouve le dernier '/'
    if (basename) {
        basename++; // Passe après le '/'
    } else {
        basename = pgm_filename; // Aucun '/' trouvé, utiliser directement le nom
    }

    // Copie du nom de base sans extension
    strncpy(dif_filename, "DIFF/", sizeof(dif_filename) - 1);
    strncat(dif_filename, basename, sizeof(dif_filename) - strlen(dif_filename) - 1);

    // Remplacement de ".pgm" par ".dif"
    char *ext = strrchr(dif_filename, '.');
    if (ext) {
        strcpy(ext, ".dif"); // Remplace l'extension
    } else {
        strncat(dif_filename, ".dif", sizeof(dif_filename) - strlen(dif_filename) - 1);
    }

    printf("📂 Fichier de sortie : %s\n", dif_filename);
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

// Fonction d'encodage et sauvegarde de l'image en format .dif
void save_dif_file(const char *filename, G2Xpixmap *img, DiffImg *dif) {
    FILE *file = fopen(filename, "wb");

    if (!file) {
        perror("Erreur d'ouverture du fichier .dif");
        return;
    }

    int N = img->width * img->height;
    unsigned char *buffer = malloc(1.5 * N);
    int encoded_size = encode_differences(buffer, dif->map, N);

    unsigned short magic = 0xD1FF;
    fwrite(&magic, sizeof(unsigned short), 1, file);
    fwrite(&img->width, sizeof(unsigned short), 1, file);
    fwrite(&img->height, sizeof(unsigned short), 1, file);
    unsigned char quant[4] = {0x01, 0x02, 0x04, 0x08};
    fwrite(quant, sizeof(unsigned char), 4, file);
    fwrite(&dif->first, sizeof(unsigned char), 1, file);
    fwrite(buffer, sizeof(unsigned char), (encoded_size + 7) / 8, file);

    free(buffer);
    fclose(file);
    printf("✅ Image enregistrée sous : %s\n", filename);
}

/*! fonction d'initialisation !*/
void init(void)
{
    g2x_PixmapPreload(img);
    int w = img->width, h = img->height;
    
    difalloc(&dif, w, h);
    pixtodif_encode(img, &dif);
    
    g2x_PixmapAlloc(&visu, w, h, 1, 255);
    diftovisu(&dif, visu);

    g2x_PixmapAlloc(&orig, w, h, 1, 255);
    diftopix(&dif, orig);
}

static void compress(void) {
    save_dif_file(dif_filename, img, &dif);
}

/*! fonction de contrôle      !*/
void ctrl(void) {
    // selection de la fonte : ('n':normal,'l':large,'L':LARGE),('n':normal,'b':bold),('l':left, 'c':center, 'r':right)
    g2x_SetFontAttributes('l', 'b', 'c');
    g2x_CreateSwitch("Afficher DIFF", &SWAP, "Basculer entre l'original et l'image différentielle");
    g2x_CreatePopUp("Sauver .dif", compress, "Sauvegarder l'image compressée");
}

void evts(void)
{

}

/*! fonction de dessin        !*/
void draw(void) {
    switch (SWAP) {
    case false:
        g2x_PixmapRecall(img, true); /* rappel de l'image originale */
        g2x_StaticPrint(10, 10, G2Xr, "Image originale");
        break;
    case true:
        g2x_PixmapShow(visu, true); /* affiche la copie de travail */
        g2x_StaticPrint(10, 10, G2Xr, "Image différentielle");
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
    if (argc < 2) {
        fprintf(stderr, "\e[1musage\e[0m : %s <path_to_image>\n", argv[0]);
        fprintf(stderr, "Image must be in PGM format.\n");
        return 1;
    }

    // Si l'extension du fichier est différente de .pgm, alors on affiche un message d'erreur
    if (strstr(argv[1], ".pgm") == NULL) {
        fprintf(stderr, "Image must be in PGM format.\n");
        return 1;
    }

    /* cas particulier :
     * ouvre directement sur une image
     * -> le pixmap est allouée par la lib. (et libéré également).
     */
    if (!(img = g2x_InitImage(argv[1], &pathname, &rootname, &extname))) {
        fprintf(stderr, "\e[1m%s\e[0m : cannot read %s \n", argv[0], argv[1]);
        return 1;
    }

    generate_dif_filename(argv[1]); // Génération du nom de fichier .dif

    g2x_SetInitFunction(init); /*! fonction d'initialisation !*/
    g2x_SetCtrlFunction(ctrl); /*! fonction de controle      !*/
    g2x_SetEvtsFunction(evts); /*! fonction d'événements     !*/
    g2x_SetDrawFunction(draw); /*! fonction de dessin        !*/
    g2x_SetExitFunction(quit); /*! fonction de sortie        !*/

    return g2x_MainStart();
}
