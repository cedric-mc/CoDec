/*!=================================================================!*/
/*!= E.Incerti - eric.incerti@univ-eiffel.fr                       =!*/
/*!= Université Gustave Eiffel                                     =!*/
/*!= Code exemple pour prototypage avec libg2x.6e (2023)           =!*/
/*!=================================================================!*/

#include <g2x.h>
#include <string.h>

#define MAGIC_NUMBER 0xD1FF // Identifiant unique pour le format DIFF

typedef signed short dword;
typedef struct
{
    int width, height;
    dword *map, *end;
    uchar first;
    dword difmax;
} DiffImg;

typedef struct 
{
    unsigned short width;
    unsigned short height;
    unsigned char first; // Premier pixel brut
} ImgDif;

typedef struct
{
    unsigned char *ptr; // Pointeur vers l'octet courant
    size_t cap;         // Nombre de bits disponibles dans l'octet courant
} BitStream;

static char *rootname = NULL;

/* l'image elle-même : créée/libérée automatiquement par <g2x> */
static G2Xpixmap *img = NULL, *copie = NULL, *visu = NULL, *orig = NULL;

static DiffImg dif; // Structure contenant les données différentielles

static char *dif_filename[256]; // Stocke le nom du fichier .dif 

void pull_bits(BitStream *curr, uchar *dst, size_t size)
{ /* ex. on veut extraire nbit=2 et rbit=5 */                                      /* ex. on veut extraire nbit=8 mais rbit=5 */
    /* [xxxab***] (x déjà lu, * pas encore lu) */                                  /* [xxxabcde][fgh-----] (x : déjà lu) */
    *dst = *(curr->ptr); /* dst:[xxxab***] rbit=5 */                               /* dst:[xxxabcde] rbit=5 */
    *dst <<= (CHAR_BIT - curr->cap); /* dst:[xxxab***] -> dst<<(8-5):[ab***---] */ /* dst:[xxxabcde] -> dst<<(8-5):[abcde---] */
    *dst >>= (CHAR_BIT - size); /* dst:[ab***---] -> dst>>(8-2):[------ab] */      /* dst:[abcde---] -> dst<<(8-8):[abcde---] */
    /* -> il reste [------ab] */                                                   /* -> il reste [abcde---] */
    if (size < curr->cap)                                                          /* si on a tout lu */
    {                                                                              /* on ajuste la capacité */
        curr->cap -= size;                                                         /* rbit : 5-2=3 */
        return;                                                                    /* et c’est fini */
    } /* ---------------------------------------- */                               /* ----------------------------------------------- */
    size -= curr->cap;                                                             /* sinon : il reste des bits à lire (ici 8-5=3) */
    (curr->ptr)++;                                                                 /* => il faut aller dans l’octet suivant */
    curr->cap = CHAR_BIT;                                                          /* capacité de lecture maximale buf:[fgh******] */
    if (size == 0)
        return;                          /* si nbit=0, fini, on sort */
    curr->cap -= size;                   /* sinon, il faut lire les nbit (ici 3) restant */
    *dst |= (*(curr->ptr)) >> curr->cap; /* (dst>>(8-3) : [-----fgh] et capacité rbit:8-3=5 */
} /* => dst:[abcde---]|[-----fgh]->[abcdefgh] */

int decode_differences(uchar *dest, uchar *src, int P) {
    BitStream stream = {src, CHAR_BIT};
    int N = 0;
    signed char first_pixel;

    for (int i = 0; i < P; i++) {
        uchar prefix = 0;
        char value, sign;
        for (int j = 0; j < 3; j++) {
            uchar bit;
            pull_bits(&stream, &bit, 1);
            prefix = (prefix << 1) | bit;
            i++;
            if (bit == 0) {
                break;
            }
        }
        // printf("Prefix: %d\n", prefix);

        if (prefix == 0b0) {
            pull_bits(&stream, &value, 1);
            pull_bits(&stream, &sign, 1);
            i += 2;
        } else {
            if (prefix == 0b10) {
                pull_bits(&stream, &value, 2);
                pull_bits(&stream, &sign, 1);
                value += 2;
                i += 3;
            } else {
                if (prefix == 0b110) {
                    pull_bits(&stream, &value, 4);
                    pull_bits(&stream, &sign, 1);
                    value += 6;
                    i += 5;
                } else {
                    pull_bits(&stream, &value, 8);
                    pull_bits(&stream, &sign, 1);
                    value += 22;
                    i += 9;
                }
            }
        }

        char decoded_value = (sign == 1) ? -value : value;
        dest[N] = decoded_value;
        N++;
    }

    return N;
}

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
    int N = img->width * img->height;

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
    
}

/*! fonction de contrôle      !*/
void ctrl(void) {
    // selection de la fonte : ('n':normal,'l':large,'L':LARGE),('n':normal,'b':bold),('l':left, 'c':center, 'r':right)
    g2x_SetFontAttributes('l', 'b', 'c');
}

void evts(void)
{

}

/*! fonction de dessin        !*/
void draw(void) {
    
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