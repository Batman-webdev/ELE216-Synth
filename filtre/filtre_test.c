#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "filtre.h"
#include "../Unity/unity.h"

#define PI_F 3.14159265358979323846f
#define TAUX 48000

void setUp(void)
{
    // This function is called before each test case
}

void tearDown(void)
{
    // This function is called after each test case
}

/* ---------- Utilitaires de test ---------- */

/* Signal carre d'amplitude +/-1, periode de 48 echantillons (1 kHz a 48 kHz) */
static void generer_carre(float *buffer, int n)
{
    for (int i = 0; i < n; i++)
        buffer[i] = ((i / 24) % 2 == 0) ? 1.0f : -1.0f;
}

/* Amplitude crete sur la seconde moitie du buffer (regime etabli) */
static float amplitude_regime_etabli(const float *buffer, int n)
{
    float max = 0.0f;
    for (int i = n / 2; i < n; i++)
    {
        float a = fabsf(buffer[i]);
        if (a > max)
            max = a;
    }
    return max;
}

/* Ecrit un WAV PCM 16 bits mono : entete de 44 octets + 2 octets par echantillon.
 * NOTE : filtre.h ne contient pas de fonction d'export WAV. Cet utilitaire local
 * sert aux tests 5 et 6. Quand ta vraie fonction existera, remplace l'appel
 * a ecrire_wav() par la tienne. */
static int ecrire_wav(const char *chemin, const float *buffer, int n, int taux)
{
    FILE *fp = fopen(chemin, "wb");
    if (fp == NULL)
        return -1;

    uint32_t taille_data = (uint32_t)n * 2u;
    uint32_t taille_riff = 36u + taille_data;
    uint32_t taux_u = (uint32_t)taux;
    uint32_t byte_rate = taux_u * 2u;
    uint32_t fmt_taille = 16u;
    uint16_t format_pcm = 1, canaux = 1, block_align = 2, bits = 16;

    fwrite("RIFF", 1, 4, fp);
    fwrite(&taille_riff, 4, 1, fp);
    fwrite("WAVE", 1, 4, fp);
    fwrite("fmt ", 1, 4, fp);
    fwrite(&fmt_taille, 4, 1, fp);
    fwrite(&format_pcm, 2, 1, fp);
    fwrite(&canaux, 2, 1, fp);
    fwrite(&taux_u, 4, 1, fp);
    fwrite(&byte_rate, 4, 1, fp);
    fwrite(&block_align, 2, 1, fp);
    fwrite(&bits, 2, 1, fp);
    fwrite("data", 1, 4, fp);
    fwrite(&taille_data, 4, 1, fp);

    for (int i = 0; i < n; i++)
    {
        float v = buffer[i];
        if (v > 1.0f)
            v = 1.0f;
        if (v < -1.0f)
            v = -1.0f;
        int16_t s = (int16_t)(v * 32767.0f);
        fwrite(&s, 2, 1, fp);
    }

    fclose(fp);
    return 0;
}

/* ---------- Tests ---------- */

/* 1. Un signal constant n'est pas modifie par le filtre */
void test_signal_constant(void)
{
    filtre_t *f = filtre_creer(20000, 50);
    float *buffer = malloc(5 * sizeof(float));
    for (int i = 0; i < 5; i++)
    {
        buffer[i] = 5.0f;
    }

    filtre_appliquer(f, buffer, 5);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, buffer[4]);
    filtre_detruire(f);
    free(buffer);
}

/* 2. Coupure tres basse (20 Hz) sur un signal carre : amplitude reduite */
void test_coupure_basse_signal_carre(void)
{
    const int n = 4800;
    filtre_t *f = filtre_creer(20.0f, TAUX);
    float *buffer = malloc(n * sizeof(float));
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_NOT_NULL(buffer);

    generer_carre(buffer, n);
    filtre_appliquer(f, buffer, n);

    float amp = amplitude_regime_etabli(buffer, n);
    TEST_ASSERT_TRUE(amp < 0.1f); /* amplitude d'entree = 1.0 */

    filtre_detruire(f);
    free(buffer);
}

/* 3. Coupure tres haute (20 kHz) : signal presque inchange */
void test_coupure_haute_signal_presque_inchange(void)
{
    const int n = 4800;
    filtre_t *f = filtre_creer(20000.0f, TAUX);
    float *original = malloc(n * sizeof(float));
    float *buffer = malloc(n * sizeof(float));
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_NOT_NULL(original);
    TEST_ASSERT_NOT_NULL(buffer);

    /* Sinus 100 Hz, bien en dessous de la coupure */
    for (int i = 0; i < n; i++)
    {
        original[i] = sinf(2.0f * PI_F * 100.0f * (float)i / (float)TAUX);
        buffer[i] = original[i];
    }

    filtre_appliquer(f, buffer, n);

    /* On ignore les premiers echantillons (transitoire d'initialisation) */
    for (int i = 50; i < n; i++)
        TEST_ASSERT_FLOAT_WITHIN(0.02f, original[i], buffer[i]);

    filtre_detruire(f);
    free(original);
    free(buffer);
}

/* 4. Coupure = 0 : sortie nulle */
void test_coupure_zero_sortie_nulle(void)
{
    const int n = 100;
    filtre_t *f = filtre_creer(0.0f, TAUX);
    float *buffer = malloc(n * sizeof(float));
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_NOT_NULL(buffer);

    generer_carre(buffer, n);
    filtre_appliquer(f, buffer, n);

    for (int i = 0; i < n; i++)
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, buffer[i]);

    filtre_detruire(f);
    free(buffer);
}

/* 5. Export WAV : taille du fichier = 44 + nb_echantillons * 2 */
void test_wav_taille_fichier(void)
{
    const int n = 1000;
    const char *chemin = "test_taille.wav";
    float *buffer = malloc(n * sizeof(float));
    TEST_ASSERT_NOT_NULL(buffer);

    generer_carre(buffer, n);
    TEST_ASSERT_EQUAL_INT(0, ecrire_wav(chemin, buffer, n, TAUX));

    FILE *fp = fopen(chemin, "rb");
    TEST_ASSERT_NOT_NULL(fp);
    fseek(fp, 0, SEEK_END);
    long taille = ftell(fp);
    fclose(fp);
    remove(chemin);
    free(buffer);

    TEST_ASSERT_EQUAL_INT(44 + n * 2, (int)taille);
}

/* 6. Export WAV : les 4 premiers octets sont 'RIFF' */
void test_wav_entete_riff(void)
{
    const int n = 100;
    const char *chemin = "test_riff.wav";
    float *buffer = malloc(n * sizeof(float));
    TEST_ASSERT_NOT_NULL(buffer);

    generer_carre(buffer, n);
    TEST_ASSERT_EQUAL_INT(0, ecrire_wav(chemin, buffer, n, TAUX));

    char entete[4] = {0};
    FILE *fp = fopen(chemin, "rb");
    TEST_ASSERT_NOT_NULL(fp);
    TEST_ASSERT_EQUAL_INT(4, (int)fread(entete, 1, 4, fp));
    fclose(fp);
    remove(chemin);
    free(buffer);

    TEST_ASSERT_EQUAL_MEMORY("RIFF", entete, 4);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_signal_constant);
    RUN_TEST(test_coupure_basse_signal_carre);
    RUN_TEST(test_coupure_haute_signal_presque_inchange);
    RUN_TEST(test_coupure_zero_sortie_nulle);
    RUN_TEST(test_wav_taille_fichier);
    RUN_TEST(test_wav_entete_riff);
    return UNITY_END();
}