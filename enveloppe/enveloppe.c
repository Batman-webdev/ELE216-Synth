#include "enveloppe.h"

#include <math.h>
#include <stdlib.h>

struct enveloppe {
    float attack_s;
    float decay_s;
    float sustain;
    float release_s;
    int taux_echantillonnage;
};

static float clampf(float value, float min_value, float max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

enveloppe_t *enveloppe_creer(float attack_s, float decay_s, float sustain, float release_s, int taux)
{
    enveloppe_t *e;

    if (taux <= 0) {
        return NULL;
    }

    e = (enveloppe_t *)calloc(1, sizeof(*e));
    if (e == NULL) {
        return NULL;
    }

    e->attack_s = attack_s < 0.0f ? 0.0f : attack_s;
    e->decay_s = decay_s < 0.0f ? 0.0f : decay_s;
    e->sustain = clampf(sustain, 0.0f, 1.0f);
    e->release_s = release_s < 0.0f ? 0.0f : release_s;
    e->taux_echantillonnage = taux;

    return e;
}

void enveloppe_detruire(enveloppe_t *e)
{
    free(e);
}

void enveloppe_appliquer(const enveloppe_t *e, float *buffer, int nb_echantillons, float duree_note)
{
    int i;
    float attack_s;
    float decay_s;
    float release_s;
    float note_s;
    float temps;
    float amplitude;

    if (e == NULL || buffer == NULL || nb_echantillons <= 0 || e->taux_echantillonnage <= 0) {
        return;
    }

    attack_s = e->attack_s < 0.0f ? 0.0f : e->attack_s;
    decay_s = e->decay_s < 0.0f ? 0.0f : e->decay_s;
    release_s = e->release_s < 0.0f ? 0.0f : e->release_s;
    note_s = duree_note < 0.0f ? 0.0f : duree_note;

    for (i = 0; i < nb_echantillons; ++i) {
        temps = (float)i / (float)e->taux_echantillonnage;
        amplitude = 0.0f;

        if (attack_s > 0.0f && temps < attack_s) {
            amplitude = temps / attack_s;
        } else if (temps < attack_s + decay_s) {
            if (decay_s > 0.0f) {
                amplitude = 1.0f - ((temps - attack_s) / decay_s) * (1.0f - e->sustain);
            } else {
                amplitude = e->sustain;
            }
        } else if (temps < attack_s + decay_s + note_s) {
            amplitude = e->sustain;
        } else if (release_s > 0.0f) {
            amplitude = e->sustain * (1.0f - ((temps - (attack_s + decay_s + note_s)) / release_s));
            if (amplitude < 0.0f) {
                amplitude = 0.0f;
            }
        }

        if (amplitude < 0.0f) {
            amplitude = 0.0f;
        } else if (amplitude > 1.0f) {
            amplitude = 1.0f;
        }

        buffer[i] *= amplitude;
    }
}

int enveloppe_duree_totale(const enveloppe_t *e, float duree_note)
{
    float duree_totale;

    if (e == NULL || e->taux_echantillonnage <= 0) {
        return 0;
    }

    duree_totale = e->attack_s + e->decay_s + (duree_note < 0.0f ? 0.0f : duree_note) + e->release_s;
    return (int)lroundf(duree_totale * (float)e->taux_echantillonnage);
}

void enveloppe_set_attack(enveloppe_t *e, float s)
{
    if (e == NULL) {
        return;
    }
    e->attack_s = s < 0.0f ? 0.0f : s;
}

void enveloppe_set_decay(enveloppe_t *e, float s)
{
    if (e == NULL) {
        return;
    }
    e->decay_s = s < 0.0f ? 0.0f : s;
}

void enveloppe_set_sustain(enveloppe_t *e, float niveau)
{
    if (e == NULL) {
        return;
    }
    e->sustain = clampf(niveau, 0.0f, 1.0f);
}

void enveloppe_set_release(enveloppe_t *e, float s)
{
    if (e == NULL) {
        return;
    }
    e->release_s = s < 0.0f ? 0.0f : s;
}

float enveloppe_attack(const enveloppe_t *e)
{
    if (e == NULL) {
        return 0.0f;
    }
    return e->attack_s;
}

float enveloppe_decay(const enveloppe_t *e)
{
    if (e == NULL) {
        return 0.0f;
    }
    return e->decay_s;
}

float enveloppe_sustain(const enveloppe_t *e)
{
    if (e == NULL) {
        return 0.0f;
    }
    return e->sustain;
}

float enveloppe_release(const enveloppe_t *e)
{
    if (e == NULL) {
        return 0.0f;
    }
    return e->release_s;
}