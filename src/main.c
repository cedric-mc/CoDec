#include <g2x.h>
#include <difimg.h>

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

/*! fonction d'initialisation !*/
void init(void)
{
    g2x_PixmapPreload(img);
    int w = img->width, h = img->height;
    difalloc(&dif, w, h); /* Allocation */
    pixtodif(img, &dif); /* Création */
    g2x_PixmapAlloc(&visu, w, h, 1, 255); /* Allocation */
    diftovisu(&dif, visu); /* Création */
    g2x_PixmapAlloc(&orig, w, h, 1, 255); /* Allocation */
    diftopix(&dif, orig); /* Création */
}

/* passe la copie en négatif */
static void self_negate(void)
{
    for (uchar *p = visu->map; p < visu->end; p++) *p = ~*p;
}

/*! fonction de contrôle      !*/
void ctrl(void)
{
    // selection de la fonte : ('n':normal,'l':large,'L':LARGE),('n':normal,'b':bold),('l':left, 'c':center, 'r':right)
    g2x_SetFontAttributes('l', 'b', 'c');
    g2x_CreatePopUp("NEG", self_negate, "négatif sur la copie");
    g2x_CreateSwitch("O/DIF", &SWAP, "affiche l'original ou la visuelle");
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
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "\e[1musage\e[0m : %s <path_to_image>\n", argv[0]);
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
