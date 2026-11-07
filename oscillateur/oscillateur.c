#include "oscillateur.h"
#include <stdlib.h>
#include <math.h>

#define DEUX_PI 6.28318530717958647692f

struct oscillateur_t {
    onde_type_t type_onde;
    float frequence;        /* Hz */
    float amplitude;        /* 0.0 a 1.0 */
    int taux_echantillon;   /* Hz */
    float phase_accumulee;  /* dans [0, 1) : fraction de periode */
};

oscillateur_t *oscillateur_creer(onde_type_t type, float frequence, float amplitude, int taux)
{
    if (taux <= 0 || frequence < 0.0f)
        return NULL;

    oscillateur_t *o = malloc(sizeof *o);
    if (o == NULL)
        return NULL;

    if (amplitude < 0.0f) amplitude = 0.0f;
    if (amplitude > 1.0f) amplitude = 1.0f;

    o->type_onde = type;
    o->frequence = frequence;
    o->amplitude = amplitude;
    o->taux_echantillon = taux;
    o->phase_accumulee = 0.0f;
    return o;
}

void oscillateur_detruire(oscillateur_t *o)
{
    free(o);
}

void oscillateur_generer(oscillateur_t *o, float *buffer, int nb_echantillons)
{
    if (o == NULL || buffer == NULL || nb_echantillons <= 0)
        return;

    const float increment = o->frequence / (float)o->taux_echantillon;
    float phase = o->phase_accumulee;

    for (int i = 0; i < nb_echantillons; i++) {
        float valeur = 0.0f;

        switch (o->type_onde) {
        case ONDE_SINE:
            valeur = sinf(DEUX_PI * phase);
            break;
        case ONDE_SQUARE:
            valeur = (phase < 0.5f) ? 1.0f : -1.0f;
            break;
        case ONDE_TRI:
            valeur = (phase < 0.5f) ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);
            break;
        case ONDE_SAW:
            valeur = 2.0f * phase - 1.0f;
            break;
        }

        buffer[i] = o->amplitude * valeur;

        phase += increment;
        while (phase >= 1.0f)
            phase -= 1.0f;
    }

    o->phase_accumulee = phase;
}

void oscillateur_set_frequence(oscillateur_t *o, float freq)
{
    if (o == NULL || freq < 0.0f)
        return;
    o->frequence = freq;
}

void oscillateur_set_type(oscillateur_t *o, onde_type_t type)
{
    if (o == NULL)
        return;
    o->type_onde = type;
}

void oscillateur_reset_phase(oscillateur_t *o)
{
    if (o == NULL)
        return;
    o->phase_accumulee = 0.0f;
}

float oscillateur_frequence(const oscillateur_t *o)
{
    return (o != NULL) ? o->frequence : 0.0f;
}

onde_type_t oscillateur_type(const oscillateur_t *o)
{
    return (o != NULL) ? o->type_onde : ONDE_SINE;
}