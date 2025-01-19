#include <stdio.h>
#include <g2x.h>
#include <differences.h>
#include <difimg.h>
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
static bool SWAP_RATIO = false; // Flag pour afficher le taux de compression

Histogram histogramDiff;
Histogram histogramImg;

static char *dif_filename[256]; // Stocke le nom du fichier .dif 📂

static void save_dif_file(const char *filename, G2Xpixmap *pix, DiffImg *dif) {
    size_t N = dif->width * dif->height;

    // --- ALLOCATION DU BUFFER ---
    size_t buffer_size = 12 * N; // Taille du buffer
    uchar *buffer = calloc(buffer_size, sizeof(uchar));

    // --- ENCODAGE DANS LE BUFFER ---
    encode_differences(buffer, dif->map, N);

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
    fwrite(buffer, sizeof(uchar), buffer_size, file);

    // --- CALCUL DU TAUX DE COMPRESSION ---
    // compression_ratio = (float)buffer_size / (float)(pix->width * pix->height);
    

    fclose(file);
    printf("Encodage terminé. Écriture dans '%s'\n", filename);
}

/*! fonction d'initialisation !*/
void init(void) {
    g2x_PixmapPreload(img);
    int w = img->width, h = img->height;

    // Calcul du taux de compression
    float original_size = (float)(pix->width * pix->height);
    float compression_ratio = (compressed_size / original_size) * 100.0;
    
    difalloc(&dif, w, h);
    pixtodif(img, &dif);
    
    g2x_PixmapAlloc(&visu, w, h, 1, 255);
    diftovisu(&dif, visu);

    g2x_PixmapAlloc(&orig, w, h, 1, 255);
    diftopix(&dif, orig);

    createDiffImg(&histogramDiff, &dif);
    createImg(&histogramImg, img);
}

static void compress(void) {
    save_dif_file(dif_filename, img, &dif);
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
    g2x_CreatePopUp("Histogram Show", self_histogram, "affiche l'histogramme");
    g2x_CreatePopUp("Sauver .dif", compress, "Sauvegarder l'image compressée");
    g2x_CreateSwitch("Ratio", &SWAP_RATIO, "Affiche le taux de compression");
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

    switch (SWAP_RATIO) {
        case true:
            g2x_StaticPrint(g2x_GetPixWidth()/2,40,G2Xr,"Taux de compression : %f", compression_ratio);
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
    
    // Stocke le nom du fichier .dif
    sprintf(dif_filename, "./DIFF/%s.dif", rootname);

    g2x_SetInitFunction(init); /*! fonction d'initialisation !*/
    g2x_SetCtrlFunction(ctrl); /*! fonction de controle      !*/
    g2x_SetEvtsFunction(evts); /*! fonction d'événements     !*/
    g2x_SetDrawFunction(draw); /*! fonction de dessin        !*/
    g2x_SetExitFunction(quit); /*! fonction de sortie        !*/

    return g2x_MainStart();
}
