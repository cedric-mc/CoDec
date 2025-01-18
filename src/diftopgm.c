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
static bool SWAP_DIFF = false; /* affichage : false->original  true->copie */
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
bool decode_dif(ImgDif *img, const char *filename) {
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

    img->width = read_uint16(file);
    img->height = read_uint16(file);

    uchar num_levels = fgetc(file); // Toujours 4 niveaux
    uchar quant_bits[4];
    for (int i = 0; i < 4; i++) {
        quant_bits[i] = fgetc(file);
    }

    img->first = fgetc(file); // Premier pixel

    printf("Magic Number: 0x%X\n", magic);
    printf("Taille: %d x %d\n", img->width, img->height);
    printf("Quantificateur: %d niveaux, Bits: {%d, %d, %d, %d}\n", num_levels, quant_bits[0], quant_bits[1], quant_bits[2], quant_bits[3]);
    printf("Premier pixel: %d\n", img->first);

    // --- CHARGEMENT DU BUFFER COMPRESSÉ ---
    int N = img->width * img->height;
    long data_size = file_size - 12;  // Taille réelle des données compressées
    printf("Taille réelle des données compressées : %ld octets\n", data_size);

    uchar *compressed_data = (uchar *)malloc(data_size);
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
    dword *decoded_data = (dword *)malloc(N * 1.5);
    if (!decoded_data) {
        fprintf(stderr, "Erreur d'allocation du buffer de décompression.\n");
        free(compressed_data);
        return false;
    }

    int num_decoded = decode_differences((uchar *)decoded_data, compressed_data, N - 1);

    if (num_decoded < N - 1) {
        fprintf(stderr, "Erreur de décompression: %d valeurs extraites au lieu de %d\n", num_decoded, N - 1);
        free(compressed_data);
        free(decoded_data);
        return false;
    }

    // Reconstruction de l'image
    decoded_data[0] = img->first;
    for (int i = 1; i < N; i++) {
        decoded_data[i] += decoded_data[i - 1];
    }

    free(compressed_data);
    free(decoded_data);

    return true;
}

/*! fonction d'initialisation !*/
void init(void) {
    g2x_PixmapPreload(img);
    int w = img->width, h = img->height;
    
    difalloc(&dif, w, h);
    pixtodif_encode(img, &dif);
    
    g2x_PixmapAlloc(&visu, w, h, 1, 255);
    diftovisu(&dif, visu);

    g2x_PixmapAlloc(&orig, w, h, 1, 255);
    diftopix(&dif, orig);

    createDiffImg(&histogramDiff, &dif);
    createImg(&histogramImg, img);
}

/* passe la copie en négatif */
static void self_negate(void) {
    for (uchar *p = visu->map; p < visu->end; p++) *p = ~*p;
}

static void self_histogram(void) {
    // Selon si l'image est l'originale ou la différentielle, on affiche l'histogramme correspondant
    if (SWAP_DIFF) {
        SWAP_HISTOGRAM_DIFF = !SWAP_HISTOGRAM_DIFF;
    } else {
        SWAP_HISTOGRAM_IMG = !SWAP_HISTOGRAM_IMG;
    }
}

/*! fonction de contrôle      !*/
void ctrl(void) {
    // selection de la fonte : ('n':normal,'l':large,'L':LARGE),('n':normal,'b':bold),('l':left, 'c':center, 'r':right)
    g2x_SetFontAttributes('l', 'b', 'c');
    g2x_CreatePopUp("NEG", self_negate, "négatif sur la copie");
    g2x_CreateSwitch("O/DIF", &SWAP_DIFF, "affiche l'original ou la visuelle");
    g2x_CreatePopUp("Histogram Show", self_histogram, "affiche l'histogramDiffgramme");
}

void evts(void)
{

}

/*! fonction de dessin        !*/
void draw(void) {
    if (SWAP_DIFF && SWAP_HISTOGRAM_DIFF) {
        g2x_PixmapShow(visu, true);
        display_histogram(&histogramDiff);
    } else if (!SWAP_DIFF && SWAP_HISTOGRAM_IMG) {
        g2x_PixmapRecall(img, true);
        display_histogram(&histogramImg);
    } else if (SWAP_DIFF && !SWAP_HISTOGRAM_DIFF) {
        g2x_PixmapShow(visu, true);
    } else if (!SWAP_DIFF && !SWAP_HISTOGRAM_IMG) {
        g2x_PixmapRecall(img, true);
    } else {
        g2x_PixmapRecall(img, true);
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
    sprintf(dif_filename, "./PGM/%s.dif.pgm", rootname);

    g2x_SetInitFunction(init); /*! fonction d'initialisation !*/
    g2x_SetCtrlFunction(ctrl); /*! fonction de controle      !*/
    g2x_SetEvtsFunction(evts); /*! fonction d'événements     !*/
    g2x_SetDrawFunction(draw); /*! fonction de dessin        !*/
    g2x_SetExitFunction(quit); /*! fonction de sortie        !*/

    return g2x_MainStart();
}
