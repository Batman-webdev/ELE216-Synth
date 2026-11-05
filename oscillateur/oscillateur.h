#ifndef MODULE_A_H
#define MODULE_A_H
#include <stdlib.h>


typedef struct oscillateur_t oscillateur_t;

typedef enum {
    ONDE_SINE,
    ONDE_SQUARE,
    ONDE_TRI,
    ONDE_SAW
} onde_type_t;

//Alloue et initialise.
oscillateur_t *oscillateur_creer(onde_type_t type, float frequence, float amplitude, int taux);

//Libère la mémoire.
void oscillateur_detruire(oscillateur_t *o);

//Remplit le tampon. La phase s’accumule entre les appels pour assurer la continuité.
void oscillateur_generer(oscillateur_t *o, float *buffer, int nb_echantillons);

//Modifie la fréquence.
void oscillateur_set_frequence(oscillateur_t *o, float freq);

//Change la forme d’onde.
void oscillateur_set_type(oscillateur_t *o, onde_type_t type);

//Change la forme d’onde.
void oscillateur_reset_phase(oscillateur_t *o);

//Change la forme d’onde.
float oscillateur_frequence(const oscillateur_t *o);
onde_type_t oscillateur_type(const oscillateur_t *o);



#endif