#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define PORT 5000

typedef struct {
    uint32_t tick;
    uint32_t ordre;   /* ordre d'apparition (tri stable) */
    int type;         /* 0 = tempo, 1 = OFF, 2 = ON */
    int note;
    uint32_t tempo;   /* microsecondes par noire (type 0) */
    double t_us;      /* instant absolu en microsecondes */
} evt_t;

typedef struct { evt_t *v; size_t n, cap; } liste_t;

/* ---------- Réseau ---------- */

static void envoyer(int fd, const char *cmd, int note)
{
    char msg[32];
    int n = snprintf(msg, sizeof msg, "%s %d\n", cmd, note);
    send(fd, msg, (size_t)n, 0);
}

/* ---------- Partition texte ---------- */

/* Convention MIDI : C4 = 60 */
static int nom_vers_midi(const char *s)
{
    static const int base[7] = {9, 11, 0, 2, 4, 5, 7}; /* A B C D E F G */
    if (s[0] < 'A' || s[0] > 'G') return -1;
    int semi = base[s[0] - 'A'], i = 1;
    if (s[i] == '#') { semi++; i++; }
    else if (s[i] == 'b') { semi--; i++; }
    if (s[i] < '0' || s[i] > '9') return -1;
    int note = 12 * (atoi(s + i) + 1) + semi;
    return (note >= 0 && note < 128) ? note : -1;
}

static void jouer_texte(int fd, FILE *f)
{
    char ligne[128], nom[16];
    int duree;
    while (fgets(ligne, sizeof ligne, f)) {
        if (ligne[0] == '#' || sscanf(ligne, "%15s %d", nom, &duree) != 2) continue;
        if (duree < 0) continue;

        if (strcmp(nom, "REST") == 0) {
            usleep((useconds_t)duree * 1000);
            continue;
        }
        int note = nom_vers_midi(nom);
        if (note < 0) {
            fprintf(stderr, "note invalide: %s\n", nom);
            continue;
        }
        envoyer(fd, "ON", note);
        usleep((useconds_t)duree * 1000);
        envoyer(fd, "OFF", note);
    }
}

/* ---------- Fichier MIDI ---------- */

static int ajouter(liste_t *l, evt_t e)
{
    if (l->n == l->cap) {
        size_t c = l->cap ? l->cap * 2 : 1024;
        evt_t *p = realloc(l->v, c * sizeof *p);
        if (!p) return -1;
        l->v = p;
        l->cap = c;
    }
    e.ordre = (uint32_t)l->n;
    l->v[l->n++] = e;
    return 0;
}

static uint32_t lire_be(const uint8_t *p, int n)
{
    uint32_t v = 0;
    while (n--) v = (v << 8) | *p++;
    return v;
}

/* Entier à longueur variable (7 bits par octet) */
static int lire_vlq(const uint8_t *d, size_t fin, size_t *pos, uint32_t *out)
{
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) {
        if (*pos >= fin) return -1;
        uint8_t b = d[(*pos)++];
        v = (v << 7) | (b & 0x7F);
        if (!(b & 0x80)) { *out = v; return 0; }
    }
    return -1;
}

static int lire_piste(const uint8_t *d, size_t debut, size_t fin, liste_t *l)
{
    size_t pos = debut;
    uint32_t tick = 0;
    int statut = 0;                       /* running status */

    while (pos < fin) {
        uint32_t delta;
        if (lire_vlq(d, fin, &pos, &delta) != 0) return -1;
        tick += delta;
        if (pos >= fin) return -1;

        int s;
        if (d[pos] & 0x80) {
            s = d[pos++];
            if (s < 0xF0) statut = s;
            else if (s != 0xFF) statut = 0;
        } else {
            s = statut;
            if (s == 0) return -1;
        }

        if (s == 0xFF) {                  /* meta-événement */
            if (pos >= fin) return -1;
            int type = d[pos++];
            uint32_t len;
            if (lire_vlq(d, fin, &pos, &len) != 0 || pos + len > fin) return -1;
            if (type == 0x51 && len == 3) {
                evt_t e = { .tick = tick, .type = 0, .tempo = lire_be(d + pos, 3) };
                if (ajouter(l, e) != 0) return -1;
            }
            pos += len;
            if (type == 0x2F) break;      /* fin de piste */
        } else if (s == 0xF0 || s == 0xF7) {   /* sysex : ignoré */
            uint32_t len;
            if (lire_vlq(d, fin, &pos, &len) != 0 || pos + len > fin) return -1;
            pos += len;
        } else {                          /* événement de canal */
            int cmd = s & 0xF0;
            int nb = (cmd == 0xC0 || cmd == 0xD0) ? 1 : 2;
            if ((s & 0x0F) == 9) { 
                pos += (size_t)nb; 
                continue; 
            }
            if (pos + (size_t)nb > fin) return -1;
            int n1 = d[pos];
            int n2 = (nb == 2) ? d[pos + 1] : 0;
            pos += (size_t)nb;

            if (cmd == 0x90 && n2 > 0) {
                evt_t e = { .tick = tick, .type = 2, .note = n1 };
                if (ajouter(l, e) != 0) return -1;
            } else if (cmd == 0x80 || cmd == 0x90) {   /* 0x90 vélocité 0 = OFF */
                evt_t e = { .tick = tick, .type = 1, .note = n1 };
                if (ajouter(l, e) != 0) return -1;
            }
        }
    }
    return 0;
}

