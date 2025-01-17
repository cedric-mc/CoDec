#include <differences.h>

bool difalloc(DiffImg *dif, int width, int height) {
    dif->map = (dword *)calloc(width * height, sizeof(dword));
    if (!dif->map) return false;
    dif->end = dif->map + width * height;
    dif->width = width;
    dif->height = height;
    dif->first = 0;
    return true;
}

bool diffree(DiffImg *dif) {
    if (!dif || !dif->map) return false;
    free(dif->map);
    dif->map = dif->end = NULL;
    return true;
}

bool pixtodif(G2Xpixmap *pix, DiffImg *dif) {
    if (!pix || !dif) return false;
    dif->first = *pix->map;
    uchar *p = pix->map + 1;
    dword *d = dif->map + 1;
    dword max = 0;

    while (p < pix->end) {
        *d = *p - *(p - 1);
        if (abs(*d) > max) max = abs(*d);
        p++;
        d++;
    }
    dif->difmax = max;
    return true;
}

bool diftopix(DiffImg *dif, G2Xpixmap *orig) {
    if (!dif || !orig) return false;
    uchar *p = orig->map + 1;
    dword *d = dif->map + 1;
    *p = dif->first;

    while (d < dif->end) {
        *p = *(p - 1) + *d;
        p++;
        d++;
    }
    return true;
}

bool diftovisu(DiffImg *dif, G2Xpixmap *visu) {
    if (!dif || !visu) return false;
    double calib = 1.0 / dif->difmax;
    uchar *p = visu->map + 1;
    dword *d = dif->map + 1;

    while (d < dif->end) {
        *p = (uchar)(255 * abs(*d) * calib);
        p++;
        d++;
    }
    return true;
}

void save_dif_file(const char *filename, G2Xpixmap *img, DiffImg *dif) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Erreur d'ouverture du fichier .dif");
        return;
    }

    int N = img->width * img->height;
    unsigned char *buffer = malloc(1.5 * N);
    int encoded_size = encode_differences(buffer, dif->map, N);

    unsigned short magic = 0xD1FF;
    fwrite(&magic, sizeof(unsigned short), 1, file);
    fwrite(&img->width, sizeof(unsigned short), 1, file);
    fwrite(&img->height, sizeof(unsigned short), 1, file);
    unsigned char quant[4] = {0x01, 0x02, 0x04, 0x08};
    fwrite(quant, sizeof(unsigned char), 4, file);
    fwrite(&dif->first, sizeof(unsigned char), 1, file);
    fwrite(buffer, sizeof(unsigned char), (encoded_size + 7) / 8, file);

    free(buffer);
    fclose(file);
    printf("✅ Image enregistrée sous : %s\n", filename);
}
