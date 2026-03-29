/*
 * Université de Lille
 * Calcul de l'ensemble de Mandelbrot, Version parallèle OMP avec schedule(runtime)
 *
 * Le type de scheduling (static, dynamic, guided...) et la taille de chunk
 * sont contrôlés à l'exécution via la variable d'environnement OMP_SCHEDULE.
 *
 * Exemples :
 *   OMP_SCHEDULE="static"        ./mandel-para-schedule 800 800 -2 -2 2 2 10000
 *   OMP_SCHEDULE="dynamic,1"     ./mandel-para-schedule 800 800 -2 -2 2 2 10000
 *   OMP_SCHEDULE="guided,4"      ./mandel-para-schedule 800 800 -2 -2 2 2 10000
 *
 * Compilation :
 *   gcc -O2 -fopenmp -o mandel-para-schedule mandel-para-schedule.c -lm
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>   /* chronometrage */
#include <string.h> /* pour memset */
#include <math.h>
#include <sys/time.h>
#include <omp.h>

#include "rasterfile.h"

char info[] = "\
Usage:\n\
      mandel-para-schedule dimx dimy xmin ymin xmax ymax prof\n\
\n\
      dimx,dimy : dimensions de l'image a generer\n\
      xmin,ymin,xmax,ymax : domaine a calculer dans le plan complexe\n\
      prof : nombre maximale d'iteration\n\
\n\
Le type de scheduling est controle par la variable OMP_SCHEDULE :\n\
      export OMP_SCHEDULE=\"static\"      # repartition statique\n\
      export OMP_SCHEDULE=\"dynamic,1\"   # repartition dynamique (chunk=1)\n\
      export OMP_SCHEDULE=\"guided,4\"    # repartition guidee (chunk min=4)\n\
\n\
Quelques exemples d'execution\n\
      mandel-para-schedule 800 800 0.35 0.355 0.353 0.358 200\n\
      mandel-para-schedule 800 800 -0.736 -0.184 -0.735 -0.183 500\n\
      mandel-para-schedule 800 800 -0.736 -0.184 -0.735 -0.183 300\n\
      mandel-para-schedule 800 800 -1.48478 0.00006 -1.48440 0.00044 100\n\
      mandel-para-schedule 800 800 -1.5 -0.1 -1.3 0.1 10000\n\
";

double my_gettimeofday()
{
    struct timeval tmp_time;
    gettimeofday(&tmp_time, NULL);
    return tmp_time.tv_sec + (tmp_time.tv_usec * 1.0e-6L);
}

/**
 * Convertion entier (4 octets) LINUX en un entier SUN
 */
int swap(int i)
{
    int init = i;
    int conv;
    unsigned char *o, *d;

    o = ((unsigned char *)&init) + 3;
    d = (unsigned char *)&conv;

    *d++ = *o--;
    *d++ = *o--;
    *d++ = *o--;
    *d++ = *o--;

    return conv;
}

/***
 * Par Francois-Xavier MOREL (M2 SAR, oct2009):
 */

unsigned char power_composante(int i, int p)
{
    unsigned char o;
    double iD = (double)i;

    iD /= 255.0;
    iD = pow(iD, p);
    iD *= 255;
    o = (unsigned char)iD;
    return o;
}

unsigned char cos_composante(int i, double freq)
{
    unsigned char o;
    double iD = (double)i;
    iD = cos(iD / 255.0 * 2 * M_PI * freq);
    iD += 1;
    iD *= 128;

    o = (unsigned char)iD;
    return o;
}

/***
 * Choix du coloriage : definir une (et une seule) des constantes ci-dessous
 */
// #define ORIGINAL_COLOR
#define COS_COLOR

#ifdef ORIGINAL_COLOR
#define COMPOSANTE_ROUGE(i) ((i) / 2)
#define COMPOSANTE_VERT(i) ((i) % 190)
#define COMPOSANTE_BLEU(i) (((i) % 120) * 2)
#endif /* #ifdef ORIGINAL_COLOR */
#ifdef COS_COLOR
#define COMPOSANTE_ROUGE(i) cos_composante(i, 13.0)
#define COMPOSANTE_VERT(i) cos_composante(i, 5.0)
#define COMPOSANTE_BLEU(i) cos_composante(i + 10, 7.0)
#endif /* #ifdef COS_COLOR */

/**
 * Sauvegarde le tableau de données au format rasterfile
 */
