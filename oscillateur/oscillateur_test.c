#include "module_A.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

static int echecs = 0;

#define VERIFIER(cond, msg) do { \
    if (!(cond)) { printf("  ECHEC : %s\n", msg); echecs++; } \
    else         { printf("  OK    : %s\n", msg); } \
} while (0)

/* SINE 440 Hz @ 44100 : periode ~ 100 echantillons */
static void test_periode_sine(void)
{
    printf("test_periode_sine\n");
    oscillateur_t *o = oscillateur_creer(ONDE_SINE, 440.0f, 1.0f, 44100);
    enum { N = 1000 };
    float buf[N];
    oscillateur_generer(o, buf, N);

    /* passages par zero montants */
    int premier = -1, dernier = -1, nb = 0;
    for (int i = 1; i < N; i++) {
        if (buf[i - 1] < 0.0f && buf[i] >= 0.0f) {
            if (premier < 0) premier = i;
            dernier = i;
            nb++;
        }
    }
    float periode = (nb > 1) ? (float)(dernier - premier) / (float)(nb - 1) : 0.0f;
    printf("  periode mesuree = %.2f echantillons\n", periode);
    VERIFIER(fabsf(periode - 100.0f) < 1.5f, "periode ~ 100 echantillons");
    oscillateur_detruire(o);
}

/* SQUARE : seulement +amplitude / -amplitude, et les deux apparaissent */
static void test_square(void)
{
    printf("test_square\n");
    const float A = 0.7f;
    oscillateur_t *o = oscillateur_creer(ONDE_SQUARE, 440.0f, A, 44100);
    enum { N = 500 };
    float buf[N];
    oscillateur_generer(o, buf, N);

    int ok = 1, pos = 0, neg = 0;
    for (int i = 0; i < N; i++) {
        if (fabsf(buf[i] - A) < 1e-6f) pos++;
        else if (fabsf(buf[i] + A) < 1e-6f) neg++;
        else ok = 0;
    }
    VERIFIER(ok, "tous les echantillons valent +A ou -A");
    VERIFIER(pos > 0 && neg > 0, "le signal alterne entre +A et -A");
    oscillateur_detruire(o);
}

/* Amplitude 0.0 : que des zeros, pour les 4 formes d'onde */
static void test_amplitude_zero(void)
{
    printf("test_amplitude_zero\n");
    for (onde_type_t t = ONDE_SINE; t <= ONDE_SAW; t++) {
        oscillateur_t *o = oscillateur_creer(t, 440.0f, 0.0f, 44100);
        float buf[256];
        oscillateur_generer(o, buf, 256);
        int ok = 1;
        for (int i = 0; i < 256; i++)
            if (buf[i] != 0.0f) ok = 0;
        VERIFIER(ok, "buffer rempli de zeros");
        oscillateur_detruire(o);
    }
}

/* Tous les echantillons dans [-A, +A] */
static void test_bornes(void)
{
    printf("test_bornes\n");
    const float A = 0.5f;
    for (onde_type_t t = ONDE_SINE; t <= ONDE_SAW; t++) {
        oscillateur_t *o = oscillateur_creer(t, 1234.5f, A, 44100);
        enum { N = 5000 };
        static float buf[N];
        oscillateur_generer(o, buf, N);
        int ok = 1;
        for (int i = 0; i < N; i++)
            if (buf[i] < -A - 1e-6f || buf[i] > A + 1e-6f) ok = 0;
        VERIFIER(ok, "echantillons dans [-A, +A]");
        oscillateur_detruire(o);
    }
}

/* Deux appels successifs == un seul appel de taille double */
static void test_continuite(void)
{
    printf("test_continuite\n");
    for (onde_type_t t = ONDE_SINE; t <= ONDE_SAW; t++) {
        oscillateur_t *a = oscillateur_creer(t, 440.0f, 1.0f, 44100);
        oscillateur_t *b = oscillateur_creer(t, 440.0f, 1.0f, 44100);
        enum { N = 137 }; /* taille volontairement non alignee sur la periode */
        float deux[2 * N], un[2 * N];

        oscillateur_generer(a, deux, N);
        oscillateur_generer(a, deux + N, N);
        oscillateur_generer(b, un, 2 * N);

        int ok = 1;
        for (int i = 0; i < 2 * N; i++)
            if (fabsf(deux[i] - un[i]) > 1e-4f) ok = 0;
        VERIFIER(ok, "deux appels == un appel double (pas de discontinuite)");
        oscillateur_detruire(a);
        oscillateur_detruire(b);
    }
}

/* Accesseurs, setters, reset de phase */
static void test_accesseurs(void)
{
    printf("test_accesseurs\n");
    oscillateur_t *o = oscillateur_creer(ONDE_TRI, 220.0f, 1.0f, 44100);
    VERIFIER(oscillateur_type(o) == ONDE_TRI, "type initial");
    VERIFIER(oscillateur_frequence(o) == 220.0f, "frequence initiale");

    oscillateur_set_type(o, ONDE_SAW);
    oscillateur_set_frequence(o, 880.0f);
    VERIFIER(oscillateur_type(o) == ONDE_SAW, "set_type");
    VERIFIER(oscillateur_frequence(o) == 880.0f, "set_frequence");

    float a[64], b[64];
    oscillateur_generer(o, a, 64);
    oscillateur_reset_phase(o);
    oscillateur_generer(o, b, 64);
    int ok = 1;
    for (int i = 0; i < 64; i++)
        if (fabsf(a[i] - b[i]) > 1e-6f) ok = 0;
    VERIFIER(ok, "reset_phase redemarre au meme point");

    VERIFIER(oscillateur_creer(ONDE_SINE, 440.0f, 1.0f, 0) == NULL, "taux invalide -> NULL");
    oscillateur_detruire(o);
    oscillateur_detruire(NULL);
}

int main(void)
{
    test_periode_sine();
    test_square();
    test_amplitude_zero();
    test_bornes();
    test_continuite();
    test_accesseurs();

    printf("\n%s (%d echec(s))\n", echecs ? "TESTS ECHOUES" : "TOUS LES TESTS PASSENT", echecs);
    return echecs ? 1 : 0;
}