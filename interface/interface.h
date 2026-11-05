#ifndef INTERFACE_H
#define INTERFACE_H

typedef struct interface interface_t;

/**
 * @brief Initialise SDL (vidéo et audio), crée la fenêtre, dessine le panneau initial.
 * 
 * @param osc 
 * @param flt 
 * @param env 
 * @return interface_t* 
 */
interface_t *interface_creer(oscillateur_t *osc, filtre_t *flt, enveloppe_t *env);

/**
 * @brief Libère les ressources SDL.
 * 
 * @param ui 
 */
void interface_detruire(interface_t *ui);

/**
 * @brief Boucle d’événements principale.
 * 
 * @param ui 
 */
void interface_boucle(interface_t *ui);

#endif