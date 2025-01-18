#include <stdio.h>
#include <g2x.h>
#include <differences.h>

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

static int hMaxDiff= 0; // Valeur maximale de l'histogramme
static int hMaxImg = 0; // Valeur maximale de l'histogramme
static int histogramDiff [256] = {0}; // Tableau de l'histogramme de l'image différentielle
static int histogramImg [256] = {0}; // Tableau de l'histogramme de l'image originale

static char *dif_filename[256]; // Stocke le nom du fichier .dif 📂

static void save_dif_file(const char *filename, G2Xpixmap *pix, DiffImg *dif) {
    int N = (pix->end - pix->map); // Nombre de différences à encoder

    // --- ALLOCATION DU BUFFER ---
    int buffer_size = (int)(BUFFER_FACTOR * N); 
    uchar buffer[buffer_size]; // Allocation statique du buffer

    // --- ENCODAGE DANS LE BUFFER ---
    int bits_used = encode_differences(buffer, (int*)dif->map, N);

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
    fwrite(buffer, 1, (bits_used + 7) / 8, file);

    fclose(file);
    printf("Encodage terminé. %d bits écrits dans '%s'\n", bits_used, filename);
}

static void createDiffImg(void) {
    if (!dif.map || !dif.end) {
        fprintf(stderr, "Erreur : données de l'image non valides\n");
        return;
    }

    if (dif.width <= 0 || dif.height <= 0) {
        fprintf(stderr, "Erreur : dimensions de l'image incorrectes [%dx%d]\n", dif.width, dif.height);
        return;
    }

    for (uchar *p = dif.map; p < dif.end; p++) {
        if (*p >= 0 && *p < 256) {
            histogramDiff[*p]++;
            if (hMaxDiff < histogramDiff[*p]) {
                hMaxDiff = histogramDiff[*p];
            }
        } else {
            fprintf(stderr, "Valeur pixel invalide : %f\n", *p);
        }
    }
}

static void createImg(void) {
    if (!img || !img->map || !img->end) {
        fprintf(stderr, "Erreur : données de l'image non valides\n");
        return;
    }

    if (img->width <= 0 || img->height <= 0) {
        fprintf(stderr, "Erreur : dimensions de l'image incorrectes [%fx%f]\n", img->width, img->height);
        return;
    }

    for (uchar *p = img->map; p < img->end; p++) {
        if (*p >= 0 && *p < 256) {
            histogramImg[*p]++;
            if (hMaxImg < histogramImg[*p]) {
                hMaxImg = histogramImg[*p];
            }
        } else {
            fprintf(stderr, "Valeur pixel invalide : %f\n", *p);
        }
    }
}

static void display_histogram(int hMax, int histogram[256]) {
    double x = g2x_GetXMin(); // Bord gauche de la fenêtre
    double y = g2x_GetYMin(); // Bord inférieur de la fenêtre
    double wtdh = (g2x_GetXMax() - g2x_GetXMin()) / 256; // Largeur de chaque barre

    double maxHeight = g2x_GetYMax() - g2x_GetYMin(); // Hauteur maximale dans l'espace G2X
    double coef = maxHeight / hMax;                   // Mise à l'échelle basée sur la hauteur disponible

    for (int elt = 0; elt < 256; elt++) {
        double barHeight = histogram[elt] * coef; // Hauteur proportionnelle dans l'espace G2X
        g2x_FillRectangle(x, y, x + wtdh, y + barHeight, G2Xr);
        x += wtdh;
    }
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

    createDiffImg();
    createImg();
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
    g2x_CreatePopUp("Histogram Show", self_histogram, "affiche l'histogramDiffgramme");
    g2x_CreatePopUp("Sauver .dif", compress, "Sauvegarder l'image compressée");
}

void evts(void)
{

}

/*! fonction de dessin        !*/
void draw(void) {
    if (SWAP_DIFF && SWAP_HISTOGRAM_DIFF) {
        g2x_PixmapShow(visu, true);
        display_histogram(hMaxDiff, histogramDiff);
    } else if (!SWAP_DIFF && SWAP_HISTOGRAM_IMG) {
        g2x_PixmapRecall(img, true);
        display_histogram(hMaxImg, histogramImg);
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
