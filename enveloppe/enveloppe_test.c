#include "../Unity/unity.h"
#include "enveloppe.h"
 
#include <math.h>
#include <stdlib.h>
 
#define TAUX 8000
#define TOL 0.01f
 
static enveloppe_t *env = NULL;
static float *buf = NULL;
 
void setUp(void)
{
    env = NULL;
    buf = NULL;
}
 
void tearDown(void)
{
    enveloppe_detruire(env);
    free(buf);
    env = NULL;
    buf = NULL;
}
 
static float *buffer_unitaire(int n)
{
    float *b = (float *)malloc((size_t)n * sizeof(float));
    int i;
    TEST_ASSERT_NOT_NULL(b);
    for (i = 0; i < n; ++i) {
        b[i] = 1.0f;
    }
    return b;
}
 
void test_premier_echantillon_proche_de_zero(void)
{
    int n;
 
    env = enveloppe_creer(0.01f, 0.05f, 0.7f, 0.1f, TAUX);
    TEST_ASSERT_NOT_NULL(env);
    n = enveloppe_duree_totale(env, 0.5f);
    buf = buffer_unitaire(n);
 
    enveloppe_appliquer(env, buf, n, 0.5f);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f, buf[0]);
}
 
void test_sustain_egal_au_niveau(void)
{
    int n;
    int i_milieu = (int)(0.3f * TAUX); /* apres A+D (0.06 s), avant fin de note (0.56 s) */
 
    env = enveloppe_creer(0.01f, 0.05f, 0.7f, 0.1f, TAUX);
    TEST_ASSERT_NOT_NULL(env);
    n = enveloppe_duree_totale(env, 0.5f);
    buf = buffer_unitaire(n);
 
    enveloppe_appliquer(env, buf, n, 0.5f);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.7f, buf[i_milieu]);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.7f, buf[i_milieu + 100]);
}
 
void test_dernier_echantillon_release_proche_de_zero(void)
{
    int n;
 
    env = enveloppe_creer(0.01f, 0.05f, 0.7f, 0.1f, TAUX);
    TEST_ASSERT_NOT_NULL(env);
    n = enveloppe_duree_totale(env, 0.5f);
    buf = buffer_unitaire(n);
 
    enveloppe_appliquer(env, buf, n, 0.5f);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f, buf[n - 1]);
}
 
void test_buffer_inchange_si_adsr_neutre(void)
{
    enum { N = 100 }; /* dans la duree de la note (1 s = 8000 echantillons) */
    float copie[N];
    int i;
 
    env = enveloppe_creer(0.0f, 0.0f, 1.0f, 0.0f, TAUX);
    TEST_ASSERT_NOT_NULL(env);
    buf = (float *)malloc(N * sizeof(float));
    TEST_ASSERT_NOT_NULL(buf);
    for (i = 0; i < N; ++i) {
        buf[i] = 0.1f * (float)(i % 7) - 0.3f;
        copie[i] = buf[i];
    }
 
    enveloppe_appliquer(env, buf, N, 1.0f);
    TEST_ASSERT_EQUAL_FLOAT_ARRAY(copie, buf, N);
}
 
void test_facteurs_entre_0_et_1(void)
{
    int n;
    int i;
 
    env = enveloppe_creer(0.008f, 0.12f, 0.7f, 0.24f, TAUX);
    TEST_ASSERT_NOT_NULL(env);
    n = enveloppe_duree_totale(env, 0.4f);
    buf = buffer_unitaire(n);
 
    enveloppe_appliquer(env, buf, n, 0.4f);
    for (i = 0; i < n; ++i) {
        TEST_ASSERT_TRUE(buf[i] >= 0.0f);
        TEST_ASSERT_TRUE(buf[i] <= 1.0f);
    }
}
 
void test_duree_totale(void)
{
    env = enveloppe_creer(0.01f, 0.05f, 0.7f, 0.1f, TAUX);
    TEST_ASSERT_NOT_NULL(env);
    /* (0.01 + 0.05 + 0.5 + 0.1) * 8000 = 5280 */
    TEST_ASSERT_EQUAL_INT(5280, enveloppe_duree_totale(env, 0.5f));
    TEST_ASSERT_EQUAL_INT(0, enveloppe_duree_totale(NULL, 0.5f));
}
 
void test_accesseurs_valeurs_initiales(void)
{
    env = enveloppe_creer(0.01f, 0.05f, 0.7f, 0.1f, TAUX);
    TEST_ASSERT_NOT_NULL(env);
    TEST_ASSERT_EQUAL_FLOAT(0.01f, enveloppe_attack(env));
    TEST_ASSERT_EQUAL_FLOAT(0.05f, enveloppe_decay(env));
    TEST_ASSERT_EQUAL_FLOAT(0.7f, enveloppe_sustain(env));
    TEST_ASSERT_EQUAL_FLOAT(0.1f, enveloppe_release(env));
}
 
void test_mutateurs(void)
{
    env = enveloppe_creer(0.01f, 0.05f, 0.7f, 0.1f, TAUX);
    TEST_ASSERT_NOT_NULL(env);
 
    enveloppe_set_attack(env, 0.02f);
    enveloppe_set_decay(env, 0.03f);
    enveloppe_set_sustain(env, 0.4f);
    enveloppe_set_release(env, 0.2f);
    TEST_ASSERT_EQUAL_FLOAT(0.02f, enveloppe_attack(env));
    TEST_ASSERT_EQUAL_FLOAT(0.03f, enveloppe_decay(env));
    TEST_ASSERT_EQUAL_FLOAT(0.4f, enveloppe_sustain(env));
    TEST_ASSERT_EQUAL_FLOAT(0.2f, enveloppe_release(env));
}
 
void test_bornage_des_parametres(void)
{
    env = enveloppe_creer(0.01f, 0.05f, 0.7f, 0.1f, TAUX);
    TEST_ASSERT_NOT_NULL(env);
 
    enveloppe_set_sustain(env, 2.0f);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, enveloppe_sustain(env));
    enveloppe_set_sustain(env, -1.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, enveloppe_sustain(env));
    enveloppe_set_attack(env, -1.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, enveloppe_attack(env));
}
 
void test_creation_taux_invalide(void)
{
    TEST_ASSERT_NULL(enveloppe_creer(0.01f, 0.05f, 0.7f, 0.1f, 0));
    TEST_ASSERT_NULL(enveloppe_creer(0.01f, 0.05f, 0.7f, 0.1f, -1));
}
 
void test_detruire_null(void)
{
    enveloppe_detruire(NULL);
    TEST_PASS();
}
 
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_premier_echantillon_proche_de_zero);
    RUN_TEST(test_sustain_egal_au_niveau);
    RUN_TEST(test_dernier_echantillon_release_proche_de_zero);
    RUN_TEST(test_buffer_inchange_si_adsr_neutre);
    RUN_TEST(test_facteurs_entre_0_et_1);
    RUN_TEST(test_duree_totale);
    RUN_TEST(test_accesseurs_valeurs_initiales);
    RUN_TEST(test_mutateurs);
    RUN_TEST(test_bornage_des_parametres);
    RUN_TEST(test_creation_taux_invalide);
    RUN_TEST(test_detruire_null);
    return UNITY_END();
}
