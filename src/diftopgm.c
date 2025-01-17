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

/*
 * Convertit une image différentielle DIFF en image normalisée PGM
 * Reconstruit les pixels à partir du fichier .dif et les sauvegarde en PGM.
 */
int diftopgm(const char *diff_filename, const char *pgm_filename) {
    FILE *diff_file = fopen(diff_filename, "rb");
    if (!diff_file) {
        perror("Erreur lors de l'ouverture du fichier DIFF");
        return -1;
    }
    
    // Lecture de l'en-tête
    unsigned short magic, width, height;
    fread(&magic, sizeof(unsigned short), 1, diff_file);
    if (magic != MAGIC_NUMBER) {
        printf("Fichier DIFF non valide.\n");
        fclose(diff_file);
        return -1;
    }
    fread(&width, sizeof(unsigned short), 1, diff_file);
    fread(&height, sizeof(unsigned short), 1, diff_file);
    
    // Lecture des différences
    signed short *differences = malloc((width * height - 1) * sizeof(signed short));
    fread(differences, sizeof(signed short), width * height - 1, diff_file);
    fclose(diff_file);
    
    // Reconstruction de l'image
    unsigned char *pixels = malloc(width * height);
    pixels[0] = 128; // Valeur arbitraire pour la reconstruction
    for (int i = 1; i < width * height; i++) {
        pixels[i] = pixels[i - 1] + differences[i - 1];
    }
    free(differences);
    
    // Écriture du fichier PGM
    FILE *pgm_file = fopen(pgm_filename, "wb");
    fprintf(pgm_file, "P5\n%d %d\n255\n", width, height);
    fwrite(pixels, sizeof(unsigned char), width * height, pgm_file);
    fclose(pgm_file);
    free(pixels);
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
        fprintf(stderr, "\e[1musage\e[0m : %s <path_to_image.pgm>\n", argv[0]);
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