static int comparer(const void *a, const void *b)
{
    const evt_t *x = a, *y = b;
    if (x->tick != y->tick) return x->tick < y->tick ? -1 : 1;
    if (x->type != y->type) return x->type - y->type;   /* tempo, OFF, puis ON */
    return (x->ordre > y->ordre) - (x->ordre < y->ordre);
}

static int charger_midi(const char *chemin, liste_t *l)
{
    FILE *f = fopen(chemin, "rb");
    if (!f) { perror("midi"); return -1; }
    fseek(f, 0, SEEK_END);
    long t = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (t < 14) { fclose(f); fprintf(stderr, "fichier trop court\n"); return -1; }

    size_t taille = (size_t)t;
    uint8_t *d = malloc(taille);
    if (!d || fread(d, 1, taille, f) != taille) {
        fclose(f);
        free(d);
        return -1;
    }
    fclose(f);

    int ok = -1;
    if (memcmp(d, "MThd", 4) != 0) {
        fprintf(stderr, "pas un fichier MIDI\n");
    } else if (lire_be(d + 8, 2) > 1) {
        fprintf(stderr, "format MIDI 2 non supporte\n");
    } else if ((lire_be(d + 12, 2) & 0x8000) || lire_be(d + 12, 2) == 0) {
        fprintf(stderr, "division SMPTE non supportee\n");
    } else {
        uint32_t entete = lire_be(d + 4, 4);
        int ntrks = (int)lire_be(d + 10, 2);
        uint32_t division = lire_be(d + 12, 2);
        size_t pos = 8 + (size_t)entete;

        for (int i = 0; i < ntrks && pos + 8 <= taille; i++) {
            uint32_t len = lire_be(d + pos + 4, 4);
            size_t debut = pos + 8;
            size_t fin = debut + len;
            if (fin > taille) fin = taille;
            if (memcmp(d + pos, "MTrk", 4) == 0 && lire_piste(d, debut, fin, l) != 0)
                fprintf(stderr, "piste %d : lecture incomplete\n", i);
            pos = debut + len;
        }

        qsort(l->v, l->n, sizeof *l->v, comparer);

        uint32_t tempo = 500000, dernier = 0;   /* 120 BPM par défaut */
        double temps = 0.0;
        for (size_t i = 0; i < l->n; i++) {
            temps += (double)(l->v[i].tick - dernier) * tempo / division;
            dernier = l->v[i].tick;
            l->v[i].t_us = temps;
            if (l->v[i].type == 0) tempo = l->v[i].tempo;
        }
        ok = 0;
    }
    free(d);
    return ok;
}

static void attendre_jusqua(const struct timespec *debut, double t_us)
{
    long long ns = (long long)(t_us * 1000.0);
    struct timespec cible = *debut;
    cible.tv_sec += (time_t)(ns / 1000000000LL);
    cible.tv_nsec += (long)(ns % 1000000000LL);
    if (cible.tv_nsec >= 1000000000L) {
        cible.tv_sec++;
        cible.tv_nsec -= 1000000000L;
    }
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &cible, NULL);
}

static void jouer_midi(int fd, const liste_t *l)
{
    int actives[128] = {0};
    struct timespec debut;
    clock_gettime(CLOCK_MONOTONIC, &debut);

    for (size_t i = 0; i < l->n; i++) {
        const evt_t *e = &l->v[i];
        if (e->type == 0) continue;
        attendre_jusqua(&debut, e->t_us);
        if (e->type == 2) {
            envoyer(fd, "ON", e->note);
            actives[e->note]++;
        } else {
            envoyer(fd, "OFF", e->note);
            if (actives[e->note] > 0) actives[e->note]--;
        }
    }
    for (int n = 0; n < 128; n++)       /* notes restées tenues à la fin */
        if (actives[n] > 0) envoyer(fd, "OFF", n);
}

static int est_midi(const char *s)
{
    size_t n = strlen(s);
    return (n >= 4 && strcasecmp(s + n - 4, ".mid") == 0) ||
           (n >= 5 && strcasecmp(s + n - 5, ".midi") == 0);
}

/* ---------- main ---------- */

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s partition.txt|fichier.mid\n", argv[0]);
        return 1;
    }

    int midi = est_midi(argv[1]);
    liste_t liste = {0};
    FILE *f = NULL;

    if (midi) {
        if (charger_midi(argv[1], &liste) != 0) return 1;
        if (liste.n == 0) { fprintf(stderr, "aucun evenement\n"); return 1; }
    } else {
        f = fopen(argv[1], "r");
        if (!f) { perror("partition"); return 1; }
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int un = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &un, sizeof un);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    if (connect(fd, (struct sockaddr *)&a, sizeof a) != 0) {
        perror("connect (le synthe est-il lance ?)");
        if (f) fclose(f);
        free(liste.v);
        return 1;
    }

    if (midi) jouer_midi(fd, &liste);
    else jouer_texte(fd, f);

    close(fd);
    if (f) fclose(f);
    free(liste.v);
    return 0;
}