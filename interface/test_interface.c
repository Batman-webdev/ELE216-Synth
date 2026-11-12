#include <stdio.h>
#include "interface.h"
#include "../oscillateur/oscillateur.h"
#include "../filtre/filtre.h"
#include "../enveloppe/enveloppe.h"
#include "../Unity/unity.h"

static oscillateur_t *osc;
static filtre_t *flt;
static enveloppe_t *env;

void setUp(void)
{
    int taux = 44100;

    osc = oscillateur_creer(ONDE_SINE, 440.0f, 0.5f, taux);
    flt = filtre_creer(2000.0f, taux);
    env = enveloppe_creer(0.01f, 0.1f, 0.7f, 0.2f, taux);
}


void tearDown(void)
{
    if (osc != NULL)
    {
        oscillateur_detruire(osc);
        osc = NULL;
    }

    if (flt != NULL)
    {
        filtre_detruire(flt);
        flt = NULL;
    }

    if (env != NULL)
    {
        enveloppe_detruire(env);
        env = NULL;
    }
}

void test_interface_creer_valide(void)
{
    TEST_ASSERT_NOT_NULL(osc);
    TEST_ASSERT_NOT_NULL(flt);
    TEST_ASSERT_NOT_NULL(env);

    interface_t *ui = interface_creer(osc, flt, env);

    TEST_ASSERT_NOT_NULL(ui);

    interface_detruire(ui);
}


void test_interface_creer_osc_null(void)
{
    interface_t *ui = interface_creer(NULL, flt, env);

    TEST_ASSERT_NULL(ui);
}

void test_interface_creer_filtre_null(void)
{
    interface_t *ui = interface_creer(osc, NULL, env);

    TEST_ASSERT_NULL(ui);
}

void test_interface_creer_enveloppe_null(void)
{
    interface_t *ui = interface_creer(osc, flt, NULL);

    TEST_ASSERT_NULL(ui);
}

void test_interface_creer_tous_null(void)
{
    interface_t *ui = interface_creer(NULL, NULL, NULL);

    TEST_ASSERT_NULL(ui);
}

void test_interface_detruire_null(void)
{
    interface_detruire(NULL);

    TEST_PASS();
}


void test_interface_boucle_null(void)
{
    interface_boucle(NULL);

    TEST_PASS();
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_interface_creer_valide);
    RUN_TEST(test_interface_creer_osc_null);
    RUN_TEST(test_interface_creer_filtre_null);
    RUN_TEST(test_interface_creer_enveloppe_null);
    RUN_TEST(test_interface_creer_tous_null);
    RUN_TEST(test_interface_detruire_null);
    RUN_TEST(test_interface_boucle_null);

    return UNITY_END();
}