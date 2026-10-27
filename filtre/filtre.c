#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "filtre.h"

#define M_PI 3.14159265358979323846

typedef struct
{
    int frequence_coupure;
    int taux_echantillonnage;
    float coefficent;
    int etat;
} filtre_t;

filtre_t *filtre_creer(float freq_coupure, int taux)
{
    filtre_t *f = malloc(sizeof(filtre_t));
    if (f == NULL)
        return NULL;

    float tau = 1.0f / (2.0f * (float)M_PI * freq_coupure);
    float dt = 1.0f / taux;

    f->coefficent = dt / (tau + dt);
}

void filtre_detruire(filtre_t *f)
{
}

void filtre_appliquer(filtre_t *f, float *buffer, int nb_echantillons)
{
}

void filtre_set_coupure(filtre_t *f, float freq_coupure)
{
}

void filtre_reset(filtre_t *f)
{
}

float filtre_coupure(const filtre_t *f)
{
}