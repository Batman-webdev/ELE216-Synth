#include <stdio.h>
#include "../interface/interface.h"
#include "../oscillateur/oscillateur.h"
#include "../filtre/filtre.h"
#include "../enveloppe/enveloppe.h"

void setUp(void){}
void tearDown(void){}

int main(void)
{
    const int taux = 44100;

    /* 1. Creation de l'oscillateur */
    oscillateur_t *osc = oscillateur_creer(
        ONDE_SINE,
        440.0f,
        0.8f,
        taux
    );

    if (osc == NULL)
    {
        printf("Erreur : impossible de creer l'oscillateur.\n");
        return 1;
    }

    /* 2. Creation du filtre passe-bas */
    filtre_t *flt = filtre_creer(
        2000.0f,
        taux
    );

    if (flt == NULL)
    {
        printf("Erreur : impossible de creer le filtre.\n");
        oscillateur_detruire(osc);
        return 1;
    }

    /* 3. Creation de l'enveloppe ADSR */
    enveloppe_t *env = enveloppe_creer(
        0.008f,  /* Attack */
        0.120f,  /* Decay */
        0.7f,    /* Sustain */
        0.240f,  /* Release */
        taux
    );

    if (env == NULL)
    {
        printf("Erreur : impossible de creer l'enveloppe.\n");
        oscillateur_detruire(osc);
        filtre_detruire(flt);
        return 1;
    }

    /* 4. Creation de l'interface SDL */
    interface_t *ui = interface_creer(osc, flt, env);

    if (ui == NULL)
    {
        printf("Erreur : impossible de creer l'interface SDL.\n");

        oscillateur_detruire(osc);
        filtre_detruire(flt);
        enveloppe_detruire(env);

        return 1;
    }

    printf("Interface du synthetiseur demarree.\n");

    /* 5. Lancement de la boucle graphique */
    interface_boucle(ui);

    /* 6. Liberation des ressources */
    interface_detruire(ui);

    oscillateur_detruire(osc);
    filtre_detruire(flt);
    enveloppe_detruire(env);

    printf("Synthetiseur ferme correctement.\n");

    return 0;
}