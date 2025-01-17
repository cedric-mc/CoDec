/*!=================================================================!*/
/*!= E.Incerti - eric.incerti@univ-eiffel.fr                       =!*/
/*!= Université Gustave Eiffel                                     =!*/
/*!= Code exemple pour prototypage avec libg2x.6e (2023)           =!*/
/*!=================================================================!*/

#include <g2x.h>

/* récupère et segmente le chemin vers l'image : pathname/rootname.extname */
static char      *rootname=NULL;
static char      *pathname=NULL;
static char      *extname =NULL;
/* l'image elle-même : créée/libérée automatiquement par <g2x> */
static G2Xpixmap *img=NULL,*copie=NULL;
static int hMax= 0;
static int histo [256]={0};

/*fonction de création de l'histogramme*/
static void create_histo(void){
  if (!img || !img->map || !img->end) {
    fprintf(stderr, "Erreur : données de l'image non valides\n");
    return;
  }

  if (img->width <= 0 || img->height <= 0) {
    fprintf(stderr, "Erreur : dimensions de l'image incorrectes [%fx%f]\n", img->width, img->height);
    return;
  }

  for (uchar* p = img->map; p < img->end; p++) {
    if (*p >= 0 && *p < 256) {
        histo[*p]++;
        if (hMax < histo[*p]) {
            hMax = histo[*p];
        }
    } else {
        fprintf(stderr, "Valeur pixel invalide : %f\n", *p);
    }
}
   
}
/*fonction d'affichage de l'histogramme*/
static void show_histo(void){
  double x = g2x_GetXMin();  // Bord gauche de la fenêtre
  double y = g2x_GetYMin();  // Bord inférieur de la fenêtre
  double wtdh = (g2x_GetXMax() - g2x_GetXMin()) / 256; // Largeur de chaque barre

  double maxHeight = g2x_GetYMax() - g2x_GetYMin(); // Hauteur maximale dans l'espace G2X
  double coef = maxHeight / hMax;  // Mise à l'échelle basée sur la hauteur disponible

  for (int elt = 0; elt < 256; elt++) {
    double barHeight = histo[elt] * coef; // Hauteur proportionnelle dans l'espace G2X
    g2x_FillRectangle(x, y, x + wtdh, y + barHeight, G2Xr);
    x += wtdh;
  }
}

/* paramètres d'interaction */
static bool  showHST=true; /* affichage : false->original  true->copie */

/*! fonction d'initialisation !*/
static void init(void)
{
  g2x_PixmapPreload(img);      /* pré-chargement de l'image d'entrée       */
  g2x_PixmapCpy(&copie,img);   /* crée une copie de l'image d'entrée       */
}


/*! fonction de contrôle      !*/
static void ctrl(void)
{
  // selection de la fonte : ('n':normal,'l':large,'L':LARGE),('n':normal,'b':bold),('l':left, 'c':center, 'r':right)
  g2x_CreateSwitch("afficher l'histogramme",&showHST,"affiche l'histogramme");
}

static void evts(void)
{}


/*! fonction de dessin        !*/
static void draw(void)
{
  if (showHST){
    g2x_PixmapRecall(img,true);
    create_histo();
    show_histo();
  }
  else{
    g2x_PixmapRecall(img,true);
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
int main(int argc, char* argv[])
{
  if (argc<2)
  {
    fprintf(stderr,"\e[1musage\e[0m : %s <path_to_image>\n",argv[0]);
    return 1;
  }
  /* cas particulier :
   * ouvre directement sur une image
   * -> le pixmap est allouée par la lib. (et libéré également).
   */
  if (!(img = g2x_InitImage(argv[1],&pathname,&rootname,&extname)))
  {
    fprintf(stderr,"\e[1m%s\e[0m : cannot read %s \n",argv[0],argv[1]);
    return 1;
  }
  g2x_SetInitFunction(init); /*! fonction d'initialisation !*/
  g2x_SetCtrlFunction(ctrl); /*! fonction de controle      !*/
  g2x_SetEvtsFunction(NULL); /*! fonction d'événements     !*/
  g2x_SetDrawFunction(draw); /*! fonction de dessin        !*/
  g2x_SetExitFunction(NULL); /*! fonction de sortie        !*/

  return g2x_MainStart();
}
