#ifndef OSCILLATEUR_H
#define OSCILLATEUR_H
 
/* Formes d'onde supportees. Enum public : il est passe par valeur. */
typedef enum {
    ONDE_SINE,
    ONDE_SQUARE,
    ONDE_TRI,
    ONDE_SAW
} onde_type_t;
 
/* Type opaque : la structure est definie dans module_A.c */
typedef struct oscillateur_t oscillateur_t;
 
/* Alloue et initialise. Retourne NULL en cas d'echec (memoire ou parametres invalides). */
oscillateur_t *oscillateur_creer(onde_type_t type, float frequence, float amplitude, int taux);
 
/* Libere la memoire. Accepte NULL. */
void oscillateur_detruire(oscillateur_t *o);
 
/* Remplit le tampon. La phase s'accumule entre les appels pour assurer la continuite. */
void oscillateur_generer(oscillateur_t *o, float *buffer, int nb_echantillons);
 
/* Modifie la frequence (Hz). */
void oscillateur_set_frequence(oscillateur_t *o, float freq);
 
/* Change la forme d'onde. */
void oscillateur_set_type(oscillateur_t *o, onde_type_t type);
 
/* Remet la phase a zero. */
void oscillateur_reset_phase(oscillateur_t *o);
 
/* Accesseurs. */
float oscillateur_frequence(const oscillateur_t *o);
onde_type_t oscillateur_type(const oscillateur_t *o);
 
#endif
 