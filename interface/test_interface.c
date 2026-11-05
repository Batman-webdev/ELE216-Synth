#include <stdio.h>
#include "interface.h"
#include "oscillateur.h"
#include "filtre.h"
#include "enveloppe.h"

int main(void)
{
    /* Créer les modules */
    oscillateur_t *osc = oscillateur_creer();
    filtre_t *flt = filtre_creer();
    enveloppe_t *env = enveloppe_creer();

    if (osc == NULL || flt == NULL || env == NULL)
    {
        printf("Erreur lors de la creation des modules.\n");
        return 1;
    }

    /* Créer l'interface */
    interface_t *ui = interface_creer(osc, flt, env);

    if (ui == NULL)
    {
        printf("Erreur lors de la creation de l'interface.\n");
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