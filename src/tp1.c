/*!=================================================================!*/
/*!= E.Incerti - eric.incerti@univ-eiffel.fr                       =!*/
/*!= Université Gustave Eiffel                                     =!*/
/*!= Code exemple pour prototypage avec libg2x.6e (2023)           =!*/
/*!=================================================================!*/

#include <g2x.h>

/* récupère et segmente le chemin vers l'image : pathname/rootname.extname */
static char *rootname = NULL;
static char *pathname = NULL;
static char *extname = NULL;
/* l'image elle-même : créée/libérée automatiquement par <g2x> */
static G2Xpixmap *img = NULL, *copie = NULL;

/* paramètres d'interaction */
static bool SWAP = true; /* affichage : false->original  true->copie */
static bool FILT = true; /* choix du filtre                          */
static int bitmask = 7;  /* nombre de plans de bits affichés         */
static uchar mask = 0;   /* masque binaire des plans de bits bitmask=5 => mask=11111000 */

static int H[255];

/*! fonction d'initialisation !*/
static void init(void)
{
    g2x_PixmapPreload(img);     /* pré-chargement de l'image d'entrée       */
    g2x_PixmapCpy(&copie, img); /* crée une copie de l'image d'entrée       */
}

/* passe la copie en négatif */
static void self_negate(void)
{
    for (uchar *p = copie->map; p < copie->end; p++)
        *p = ~*p;
}

/* applique le filtre 'mask' sur l'image (original->copie) */
static void bitfilter(bool F)
{
    uchar *p = img->map, *q = copie->map;
    switch (F)
    {
    case true:
        mask = (~((1 << (7 - bitmask)) - 1));
        for (; p < img->end; (p++, q++))
            *q = ((*p) & mask);
        break;
    case false:
        mask = (1 << bitmask);
        for (; p < img->end; (p++, q++))
            *q = ((*p) & mask) ? 255 : 0;
        break;
    }
}

/*! fonction de contrôle      !*/
static void ctrl(void)
{
    // selection de la fonte : ('n':normal,'l':large,'L':LARGE),('n':normal,'b':bold),('l':left, 'c':center, 'r':right)
    g2x_SetFontAttributes('l', 'b', 'c');
    g2x_CreatePopUp("NEG", self_negate, "négatif sur la copie");
    g2x_CreateSwitch("O/C", &SWAP, "affiche l'original ou la copie");
    g2x_CreateSwitch("F/B", &FILT, "filtre");
    g2x_CreateScrollv_i("b", &bitmask, 0, 7, "filtre les plans de bits");
}

static void evts(void)
{
    static int _bitmask_ = -1;
    static int _FILT_ = -1;
    if (_bitmask_ == bitmask && _FILT_ == FILT)
        return;
    _bitmask_ = bitmask;
    _FILT_ = FILT;
    bitfilter(FILT);
    if (!system("clear"))
        ;
    g2x_PixmapInfo(copie);
}

void showHistogramme(int *H, int N) {
    /* 1 extraire la valeur max */
    float max = 0;
    for (int i = 0; i < N; i++) {
        if (H[i] > max) {
            max = H[i];
        }
    }

    /* créer un tableau intermédiaire (réels) */
    float Hf[N];
    /* remplir Hf à partir de H et max -> Hf[i] ∈ [0, 1] */

    /* Calibrer */
    int wwidth = (g2x_GetXMax() - g2x_GetXMin());
    int wheight = (g2x_GetYMax() - g2x_GetYMin());

    /* tracer */
    double a = g2x_GetXMin();
    double b = g2x_GetYMin();
    int x, y;
    for (int i = 0; i < N; i++) {
        x = g2x_GetXMin() + i * wwidth / N;
        y = g2x_GetYMin() + Hf[i] * wheight;
        g2x_Line(a, b, x, y, G2Xr, 1);
        a = x;
        b = y;
    }
}

/*! fonction de dessin        !*/
static void draw(void)
{
    switch (SWAP)
    {
    case false:
        g2x_PixmapRecall(img, true); /* rappel de l'image originale */
        g2x_StaticPrint(g2x_GetPixWidth() / 2, 20, G2Xr, "ORIGINAL");
        break;
    case true:
        g2x_PixmapShow(copie, true); /* affiche la copie de travail */
        g2x_StaticPrint(g2x_GetPixWidth() / 2, 20, G2Xr, "COPIE AVEC HISTOGRAMME");
        g2x_StaticPrint(g2x_GetPixWidth() / 2, 40, G2Xr, "%0b", mask);
        showHistogramme(H, 255.0);
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
