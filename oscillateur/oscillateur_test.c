#include "unity.h"
#include "oscillateur.h"
#include <math.h>

void setUp(void){}
void tearDown(void){}

/* ---------- creation / accesseurs ---------- */

void creation_valide(void){
    oscillateur_t *o = oscillateur_creer(ONDE_TRI, 220.0f, 1.0f, 44100);

    TEST_ASSERT_NOT_NULL(o);
    TEST_ASSERT(oscillateur_type(o) == ONDE_TRI);
    TEST_ASSERT_EQUAL_FLOAT(220.0f, oscillateur_frequence(o));

    oscillateur_detruire(o);
}

void creation_taux_invalide(void){
    oscillateur_t *o = oscillateur_creer(ONDE_SINE, 440.0f, 1.0f, 0);

    TEST_ASSERT_NULL(o);
}

void setters(void){
    oscillateur_t *o = oscillateur_creer(ONDE_SINE, 440.0f, 1.0f, 44100);

    oscillateur_set_type(o, ONDE_SAW);
    oscillateur_set_frequence(o, 880.0f);

    TEST_ASSERT(oscillateur_type(o) == ONDE_SAW);
    TEST_ASSERT_EQUAL_FLOAT(880.0f, oscillateur_frequence(o));

    oscillateur_detruire(o);
}

/* ---------- SINE ---------- */

void sine_440hz_periode_environ_100(void){
    enum { N = 1000 };
    float buf[N];
    oscillateur_t *o = oscillateur_creer(ONDE_SINE, 440.0f, 1.0f, 44100);

    oscillateur_generer(o, buf, N);

    /* periode moyenne entre les passages par zero montants */
    int premier = -1, dernier = -1, nb = 0;
    for (int i = 1; i < N; i++){
        if (buf[i - 1] < 0.0f && buf[i] >= 0.0f){
            if (premier < 0) premier = i;
            dernier = i;
            nb++;
        }
    }
    TEST_ASSERT_TRUE(nb > 1);

    float periode = (float)(dernier - premier) / (float)(nb - 1);
    TEST_ASSERT_FLOAT_WITHIN(1.5f, 100.0f, periode);

    oscillateur_detruire(o);
}

/* ---------- SQUARE ---------- */

void square_alterne_entre_plus_et_moins_amplitude(void){
    enum { N = 500 };
    const float A = 0.7f;
    float buf[N];
    int pos = 0, neg = 0;
    oscillateur_t *o = oscillateur_creer(ONDE_SQUARE, 440.0f, A, 44100);

    oscillateur_generer(o, buf, N);

    for (int i = 0; i < N; i++){
        if (buf[i] > 0.0f){
            TEST_ASSERT_EQUAL_FLOAT(A, buf[i]);
            pos++;
        } else {
            TEST_ASSERT_EQUAL_FLOAT(-A, buf[i]);
            neg++;
        }
    }
    TEST_ASSERT_TRUE(pos > 0);
    TEST_ASSERT_TRUE(neg > 0);

    oscillateur_detruire(o);
}

/* ---------- amplitude / bornes ---------- */

void amplitude_zero_donne_des_zeros(void){
    float buf[256];

    for (onde_type_t t = ONDE_SINE; t <= ONDE_SAW; t++){
        oscillateur_t *o = oscillateur_creer(t, 440.0f, 0.0f, 44100);

        oscillateur_generer(o, buf, 256);

        for (int i = 0; i < 256; i++)
            TEST_ASSERT_EQUAL_FLOAT(0.0f, buf[i]);

        oscillateur_detruire(o);
    }
}

void echantillons_dans_les_bornes(void){
    enum { N = 5000 };
    const float A = 0.5f;
    static float buf[N];

    for (onde_type_t t = ONDE_SINE; t <= ONDE_SAW; t++){
        oscillateur_t *o = oscillateur_creer(t, 1234.5f, A, 44100);

        oscillateur_generer(o, buf, N);

        for (int i = 0; i < N; i++){
            TEST_ASSERT_TRUE(buf[i] >= -A - 1e-6f);
            TEST_ASSERT_TRUE(buf[i] <= A + 1e-6f);
        }

        oscillateur_detruire(o);
    }
}

/* ---------- continuite / phase ---------- */

void deux_appels_successifs_sont_continus(void){
    enum { N = 137 };   /* volontairement different de la periode */
    float deux[2 * N], un[2 * N];

    for (onde_type_t t = ONDE_SINE; t <= ONDE_SAW; t++){
        oscillateur_t *a = oscillateur_creer(t, 440.0f, 1.0f, 44100);
        oscillateur_t *b = oscillateur_creer(t, 440.0f, 1.0f, 44100);

        oscillateur_generer(a, deux, N);
        oscillateur_generer(a, deux + N, N);
        oscillateur_generer(b, un, 2 * N);

        for (int i = 0; i < 2 * N; i++)
            TEST_ASSERT_FLOAT_WITHIN(1e-4f, un[i], deux[i]);

        oscillateur_detruire(a);
        oscillateur_detruire(b);
    }
}

void reset_phase_redemarre_au_meme_point(void){
    float a[64], b[64];
    oscillateur_t *o = oscillateur_creer(ONDE_SAW, 880.0f, 1.0f, 44100);

    oscillateur_generer(o, a, 64);
    oscillateur_reset_phase(o);
    oscillateur_generer(o, b, 64);

    for (int i = 0; i < 64; i++)
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, a[i], b[i]);

    oscillateur_detruire(o);
}


int main(void){

    UNITY_BEGIN();
    RUN_TEST(creation_valide);
    RUN_TEST(creation_taux_invalide);
    RUN_TEST(setters);
    RUN_TEST(sine_440hz_periode_environ_100);
    RUN_TEST(square_alterne_entre_plus_et_moins_amplitude);
    RUN_TEST(amplitude_zero_donne_des_zeros);
    RUN_TEST(echantillons_dans_les_bornes);
    RUN_TEST(deux_appels_successifs_sont_continus);
    RUN_TEST(reset_phase_redemarre_au_meme_point);
    return UNITY_END();

}