#include <stdio.h>
#include <g2x.h>
#include <difimg.h>
#include <differences.h>
#include <histogram.h>

#define BUFFER_FACTOR 1.5 // Facteur pour la taille du buffer compressé

/* variables globales */
/* récupère et segmente le chemin vers l'image : pathname/rootname.extname */
static char *rootname = NULL;
static char *pathname = NULL;
static char *extname = NULL;
G2Xpixmap *img = NULL;
static G2Xpixmap *visu = NULL, *orig = NULL;
DiffImg dif = {0};

/* paramètres d'interaction */
static bool SWAP_IMG = true; /* affichage : false->original  true->copie */
static bool SWAP_HISTOGRAM_DIFF = false; // Flag pour afficher l'histogramme de l'image différentielle
static bool SWAP_HISTOGRAM_IMG = false; // Flag pour afficher l'histogramme de l'image originale

Histogram histogramDiff;
Histogram histogramImg;

static char *dif_filename[256]; // Stocke le nom du fichier .dif 
// 📂

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
bool decode_dif(DiffImg *dif, const char *filename) {
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
    printf("N = %ld\n", N);
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

/**
 * Sauvegarde un fichier .pgm
 */
static void save_pgm_file(const char *filename, G2Xpixmap *pix) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Erreur : Impossible d'ouvrir %s\n", filename);
        return;
    }

    fprintf(file, "P5\n%d %d\n255\n", pix->width, pix->height);
    fwrite(pix->map, 1, pix->width * pix->height, file);
    fclose(file);
}

/*! fonction d'initialisation !*/
void init(void) {
    int w = dif.width, h = dif.height;
    
    g2x_PixmapAlloc(&visu, w, h, 1, 255); // Allocation de la pixmap pour l'image visuelle
    diftovisu(&dif, visu); // Conversion de l'image différentielle en image visuelle

    g2x_PixmapAlloc(&orig, w, h, 1, 255); // Allocation de la pixmap pour l'image originale
    diftopix(&dif, orig); // Conversion de l'image différentielle en image originale

    pixtodif(orig, &dif); // Conversion de l'image originale en image différentielle

    g2x_PixmapShow(visu, true);

    initHistogram(&histogramDiff);
    initHistogram(&histogramImg);
    createDiffImg(&histogramDiff, &dif);
    createImg(&histogramImg, &img);
}

/* passe la copie en négatif */
static void self_negate(void) {
    for (uchar *p = visu->map; p < visu->end; p++) *p = ~*p;
}

static void self_histogram(void) {
    // Selon si l'image est l'originale ou la différentielle, on affiche l'histogramme correspondant
    if (SWAP_IMG) {
        SWAP_HISTOGRAM_IMG = !SWAP_HISTOGRAM_IMG;
    } else {
        SWAP_HISTOGRAM_DIFF = !SWAP_HISTOGRAM_DIFF;
    }
}

static void compress(void) {
    save_pgm_file(dif_filename, orig);
}

/*! fonction de contrôle      !*/
void ctrl(void) {
    // selection de la fonte : ('n':normal,'l':large,'L':LARGE),('n':normal,'b':bold),('l':left, 'c':center, 'r':right)
    g2x_SetFontAttributes('l', 'b', 'c');
    g2x_CreatePopUp("NEG", self_negate, "négatif sur la copie");
    g2x_CreateSwitch("DIF/O", &SWAP_IMG, "affiche l'original ou la visuelle");
    g2x_CreatePopUp("Histogram Show", self_histogram, "affiche l'histograme");
    g2x_CreatePopUp("Sauver .dif.pgm", compress, "Sauvegarder l'image compressée");
}

void evts(void)
{

}

/*! fonction de dessin        !*/
void draw(void) {
    switch (SWAP_IMG) {
        case false:
            g2x_PixmapShow(orig, true);
            display_histogram(&histogramImg);
            break;
        case true:
            g2x_PixmapShow(visu, true);
            display_histogram(&histogramDiff);
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
        fprintf(stderr, "Image must be in DIF format.\n");
        return 1;
    }

    // Si l'extension du fichier est différente de .pgm, alors on affiche un message d'erreur
    if (strstr(argv[1], ".dif") == NULL) {
        fprintf(stderr, "Image must be in DIF format.\n");
        return 1;
    }

    /* cas particulier :
     * ouvre directement sur une image
     * -> le pixmap est allouée par la lib. (et libéré également).
     */
    if (!decode_dif(&dif, argv[1])) {
        fprintf(stderr, "\e[1m%s\e[0m : cannot read %s \n", argv[0], argv[1]);
        return 1;
    }

    /* création de la fenêtre -- ajustement pour les grandes images */
    int w=MIN(dif.width,1600);               
    int h=MIN(dif.height,800);
    g2x_InitWindow(argv[0],w,h);

    /* création de la zone d'affichage calibrée sur les dimensions de l'image */
    double xyratio=((double)dif.width)/((double)dif.height);
    g2x_SetWindowCoord(-1.,-xyratio,+1,+xyratio);
    
    // Stocke le nom du fichier .pgm (truc.dif -> ./PGM/truc.dif.pgm)
    sprintf(dif_filename, "./PGM/%s.pgm", argv[1]);

    printf("Affichage de l'image %s\n", dif_filename);

    g2x_SetInitFunction(init); /*! fonction d'initialisation !*/
    g2x_SetCtrlFunction(ctrl); /*! fonction de controle      !*/
    g2x_SetEvtsFunction(evts); /*! fonction d'événements     !*/
    g2x_SetDrawFunction(draw); /*! fonction de dessin        !*/
    g2x_SetExitFunction(quit); /*! fonction de sortie        !*/

    return g2x_MainStart();
}