void sauver_rasterfile(char *nom, int largeur, int hauteur, unsigned char *p)
{
    FILE *fd;
    struct rasterfile file;
    int i;
    unsigned char o;

    if ((fd = fopen(nom, "wb")) == NULL)
    {
        printf("erreur dans la creation du fichier %s \n", nom);
        exit(1);
    }

    file.ras_magic = swap(RAS_MAGIC);
    file.ras_width = swap(largeur);
    file.ras_height = swap(hauteur);
    file.ras_depth = swap(8);
    file.ras_length = swap(largeur * hauteur);
    file.ras_type = swap(RT_STANDARD);
    file.ras_maptype = swap(RMT_EQUAL_RGB);
    file.ras_maplength = swap(256 * 3);

    fwrite(&file, sizeof(struct rasterfile), 1, fd);

    /* Palette de couleurs : composante rouge */
    i = 256;
    while (i--)
    {
        o = COMPOSANTE_ROUGE(i);
        fwrite(&o, sizeof(unsigned char), 1, fd);
    }

    /* Palette de couleurs : composante verte */
    i = 256;
    while (i--)
    {
        o = COMPOSANTE_VERT(i);
        fwrite(&o, sizeof(unsigned char), 1, fd);
    }

    /* Palette de couleurs : composante bleu */
    i = 256;
    while (i--)
    {
        o = COMPOSANTE_BLEU(i);
        fwrite(&o, sizeof(unsigned char), 1, fd);
    }

    fwrite(p, largeur * hauteur, sizeof(unsigned char), fd);
    fclose(fd);
}

/**
 * Étant donnée les coordonnées d'un point c=a+ib dans le plan complexe,
 * retourne la couleur correspondante (distance à l'ensemble de Mandelbrot).
 */
unsigned char xy2color(double a, double b, int prof)
{
    double x, y, temp, x2, y2;
    int i;

    x = y = 0.;
    for (i = 0; i < prof; i++)
    {
        temp = x;
        x2 = x * x;
        y2 = y * y;
        x = x2 - y2 + a;
        y = 2 * temp * y + b;
        if (x2 + y2 > 4.0)
            break;
    }
    return (i == prof) ? 255 : (int)(i % 255);
}

/*
 * Partie principale : en chaque point de la grille, appliquer xy2color.
 * La boucle externe est parallélisée avec schedule(runtime) :
 * le type de scheduling est choisi via OMP_SCHEDULE à l'exécution.
 */
int main(int argc, char *argv[])
{
    /* Domaine de calcul dans le plan complexe */
    double xmin, ymin;
    double xmax, ymax;

    /* Dimension de l'image */
    int w, h;

    /* Pas d'incrementation */
    double xinc, yinc;

    /* Profondeur d'iteration */
    int prof;

    /* Image resultat */
    unsigned char *ima;

    /* Variables intermediaires */
    double x, y;

    /* Chronometrage */
    double debut, fin;

    /* debut du chronometrage */
    debut = my_gettimeofday();

    if (argc == 1)
        fprintf(stderr, "%s\n", info);

    /* Valeurs par defaut de la fractale */
    xmin = -2;
    ymin = -2;
    xmax = 2;
    ymax = 2;
    w = h = 800;
    prof = 10000;

    /* Recuperation des parametres */
    if (argc > 1)
        w = atoi(argv[1]);
    if (argc > 2)
        h = atoi(argv[2]);
    if (argc > 3)
        xmin = atof(argv[3]);
    if (argc > 4)
        ymin = atof(argv[4]);
    if (argc > 5)
        xmax = atof(argv[5]);
    if (argc > 6)
        ymax = atof(argv[6]);
    if (argc > 7)
        prof = atoi(argv[7]);

    /* Calcul des pas d'incrementation */
    xinc = (xmax - xmin) / (w - 1);
    yinc = (ymax - ymin) / (h - 1);

    /* Affichage des parametres */
    fprintf(stderr, "Domaine: {[%lg,%lg]x[%lg,%lg]}\n", xmin, ymin, xmax, ymax);
    fprintf(stderr, "Increment : %lg %lg\n", xinc, yinc);
    fprintf(stderr, "Prof: %d\n", prof);
    fprintf(stderr, "Dim image: %dx%d\n", w, h);
    fprintf(stderr, "Nb threads OMP: %d\n", omp_get_max_threads());

    /* Allocation memoire du tableau resultat */
    ima = (unsigned char *)malloc(w * h * sizeof(unsigned char));

    if (ima == NULL)
    {
        fprintf(stderr, "Erreur allocation mémoire du tableau \n");
        return 1;
    }

/*
 * Boucle parallèle avec schedule(runtime) :
 * Le type (static, dynamic, guided...) et le chunk sont définis
 * par la variable d'environnement OMP_SCHEDULE avant l'exécution.
 * Exemple : export OMP_SCHEDULE="dynamic,4"
 *
 * Les variables i et j sont privées (déclarées dans la boucle).
 * xmin, ymin, xinc, yinc, w, h, prof sont partagées en lecture seule.
 * ima est partagée en écriture (chaque itération écrit dans une case distincte).
 */
#pragma omp parallel for schedule(runtime) private(x, y)
    for (int i = 0; i < h; i++)
    {
        y = ymin + i * yinc;
        for (int j = 0; j < w; j++)
        {
            x = xmin + j * xinc;
            ima[j + i * w] = xy2color(x, y, prof);
        }
    }

    /* fin du chronometrage */
    fin = my_gettimeofday();
    fprintf(stderr, "Temps total de calcul : %g sec\n", fin - debut);
    fprintf(stdout, "%g\n", fin - debut);

    /* Sauvegarde de la grille dans le fichier resultat */
    sauver_rasterfile("mandel_openmp_dynamic.ras", w, h, ima);

    free(ima);
    return 0;
}
