#include <stdio.h>
#include "interface.h"
#include "../oscillateur/oscillateur.h"
#include "../filtre/filtre.h"
#include "../enveloppe/enveloppe.h"

void setUp(void){}
void tearDown(void){}

int main(void)
{
    int taux = 44100;

    /* Créer les modules */
    oscillateur_t *osc = oscillateur_creer(
        ONDE_SINE, 440.0f, 0.5f, taux
    );

    filtre_t *flt = filtre_creer(2000.0f, taux);

    enveloppe_t *env = enveloppe_creer(
        0.01f, 0.1f, 0.7f, 0.2f, taux
    );

    if (osc == NULL || flt == NULL || env == NULL)
    {
        printf("Erreur lors de la creation des modules.\n");

        if (osc != NULL) oscillateur_detruire(osc);
        if (flt != NULL) filtre_detruire(flt);
        if (env != NULL) enveloppe_detruire(env);

        return 1;
    }

    /* Créer l'interface */
    interface_t *ui = interface_creer(osc, flt, env);

    if (ui == NULL)
    {
        printf("Erreur lors de la creation de l'interface.\n");

        oscillateur_detruire(osc);
        filtre_detruire(flt);
        enveloppe_detruire(env);

        return 1;
    }

    /* Lancer la boucle graphique */
    interface_boucle(ui);

    /* Libérer les ressources */
    interface_detruire(ui);

    oscillateur_detruire(osc);
    filtre_detruire(flt);
    enveloppe_detruire(env);

    return 0;
}
