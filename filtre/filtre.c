#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "filtre.h"

#define M_PI 3.14159265358979323846

typedef struct
{
    float frequence_coupure;
    int taux_echantillonnage;
    float coefficent;
    float etat_interne;
} filtre_t;

filtre_t *filtre_creer(float freq_coupure, int taux)
{
    filtre_t *f = malloc(sizeof(filtre_t));
    if (f == NULL)
        return NULL;

    float tau = 1.0f / (2.0f * (float)M_PI * freq_coupure);
    float dt = 1.0f / taux;

    f->coefficent = dt / (tau + dt);
    return f;
}

void filtre_detruire(filtre_t *f)
{
    if (f == NULL)
        return;
    free(f);
    return;
}

void filtre_appliquer(filtre_t *f, float *buffer, int nb_echantillons)
{
    float x = 0.0f;
    f->etat_interne = 0.0f;

    for (int i = 0; i < nb_echantillons; i++)
    {
        x = (f->coefficent * buffer[i]) + (1 - f->coefficent) * f->etat_interne;
        buffer[i] = x;
        f->etat_interne = x;
    }
    return;
}

void filtre_set_coupure(filtre_t *f, float freq_coupure)
{
    f->frequence_coupure = freq_coupure;

    float tau = 1.0f / (2.0f * (float)M_PI * freq_coupure);
    float dt = 1.0f / f->taux_echantillonnage;

    f->coefficent = dt / (tau + dt);
    return;
}

void filtre_reset(filtre_t *f)
{
    f->etat_interne = 0.0f;
    return;
}

float filtre_coupure(const filtre_t *f)
{
    return f->frequence_coupure;
}