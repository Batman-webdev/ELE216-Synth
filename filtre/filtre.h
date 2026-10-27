#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

typedef struct
{
} filtre_t;

filtre_t *filtre_creer(float freq_coupure, int taux);

void filtre_detruire(filtre_t *f);

void filtre_appliquer(filtre_t *f, float *buffer, int nb_echantillons);

void filtre_set_coupure(filtre_t *f, float freq_coupure);

void filtre_reset(filtre_t *f);

float filtre_coupure(const filtre_t *f);
