#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 5000

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

static void envoyer(int fd, const char *cmd, int note)
{
    char msg[32];
    int n = snprintf(msg, sizeof msg, "%s %d\n", cmd, note);
    send(fd, msg, (size_t)n, 0);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s partition.txt\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("partition"); return 1; }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    if (connect(fd, (struct sockaddr *)&a, sizeof a) != 0) {
        perror("connect (le synthe est-il lance ?)");
        fclose(f);
        return 1;
    }

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

    close(fd);
    fclose(f);
    return 0;
}