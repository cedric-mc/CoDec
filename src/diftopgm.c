#include <stdio.h>
#include <g2x.h>
#include <difimg.h>
#include <differences.h>
#include <histogram.h>
#include <encodage.h>

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

Encodage encodage;

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
    createImg(&histogramImg, orig);
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
    if (SWAP_IMG && SWAP_HISTOGRAM_IMG) { // Si on affiche l'image originale et l'histogramme de l'image originale
        g2x_PixmapShow(visu, true);
        display_histogram(&histogramDiff);
    } else if (!SWAP_IMG && SWAP_HISTOGRAM_DIFF) {
        g2x_PixmapShow(orig, true);
        display_histogram(&histogramImg);
    } else if (SWAP_IMG && !SWAP_HISTOGRAM_IMG) {
        g2x_PixmapShow(visu, true);
    } else if (!SWAP_IMG && !SWAP_HISTOGRAM_DIFF) {
        g2x_PixmapShow(orig, true);
    } else {
        g2x_PixmapShow(visu, true);
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
    // Uniquement le nom du fichier sans le chemin
    char *filename = strrchr(argv[1], '/');
    sprintf(dif_filename, "./PGM%s.pgm", filename);

    printf("Affichage de l'image %s\n", dif_filename);

    g2x_SetInitFunction(init); /*! fonction d'initialisation !*/
    g2x_SetCtrlFunction(ctrl); /*! fonction de controle      !*/
    g2x_SetEvtsFunction(evts); /*! fonction d'événements     !*/
    g2x_SetDrawFunction(draw); /*! fonction de dessin        !*/
    g2x_SetExitFunction(quit); /*! fonction de sortie        !*/

    return g2x_MainStart();
}
