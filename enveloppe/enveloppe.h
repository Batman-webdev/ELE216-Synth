#ifndef ENVELOPPE_H
#define ENVELOPPE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct enveloppe enveloppe_t;

enveloppe_t *enveloppe_creer(float attack_s, float decay_s, float sustain, float release_s, int taux);
void enveloppe_detruire(enveloppe_t *e);

void enveloppe_appliquer(const enveloppe_t *e, float *buffer, int nb_echantillons, float duree_note);
int enveloppe_duree_totale(const enveloppe_t *e, float duree_note);

void enveloppe_set_attack(enveloppe_t *e, float s);
void enveloppe_set_decay(enveloppe_t *e, float s);
void enveloppe_set_sustain(enveloppe_t *e, float niveau);
void enveloppe_set_release(enveloppe_t *e, float s);

float enveloppe_attack(const enveloppe_t *e);
float enveloppe_decay(const enveloppe_t *e);
float enveloppe_sustain(const enveloppe_t *e);
float enveloppe_release(const enveloppe_t *e);

#ifdef __cplusplus
}
#endif

#endif /* ENVELOPPE_H */