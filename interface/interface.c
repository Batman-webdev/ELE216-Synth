#include "interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_ttf.h>
#include "../oscillateur/oscillateur.h"
#include "../filtre/filtre.h"
#include "../enveloppe/enveloppe.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#define PIANO_X 50
#define PIANO_Y 420
#define PIANO_LARGEUR 900
#define PIANO_HAUTEUR 180
#define NB_TOUCHES_BLANCHES 14
#define MAX_VOIX 8

#define M_PI 3.14159265358979323846

/* Notes MIDI des touches blanches : Do4 à Si5 */
static const int notes_blanches[NB_TOUCHES_BLANCHES] = {
    60, 62, 64, 65, 67, 69, 71,
    72, 74, 76, 77, 79, 81, 83
};

/* Notes MIDI des touches noires */
static const int notes_noires[10] = {
    61, 63, 66, 68, 70,
    73, 75, 78, 80, 82
};

/* Position de chaque touche noire par rapport aux touches blanches */
static const int position_noires[10] = {
    0, 1, 3, 4, 5,
    7, 8, 10, 11, 12
};

/* Onde de couleur (effet 4) qui part d'une touche vers les deux cotes */
#define MAX_VAGUES 16
#define VAGUE_VITESSE 24.0f   /* positions de touche par seconde */
#define VAGUE_TRAINEE 0.45f   /* duree (s) du fondu de chaque touche apres le passage de l'onde */
#define FONDU_DUREE   0.60f   /* duree (s) du fondu de couleur de l'effet 3 */
#define NB_POSITIONS 28       /* 14 blanches + emplacements des noires */

typedef struct {
    bool active;
    float origine;   /* position de la touche de depart (0..27) */
    float t0;        /* instant de depart en secondes */
    float teinte;    /* couleur de l'onde (degres) */
} vague_t;

/*
 * Palette des 12 notes (teinte en degres), de Do a Si.
 * Teintes choisies a la main plutot qu'un simple pas de 30 degres,
 * pour eviter les verts/jaunes criards.
 */
static const float palette_teintes[12] = {
    350.0f, 10.0f, 28.0f, 42.0f, 160.0f, 175.0f,
    190.0f, 205.0f, 222.0f, 255.0f, 280.0f, 315.0f
};

static int t_input = 0;

typedef struct {
    bool active;
    bool released;
    int note_midi;
    float frequence_hz;
    float temps;
    float temps_release;
    float duree_note_s;
    oscillateur_t *osc;
} voix_t;

struct interface
{
    SDL_Window *fenetre;
    SDL_Renderer *rendu;
    SDL_AudioDeviceID audio;
    SDL_AudioSpec audio_spec;
    bool etat_sdl;

    oscillateur_t *osc;
    filtre_t *flt;
    enveloppe_t *env;

    float attack_ms;
    float decay_ms;
    float sustain;
    float release_ms;

    int slider_actif;

    TTF_Font *police;
    bool etat_ttf;

    int onde_selectionnee;

    float frequence_coupure_hz;
    bool dial_actif;

    int octave;          /* decalage d'octave, de OCTAVE_MIN a OCTAVE_MAX (0 = normal) */
    float volume;        /* volume general, 0.0 a 1.0 */
    bool volume_actif;   /* bouton de volume en cours de deplacement */

    float temp_voice_buffer[2048];
    bool note_active[128];
    voix_t voix[MAX_VOIX];
    int srv_fd, cli_fd;
    char rx[256];
    int rx_len;

    bool fullscreen;

    /* Effet du piano : 1 = normal, 2 a 4 = effets de couleur */
    int effet_piano;
    vague_t vagues[MAX_VAGUES];

    /* Effet 3 : fondu de la teinte du piano d'une couleur a l'autre */
    float teinte_depart;
    float teinte_cible;
    float t_changement;
};

#define NB_EFFETS 4

static void audio_callback(void *userdata, Uint8 *stream, int len);
static void piano_dessiner(interface_t *ui, SDL_Renderer *rendu);
static int piano_note_clique(int x, int y);
static int touche_note_clavier(SDL_Keycode touche);
static void piano_jouer_test(interface_t *ui, int note_midi);
static void piano_relacher_note(interface_t *ui, int note_midi);
static void vague_ajouter(interface_t *ui, int note_midi);
static void vagues_mettre_a_jour(interface_t *ui, float t);
static float effet3_teinte(interface_t *ui, float t);
static void voice_ajouter_note(interface_t *ui, int note_midi);
static voix_t *voice_disponible(interface_t *ui);
static void dessiner_icone_sinus(SDL_Renderer *rendu, SDL_Rect zone);
static void dessiner_icone_square(SDL_Renderer *rendu, SDL_Rect zone);
static void dessiner_icone_triangle(SDL_Renderer *rendu, SDL_Rect zone);
static void dessiner_icone_saw(SDL_Renderer *rendu, SDL_Rect zone);
static void dessiner_slider_vertical(SDL_Renderer *rendu, int x, int y, int hauteur, float valeur, float minimum, float maximum);
static void dessiner_texte(SDL_Renderer *rendu, TTF_Font *police, const char *texte, int x, int y);
static void dessiner_cercle_rempli(SDL_Renderer *rendu, int cx, int cy, int rayon);
static void dessiner_texte_centre(SDL_Renderer *rendu, TTF_Font *police, const char *texte, SDL_Rect zone);
static void dessiner_fleche(SDL_Renderer *rendu, SDL_Rect zone, bool vers_haut, bool actif);
static void octave_changer(interface_t *ui, int delta);

/* Selecteur d'octave */
#define OCTAVE_MIN -3
#define OCTAVE_MAX 3
static SDL_Rect bouton_effet_rect(int i);
static void dessiner_panneau_effets(interface_t *ui);
static SDL_Color hsv_vers_rgb(float h, float s, float v);
static SDL_Color piano_couleur_touche(interface_t *ui, int note, int position, bool noire, float t);
static void dessiner_dial(SDL_Renderer *rendu, int centre_x, int centre_y, int rayon, float valeur);
static void dessiner_bouton_rotatif(SDL_Renderer *rendu, int centre_x, int centre_y, int rayon, float ratio);
static float bouton_rotatif_ratio(int centre_x, int centre_y, int souris_x, int souris_y);

/* Bouton de volume */
#define VOLUME_CX 110
#define VOLUME_CY 138
#define VOLUME_RAYON 28
static void reseau_traiter(interface_t *ui);
static void dessiner_courbe_adsr(SDL_Renderer *rendu, SDL_Rect zone, float attack, float decay, float sustain, float release);

enum {
    SLIDER_ATTACK,
    SLIDER_DECAY,
    SLIDER_SUSTAIN,
    SLIDER_RELEASE
};

interface_t *interface_creer(oscillateur_t *osc, filtre_t *flt, enveloppe_t *env){
    if(osc == NULL || flt == NULL || env == NULL) return NULL;

    interface_t *ui = malloc(sizeof(interface_t));
    if(ui == NULL) return NULL;

    ui->srv_fd = -1;
    ui->cli_fd = -1;
    ui->rx_len = 0;
    ui->fenetre = NULL;
    ui->rendu = NULL;
    ui->audio = 0;
    ui->etat_sdl = false;

    for (int i = 0; i < 128; ++i) {
        ui->note_active[i] = false;
    }

    for (int i = 0; i < MAX_VOIX; ++i) {
        ui->voix[i].active = false;
        ui->voix[i].released = false;
        ui->voix[i].note_midi = -1;
        ui->voix[i].frequence_hz = 440.0f;
        ui->voix[i].temps = 0.0f;
        ui->voix[i].temps_release = 0.0f;
        ui->voix[i].duree_note_s = 0.300f;
        ui->voix[i].osc = oscillateur_creer(ONDE_SINE, 440.0f, 0.7f, 44100);
        if (ui->voix[i].osc == NULL) {
            for (int j = 0; j < i; ++j) {
                oscillateur_detruire(ui->voix[j].osc);
            }
            free(ui);
            return NULL;
        }
    }

    ui->osc = osc;
    ui->flt = flt;
    ui->env = env;

    ui->attack_ms = enveloppe_attack(ui->env) * 1000.0f;
    ui->decay_ms = enveloppe_decay(ui->env) * 1000.0f;
    ui->sustain = enveloppe_sustain(ui->env);
    ui->release_ms = enveloppe_release(ui->env) * 1000.0f;

    ui->slider_actif = -1;

    ui->police = NULL;
    ui->etat_ttf = false;

    ui->onde_selectionnee = ONDE_SINE;

    ui->frequence_coupure_hz = filtre_coupure(ui->flt);
    ui->dial_actif = false;

    ui->volume = 0.8f;
    ui->volume_actif = false;
    ui->octave = 0;

    ui->effet_piano = 1;
    ui->teinte_depart = palette_teintes[0];
    ui->teinte_cible = palette_teintes[0];
    ui->t_changement = -10.0f;
    for (int i = 0; i < MAX_VAGUES; ++i) {
        ui->vagues[i].active = false;
    }

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0){
        free(ui);
        return NULL;
    }

    ui->etat_sdl = true;

    if (TTF_Init() != 0) {
    interface_detruire(ui);
    return NULL;
    }

    ui->etat_ttf = true;

    ui->police = TTF_OpenFont(
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        18
    );

    if (ui->police == NULL) {
        interface_detruire(ui);
        return NULL;
    }

    // FENETRE
    ui->fenetre = SDL_CreateWindow("Synthetiseur", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1000, 600, SDL_WINDOW_SHOWN);
    if(ui->fenetre == NULL){
        interface_detruire(ui);
        return NULL;
    }

    ui->rendu = SDL_CreateRenderer(ui->fenetre, -1, SDL_RENDERER_ACCELERATED);
    if(ui->rendu == NULL){
        interface_detruire(ui);
        return NULL;
    }

    // Garder un systeme de coordonnees logique de 1000 x 600
    if (SDL_RenderSetLogicalSize(ui->rendu, 1000, 600) != 0) {
        interface_detruire(ui);
        return NULL;
    }

    ui->fullscreen = false;

    // AUDIO
    SDL_AudioSpec spec;
    SDL_zero(spec);

    spec.freq = 44100;
    spec.format = AUDIO_F32SYS;
    spec.channels = 1;
    spec.samples = 512;
    spec.callback = audio_callback;
    spec.userdata = ui;

    ui->audio = SDL_OpenAudioDevice(NULL, 0, &spec, &ui->audio_spec, 0);
    printf("audio: %d Hz, %d samples\n", ui->audio_spec.freq, ui->audio_spec.samples);
    if (ui->audio == 0) {
        interface_detruire(ui);
        return NULL;
    }


    SDL_PauseAudioDevice(ui->audio, 0);

    // SOCKET
    ui->srv_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ui->srv_fd >= 0) {
        int opt = 1;
        setsockopt(ui->srv_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
        struct sockaddr_in a = {0};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        a.sin_port = htons(5000);
        if (bind(ui->srv_fd, (struct sockaddr *)&a, sizeof a) != 0 ||
            listen(ui->srv_fd, 1) != 0) {
            perror("reseau");
            close(ui->srv_fd);
            ui->srv_fd = -1;
        } else {
            fcntl(ui->srv_fd, F_SETFL, O_NONBLOCK);
        }
    }

    return ui;

}

void interface_detruire(interface_t *ui){
    if(ui == NULL) return;
    if(ui->audio != 0) SDL_CloseAudioDevice(ui->audio);
    for (int i = 0; i < MAX_VOIX; ++i) {
        if (ui->voix[i].osc != NULL) {
            oscillateur_detruire(ui->voix[i].osc);
        }
    }
    
    if(ui->rendu != NULL) SDL_DestroyRenderer(ui->rendu);
    if(ui->fenetre != NULL) SDL_DestroyWindow(ui->fenetre);
    if (ui->police != NULL) TTF_CloseFont(ui->police);
    if (ui->etat_ttf) TTF_Quit();
    if(ui->etat_sdl) SDL_Quit();
    if(ui->srv_fd >= 0) close(ui->srv_fd);
    if(ui->cli_fd >= 0) close(ui->cli_fd);
    free(ui);
}


void interface_boucle(interface_t *ui){

    if (ui == NULL || ui->rendu == NULL) return;

    bool continuer = true;

    SDL_Rect zone_volume        = {50, 50, 180, 135};
    SDL_Rect zone_octave        = {245, 50, 105, 135};
    SDL_Rect octave_haut        = {272, 98, 50, 24};     /* fleche vers le haut */
    SDL_Rect octave_valeur      = {245, 124, 105, 26};   /* chiffre de l'octave */
    SDL_Rect octave_bas         = {272, 152, 50, 24};    /* fleche vers le bas */
    SDL_Rect zone_oscillateur   = {50, 200, 300, 175};
    SDL_Rect zone_adsr          = {380, 50, 270, 325};
    SDL_Rect zone_filtre        = {680, 50, 270, 250};

    SDL_Rect zone_sine          = {70, 270, 60, 60};
    SDL_Rect zone_square        = {140, 270, 60, 60};
    SDL_Rect zone_tri           = {210, 270, 60, 60};
    SDL_Rect zone_saw           = {280, 270, 60, 60};

    SDL_Rect icone_sine          = {80, 280, 40, 40};
    SDL_Rect icone_square        = {150, 280, 40, 40};
    SDL_Rect icone_tri           = {220, 280, 40, 40};
    SDL_Rect icone_saw           = {290, 280, 40, 40};

    SDL_Rect zone_graph          = {400, 100, 240, 90};

    int note_clique_active = -1;

    while(continuer){
        reseau_traiter(ui);
        SDL_Event evenement;

        while(SDL_PollEvent(&evenement)){
            if(evenement.type == SDL_QUIT) continuer = false;

            else if(evenement.type == SDL_MOUSEBUTTONDOWN){
                int x = evenement.button.x;
                int y = evenement.button.y;

                SDL_Point souris = {x,y};

                if(SDL_PointInRect(&souris, &zone_oscillateur)){
                    //printf("Oscillateur appuye\n");
                    if(SDL_PointInRect(&souris, &zone_sine)){
                        printf("Sine appuye\n");
                        ui->onde_selectionnee = ONDE_SINE;
                        oscillateur_set_type(ui->osc, ONDE_SINE);
                    }
                    if(SDL_PointInRect(&souris, &zone_square)){
                        printf("Square appuye\n");
                        ui->onde_selectionnee = ONDE_SQUARE;
                        oscillateur_set_type(ui->osc, ONDE_SQUARE);
                    }
                    if(SDL_PointInRect(&souris, &zone_tri)){
                        printf("Tri appuye\n");
                        ui->onde_selectionnee = ONDE_TRI;
                        oscillateur_set_type(ui->osc, ONDE_TRI);
                    }
                    if(SDL_PointInRect(&souris, &zone_saw)){
                        printf("SAW appuye\n");
                        ui->onde_selectionnee = ONDE_SAW;
                        oscillateur_set_type(ui->osc, ONDE_SAW);
                    }
                }
                if(SDL_PointInRect(&souris, &zone_filtre)) printf("Filtre appuye\n");

                // Boutons d'effets du piano
                for (int i = 0; i < NB_EFFETS; ++i) {
                    SDL_Rect bouton = bouton_effet_rect(i);
                    if (SDL_PointInRect(&souris, &bouton)) {
                        ui->effet_piano = i + 1;
                        printf("Effet piano : %d\n", ui->effet_piano);
                    }
                }
                if(SDL_PointInRect(&souris, &zone_adsr)){
                    //printf("ASDR appuye\n");

                }

                int note = piano_note_clique(x,y);
                if(note != -1) {
                    //printf("Note MIDI jouee : %d\n", note);
                    note_clique_active = note;
                    piano_jouer_test(ui, note);
                }

                if (y >= 100 && y <= 330)
                {
                    if (x >= 415 && x <= 445)
                        ui->slider_actif = SLIDER_ATTACK;

                    else if (x >= 475 && x <= 505)
                        ui->slider_actif = SLIDER_DECAY;

                    else if (x >= 535 && x <= 565)
                        ui->slider_actif = SLIDER_SUSTAIN;

                    else if (x >= 595 && x <= 625)
                        ui->slider_actif = SLIDER_RELEASE;
                }

                // Fleches d'octave
                if (SDL_PointInRect(&souris, &octave_haut)) octave_changer(ui, +1);
                if (SDL_PointInRect(&souris, &octave_bas))  octave_changer(ui, -1);

                // Selection du bouton de volume
                {
                    int dx = x - VOLUME_CX;
                    int dy = y - VOLUME_CY;
                    int r = VOLUME_RAYON + 8;
                    if (dx * dx + dy * dy <= r * r) {
                        ui->volume_actif = true;
                    }
                }

                // Selection du dial
                if (x >= 770 && x <= 880 &&
                    y >= 115 && y <= 225) {
                    ui->dial_actif = true;
                }

            }
            else if(evenement.type == SDL_MOUSEBUTTONUP){
                //int x = evenement.button.x;
                //int y = evenement.button.y;
                //printf("Bouton relache a (%d, %d)\n", x, y);

                if (note_clique_active != -1) {
                    piano_relacher_note(ui, note_clique_active);
                    note_clique_active = -1;
                }

                ui->slider_actif = -1;

                ui->dial_actif = false;
                ui->volume_actif = false;
            }
            else if(evenement.type == SDL_KEYDOWN){
                if(evenement.key.repeat == 0){
                    // Raccourcis d'octave : Z = descendre, X = monter
                    if (evenement.key.keysym.sym == SDLK_z) octave_changer(ui, -1);
                    if (evenement.key.keysym.sym == SDLK_x) octave_changer(ui, +1);

                    if (evenement.key.keysym.sym == SDLK_F11)
                    {
                        ui->fullscreen = !ui->fullscreen;

                        if (ui->fullscreen)
                        {
                            SDL_SetWindowFullscreen(
                                ui->fenetre,
                                SDL_WINDOW_FULLSCREEN_DESKTOP
                            );
                        }
                        else
                        {
                            SDL_SetWindowFullscreen(ui->fenetre, 0);
                        }
                    }
                    int note = touche_note_clavier(evenement.key.keysym.sym);
                    if(note != -1){
                        //printf("Touche clavier -> note MIDI %d\n", note);
                        piano_jouer_test(ui, note);
                    }
                }
            }
            else if(evenement.type == SDL_KEYUP){
                if(evenement.key.repeat == 0){
                    int note = touche_note_clavier(evenement.key.keysym.sym);
                    if(note != -1){
                        piano_relacher_note(ui, note);
                    }
                }
            }
            else if(evenement.type == SDL_MOUSEMOTION){
                int y = evenement.motion.y;
                //printf("Souris en mouvement : (%d, %d)\n", x, y);

                if(ui->slider_actif != -1){
                    float ratio = 1.0f - (float)(y - 200) / 125.0f;

                    if (ratio < 0.0f) ratio = 0.0f;
                    if (ratio > 1.0f) ratio = 1.0f;

                    switch (ui->slider_actif)
                    {
                        case SLIDER_ATTACK:
                            ui->attack_ms = ratio * 100.0f;
                            enveloppe_set_attack(ui->env, ui->attack_ms / 1000.0f);
                            printf("Attack: %.1f ms\n", enveloppe_attack(ui->env) * 1000.0f);
                            break;

                        case SLIDER_DECAY:
                            ui->decay_ms = ratio * 1000.0f;
                            enveloppe_set_decay(ui->env, ui->decay_ms / 1000.0f);
                            printf("Decay: %.1f ms\n", enveloppe_decay(ui->env) * 1000.0f);
                            break;

                        case SLIDER_SUSTAIN:
                            ui->sustain = ratio;
                            enveloppe_set_sustain(ui->env, ui->sustain);
                            printf("Sustain: %.1f ms\n", enveloppe_sustain(ui->env));
                            break;

                        case SLIDER_RELEASE:
                            ui->release_ms = ratio * 1000.0f;
                            enveloppe_set_release(ui->env, ui->release_ms / 1000.0f);
                            printf("Release: %.1f ms\n", enveloppe_release(ui->env) * 1000.0f);
                            break;
                    }
                }

                if (ui->volume_actif)
                {
                    ui->volume = bouton_rotatif_ratio(VOLUME_CX, VOLUME_CY,
                                                      evenement.motion.x, evenement.motion.y);
                }

                if (ui->dial_actif)
                {
                    // Centre du dial
                    float centre_x = 825.0f;
                    float centre_y = 170.0f;

                    // Position de la souris par rapport au centre
                    float dx = evenement.motion.x - centre_x;
                    float dy = evenement.motion.y - centre_y;

                    // Calcul de l'angle en degrés
                    float angle = atan2f(dy, dx) * 180.0f / (float)M_PI;

                    // Convertir l'angle entre 0 et 360 degrés
                    if (angle < 0.0f)
                        angle += 360.0f;

                    // Le dial commence à 135 degrés
                    if (angle < 135.0f)
                        angle += 360.0f;

                    // Limiter l'angle entre 135 et 405 degrés
                    if (angle < 135.0f)
                        angle = 135.0f;

                    if (angle > 405.0f)
                        angle = 405.0f;

                    // Convertir l'angle en ratio entre 0 et 1
                    float ratio = (angle - 135.0f) / 270.0f;

                    // Convertir le ratio en fréquence logarithmique
                    ui->frequence_coupure_hz = 500.0f * powf(20.0f, ratio);

                    // Mettre à jour le filtre
                    filtre_set_coupure(ui->flt, ui->frequence_coupure_hz);
                    printf("Frequence du filtre : %.2f Hz\n", ui->frequence_coupure_hz);
                }
            }
        }

        // Redraw interface here
        SDL_SetRenderDrawColor(ui->rendu, 30, 30, 30, 255);
        SDL_RenderClear(ui->rendu);

        // Zone Oscillateur
        SDL_SetRenderDrawColor(ui->rendu, 64, 64, 89, 255);
        SDL_RenderFillRect(ui->rendu, &zone_oscillateur);
        dessiner_texte(ui->rendu, ui->police, "OSCILLATEUR", 70, 215);

        // Zone Volume
        SDL_SetRenderDrawColor(ui->rendu, 64, 64, 89, 255);
        SDL_RenderFillRect(ui->rendu, &zone_volume);
        dessiner_texte(ui->rendu, ui->police, "VOLUME", 70, 70);
        dessiner_bouton_rotatif(ui->rendu, VOLUME_CX, VOLUME_CY, VOLUME_RAYON, ui->volume);
        {
            char texte_volume[8];
            snprintf(texte_volume, sizeof texte_volume, "%d%%", (int)(ui->volume * 100.0f + 0.5f));
            dessiner_texte(ui->rendu, ui->police, texte_volume, 162, 127);
        }

        // Zone Octave
        SDL_SetRenderDrawColor(ui->rendu, 64, 64, 89, 255);
        SDL_RenderFillRect(ui->rendu, &zone_octave);
        dessiner_texte(ui->rendu, ui->police, "OCTAVE", 260, 70);
        dessiner_fleche(ui->rendu, octave_haut, true,  ui->octave < OCTAVE_MAX);
        dessiner_fleche(ui->rendu, octave_bas,  false, ui->octave > OCTAVE_MIN);
        {
            char texte_octave[8];
            snprintf(texte_octave, sizeof texte_octave, "%+d", ui->octave);   /* +0, +1, -1 ... */
            dessiner_texte_centre(ui->rendu, ui->police, texte_octave, octave_valeur);
        }

        // Bouton SINE
        if (ui->onde_selectionnee == ONDE_SINE){
            SDL_SetRenderDrawColor(ui->rendu, 50, 150, 255, 255); // Bleu sélectionné
            SDL_RenderFillRect(ui->rendu, &zone_sine);
            SDL_RenderFillRect(ui->rendu, &icone_sine);
            dessiner_icone_sinus(ui->rendu, icone_sine);
            dessiner_texte(ui->rendu, ui->police, "SINE", 75, 340);
        }
        else {
            SDL_SetRenderDrawColor(ui->rendu, 30, 30, 35, 255);
            SDL_RenderFillRect(ui->rendu, &zone_sine);
            SDL_SetRenderDrawColor(ui->rendu, 30, 30, 35, 255);
            SDL_RenderFillRect(ui->rendu, &icone_sine);
            dessiner_icone_sinus(ui->rendu, icone_sine);
            dessiner_texte(ui->rendu, ui->police, "SINE", 75, 340);
        }
        

        // Bouton SQUARE
        if (ui->onde_selectionnee == ONDE_SQUARE){
            SDL_SetRenderDrawColor(ui->rendu, 50, 150, 255, 255);
            SDL_RenderFillRect(ui->rendu, &zone_square);
            SDL_RenderFillRect(ui->rendu, &icone_square);
            dessiner_icone_square(ui->rendu, icone_square);
            dessiner_texte(ui->rendu, ui->police, "SQUARE", 130, 340);
        }
        else{
            SDL_SetRenderDrawColor(ui->rendu, 30, 30, 35, 255);
            SDL_RenderFillRect(ui->rendu, &zone_square);
            SDL_SetRenderDrawColor(ui->rendu, 30, 30, 35, 255);
            SDL_RenderFillRect(ui->rendu, &icone_square);
            dessiner_icone_square(ui->rendu, icone_square);
            dessiner_texte(ui->rendu, ui->police, "SQUARE", 130, 340);
        }
        

        // Bouton TRIANGLE
        if (ui->onde_selectionnee == ONDE_TRI){
            SDL_SetRenderDrawColor(ui->rendu, 50, 150, 255, 255);
            SDL_RenderFillRect(ui->rendu, &zone_tri);
            SDL_RenderFillRect(ui->rendu, &icone_tri);
            dessiner_icone_triangle(ui->rendu, icone_tri);
            dessiner_texte(ui->rendu, ui->police, "TRI", 225, 340);
        }
        else{
            SDL_SetRenderDrawColor(ui->rendu, 30, 30, 35, 255);
            SDL_RenderFillRect(ui->rendu, &zone_tri);
            SDL_SetRenderDrawColor(ui->rendu, 30, 30, 35, 255);
            SDL_RenderFillRect(ui->rendu, &icone_tri);
            dessiner_icone_triangle(ui->rendu, icone_tri);
            dessiner_texte(ui->rendu, ui->police, "TRI", 225, 340);
        }
        

        // Bouton SAW
        if (ui->onde_selectionnee == ONDE_SAW){
            SDL_SetRenderDrawColor(ui->rendu, 50, 150, 255, 255);
            SDL_RenderFillRect(ui->rendu, &zone_saw);
            SDL_RenderFillRect(ui->rendu, &icone_saw);
            dessiner_icone_saw(ui->rendu, icone_saw);
            dessiner_texte(ui->rendu, ui->police, "SAW", 290, 340);
        }
        else{
            SDL_SetRenderDrawColor(ui->rendu, 30, 30, 35, 255);
            SDL_RenderFillRect(ui->rendu, &zone_saw);
            SDL_SetRenderDrawColor(ui->rendu, 30, 30, 35, 255);
            SDL_RenderFillRect(ui->rendu, &icone_saw);
            dessiner_icone_saw(ui->rendu, icone_saw);
            dessiner_texte(ui->rendu, ui->police, "SAW", 290, 340);
        }


        // Zone Filtre
        SDL_SetRenderDrawColor(ui->rendu, 64, 64, 89, 255);
        SDL_RenderFillRect(ui->rendu, &zone_filtre);
        dessiner_texte(ui->rendu, ui->police, "FILTRE", 700, 70);

        // Dial de fréquence
        dessiner_dial(ui->rendu, 825, 170, 55, ui->frequence_coupure_hz);
        dessiner_texte(ui->rendu, ui->police, "CUTOFF", 790, 240);
        dessiner_texte(ui->rendu, ui->police, "LOW-PASS", 780, 260);

        dessiner_texte(ui->rendu, ui->police, "500", 740, 220);
        dessiner_texte(ui->rendu, ui->police, "800", 720, 160);
        dessiner_texte(ui->rendu, ui->police, "1.2k", 740, 100);
        dessiner_texte(ui->rendu, ui->police, "2k", 810, 80);
        dessiner_texte(ui->rendu, ui->police, "3.2k", 870, 100);
        dessiner_texte(ui->rendu, ui->police, "5k", 900, 160);
        dessiner_texte(ui->rendu, ui->police, "10k", 875, 220);

        // Zone ADSR
        SDL_SetRenderDrawColor(ui->rendu, 64, 64, 89, 255);
        SDL_RenderFillRect(ui->rendu, &zone_adsr);
        dessiner_texte(ui->rendu, ui->police, "ENVELOPPE", 400, 70);

        // Draw sliders
        dessiner_slider_vertical(ui->rendu, 420, 200, 125, ui->attack_ms, 0, 100);
        dessiner_slider_vertical(ui->rendu, 480, 200, 125, ui->decay_ms, 0, 1000);
        dessiner_slider_vertical(ui->rendu, 540, 200, 125, ui->sustain, 0, 1);
        dessiner_slider_vertical(ui->rendu, 600, 200, 125, ui->release_ms, 0, 1000);
        dessiner_texte(ui->rendu, ui->police, "A", 417, 340);
        dessiner_texte(ui->rendu, ui->police, "D", 476, 340);
        dessiner_texte(ui->rendu, ui->police, "S", 538, 340);
        dessiner_texte(ui->rendu, ui->police, "R", 598, 340);

        dessiner_courbe_adsr(ui->rendu, zone_graph, ui->attack_ms, ui->decay_ms, ui->sustain, ui->release_ms);

        dessiner_panneau_effets(ui);

        piano_dessiner(ui, ui->rendu);

        SDL_RenderPresent(ui->rendu);

        SDL_Delay(8);
    }
}

static int touche_note_clavier(SDL_Keycode touche)
{
    switch (touche) {
        case SDLK_a: return 60; /* C4 */
        case SDLK_w: return 61; /* C#4 */
        case SDLK_s: return 62; /* D4 */
        case SDLK_e: return 63; /* D#4 */
        case SDLK_d: return 64; /* E4 */
        case SDLK_f: return 65; /* F4 */
        case SDLK_t: return 66; /* F#4 */
        case SDLK_g: return 67; /* G4 */
        case SDLK_y: return 68; /* G#4 */
        case SDLK_h: return 69; /* A4 */
        case SDLK_u: return 70; /* A#4 */
        case SDLK_j: return 71; /* B4 */
        case SDLK_k: return 72; /* C5 */
        case SDLK_o: return 73; /* C#5 */
        case SDLK_l: return 74; /* D5 */
        default: return -1;
    }
}

static void piano_dessiner(interface_t *ui, SDL_Renderer *rendu)
{
    int largeur_blanche = PIANO_LARGEUR / NB_TOUCHES_BLANCHES;
    int largeur_noire = 38;
    int hauteur_noire = 110;
    float t = (float)SDL_GetTicks() / 1000.0f;
    vagues_mettre_a_jour(ui, t);

    for (int i = 0; i < NB_TOUCHES_BLANCHES; i++)
    {
        int note = notes_blanches[i];
        SDL_Rect touche = {
            PIANO_X + i * largeur_blanche,
            PIANO_Y,
            largeur_blanche,
            PIANO_HAUTEUR
        };

        SDL_Color c = piano_couleur_touche(ui, note, i * 2, false, t);
        SDL_SetRenderDrawColor(rendu, c.r, c.g, c.b, 255);
        SDL_RenderFillRect(rendu, &touche);

        SDL_SetRenderDrawColor(rendu, 20, 20, 20, 255);
        SDL_RenderDrawRect(rendu, &touche);
    }

    for (int i = 0; i < 10; i++)
    {
        int note = notes_noires[i];
        SDL_Rect touche = {
            PIANO_X
                + (position_noires[i] + 1) * largeur_blanche
                - largeur_noire / 2,
            PIANO_Y,
            largeur_noire,
            hauteur_noire
        };

        SDL_Color c = piano_couleur_touche(ui, note, position_noires[i] * 2 + 1, true, t);
        SDL_SetRenderDrawColor(rendu, c.r, c.g, c.b, 255);
        SDL_RenderFillRect(rendu, &touche);
    }
}

/* Conversion HSV -> RGB. h en degres [0, 360), s et v dans [0, 1] */
static SDL_Color hsv_vers_rgb(float h, float s, float v)
{
    h = fmodf(h, 360.0f);
    if (h < 0.0f) h += 360.0f;

    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r = 0.0f, g = 0.0f, b = 0.0f;

    if      (h <  60.0f) { r = c; g = x; }
    else if (h < 120.0f) { r = x; g = c; }
    else if (h < 180.0f) { g = c; b = x; }
    else if (h < 240.0f) { g = x; b = c; }
    else if (h < 300.0f) { r = x; b = c; }
    else                 { r = c; b = x; }

    SDL_Color res = {
        (Uint8)((r + m) * 255.0f),
        (Uint8)((g + m) * 255.0f),
        (Uint8)((b + m) * 255.0f),
        255
    };
    return res;
}

/* Melange lineaire entre deux couleurs (k = 0 -> a, k = 1 -> b) */
static SDL_Color melanger(SDL_Color a, SDL_Color b, float k)
{
    if (k < 0.0f) k = 0.0f;
    if (k > 1.0f) k = 1.0f;
    SDL_Color res = {
        (Uint8)(a.r + (b.r - a.r) * k),
        (Uint8)(a.g + (b.g - a.g) * k),
        (Uint8)(a.b + (b.b - a.b) * k),
        255
    };
    return res;
}

/* Courbe douce 0 -> 1 (debut et fin lents) */
static float lisser(float x)
{
    if (x <= 0.0f) return 0.0f;
    if (x >= 1.0f) return 1.0f;
    return x * x * (3.0f - 2.0f * x);
}

/*
 * Couleur pleine d'une touche pour une teinte donnee.
 * Blanche = couleur vive, noire = meme teinte plus foncee.
 * Une touche enfoncee est legerement plus sombre (blanche) ou plus claire (noire).
 */
static SDL_Color couleur_pleine(float teinte, bool noire, bool appuyee)
{
    const float SATURATION     = 0.85f;
    const float V_BLANCHE      = 0.95f;  /* touche blanche au repos */
    const float V_NOIRE        = 0.42f;  /* touche noire au repos (plus foncee) */
    const float V_BLANCHE_APP  = 0.72f;  /* touche blanche enfoncee */
    const float V_NOIRE_APP    = 0.62f;  /* touche noire enfoncee */

    float v;
    if (noire) v = appuyee ? V_NOIRE_APP : V_NOIRE;
    else       v = appuyee ? V_BLANCHE_APP : V_BLANCHE;
    return hsv_vers_rgb(teinte, SATURATION, v);
}

/* Teinte actuelle de l'effet 3 (fondu de la teinte de depart vers la cible) */
static float effet3_teinte(interface_t *ui, float t)
{
    float k = lisser((t - ui->t_changement) / FONDU_DUREE);

    /* Passer par le chemin le plus court sur le cercle des teintes */
    float delta = fmodf(ui->teinte_cible - ui->teinte_depart + 540.0f, 360.0f) - 180.0f;
    float h = fmodf(ui->teinte_depart + delta * k, 360.0f);
    return (h < 0.0f) ? h + 360.0f : h;
}

/*
 * Couleur d'une touche selon l'effet actif.
 * position : position horizontale de la touche (0..27) pour l'effet d'onde.
 */
static SDL_Color piano_couleur_touche(interface_t *ui, int note, int position, bool noire, float t)
{
    SDL_Color blanc_repos = {245, 245, 245, 255};
    SDL_Color noir_repos  = {25, 25, 25, 255};
    SDL_Color blanc_app   = {170, 170, 170, 255};
    SDL_Color noir_app    = {110, 110, 110, 255};

    bool appuyee = ui->note_active[note];
    SDL_Color repos = noire ? noir_repos : blanc_repos;

    switch (ui->effet_piano)
    {
        case 2: /* Touches jouees : seule la touche enfoncee prend une couleur pleine */
            if (!appuyee) return repos;
            return couleur_pleine(palette_teintes[note % 12], noire, false);

        case 3: /* Couleur : tout le piano change de couleur (en fondu) a chaque note */
            return couleur_pleine(effet3_teinte(ui, t), noire, appuyee);

        case 4: /* Onde : part de la touche jouee vers la gauche et la droite */
        {
            if (appuyee) {
                return couleur_pleine(palette_teintes[note % 12], noire, false);
            }

            /* Chaque touche s'allume quand l'onde l'atteint puis s'estompe */
            float meilleur_k = 0.0f;
            float teinte = 0.0f;
            for (int i = 0; i < MAX_VAGUES; ++i) {
                vague_t *vg = &ui->vagues[i];
                if (!vg->active) continue;
                float distance = fabsf((float)position - vg->origine);
                float depuis = (t - vg->t0) - distance / VAGUE_VITESSE;
                if (depuis < 0.0f) continue;   /* l'onde n'est pas encore arrivee */
                float k = 1.0f - depuis / VAGUE_TRAINEE;
                if (k <= 0.0f) continue;
                k = k * k;                     /* fondu qui ralentit a la fin */
                if (k > meilleur_k) {
                    meilleur_k = k;
                    teinte = vg->teinte;
                }
            }
            if (meilleur_k <= 0.0f) return repos;
            return melanger(repos, couleur_pleine(teinte, noire, false), meilleur_k);
        }

        default: /* 1 : piano classique blanc et noir */
            if (noire) return appuyee ? noir_app : noir_repos;
            return appuyee ? blanc_app : blanc_repos;
    }
}

/* Position des 4 boutons d'effets dans le panneau */
static SDL_Rect bouton_effet_rect(int i)
{
    SDL_Rect r = {691 + i * 64, 325, 56, 40};
    return r;
}

static void dessiner_panneau_effets(interface_t *ui)
{
    SDL_Rect zone_effets = {680, 315, 270, 60};
    SDL_SetRenderDrawColor(ui->rendu, 64, 64, 89, 255);
    SDL_RenderFillRect(ui->rendu, &zone_effets);

    const char *labels[NB_EFFETS] = {"1", "2", "3", "4"};

    for (int i = 0; i < NB_EFFETS; ++i) {
        SDL_Rect bouton = bouton_effet_rect(i);

        if (ui->effet_piano == i + 1) {
            SDL_SetRenderDrawColor(ui->rendu, 50, 150, 255, 255);
        } else {
            SDL_SetRenderDrawColor(ui->rendu, 30, 30, 35, 255);
        }
        SDL_RenderFillRect(ui->rendu, &bouton);

        dessiner_texte_centre(ui->rendu, ui->police, labels[i], bouton);
    }
}

/* Change l'octave en restant entre OCTAVE_MIN et OCTAVE_MAX */
static void octave_changer(interface_t *ui, int delta)
{
    int nouvelle = ui->octave + delta;
    if (nouvelle < OCTAVE_MIN || nouvelle > OCTAVE_MAX) return;
    ui->octave = nouvelle;
    printf("Octave : %+d\n", ui->octave);
}

/*
 * Bouton fleche : fond sombre et triangle plein.
 * Le triangle est gris quand on est deja a la limite (actif = false).
 */
static void dessiner_fleche(SDL_Renderer *rendu, SDL_Rect zone, bool vers_haut, bool actif)
{
    SDL_SetRenderDrawColor(rendu, 30, 30, 35, 255);
    SDL_RenderFillRect(rendu, &zone);

    if (actif) SDL_SetRenderDrawColor(rendu, 255, 255, 255, 255);
    else       SDL_SetRenderDrawColor(rendu, 90, 90, 100, 255);

    int cx = zone.x + zone.w / 2;
    int hauteur = zone.h / 2;                 /* hauteur du triangle */
    int haut = zone.y + (zone.h - hauteur) / 2;

    /* Triangle rempli ligne par ligne : pointe en haut ou en bas */
    for (int i = 0; i <= hauteur; ++i) {
        int demi_largeur = i;                 /* largeur = 2 x hauteur */
        int y = vers_haut ? haut + i : haut + hauteur - i;
        SDL_RenderDrawLine(rendu, cx - demi_largeur, y, cx + demi_largeur, y);
    }
}

static void dessiner_texte_centre(SDL_Renderer *rendu, TTF_Font *police, const char *texte, SDL_Rect zone)
{
    int w = 0, h = 0;
    if (TTF_SizeText(police, texte, &w, &h) != 0) return;
    dessiner_texte(rendu, police, texte, zone.x + (zone.w - w) / 2, zone.y + (zone.h - h) / 2);
}

static int piano_note_clique(int x, int y)
{
    int largeur_blanche = PIANO_LARGEUR / NB_TOUCHES_BLANCHES;
    int largeur_noire = 38;
    int hauteur_noire = 110;

    /* Vérifier d'abord les touches noires */
    for (int i = 0; i < 10; i++)
    {
        SDL_Rect touche = {
            PIANO_X
                + (position_noires[i] + 1) * largeur_blanche
                - largeur_noire / 2,
            PIANO_Y,
            largeur_noire,
            hauteur_noire
        };

        if (SDL_PointInRect(&(SDL_Point){x, y}, &touche))
        {
            return notes_noires[i];
        }
    }

    /* Vérifier ensuite les touches blanches */
    for (int i = 0; i < NB_TOUCHES_BLANCHES; i++)
    {
        SDL_Rect touche = {
            PIANO_X + i * largeur_blanche,
            PIANO_Y,
            largeur_blanche,
            PIANO_HAUTEUR
        };

        if (SDL_PointInRect(&(SDL_Point){x, y}, &touche))
        {
            return notes_blanches[i];
        }
    }

    return -1;
}


static voix_t *voice_disponible(interface_t *ui)
{
    for (int i = 0; i < MAX_VOIX; ++i) {
        if (!ui->voix[i].active) {
            return &ui->voix[i];
        }
    }

    int oldest = 0;
    float oldest_time = ui->voix[0].temps;
    for (int i = 1; i < MAX_VOIX; ++i) {
        if (ui->voix[i].temps > oldest_time) {
            oldest_time = ui->voix[i].temps;
            oldest = i;
        }
    }
    return &ui->voix[oldest];
}

static void piano_jouer_test(interface_t *ui, int note_midi)
{
    if (ui == NULL || note_midi < 0 || note_midi >= 128) return;
    t_input = SDL_GetTicks();
    SDL_LockAudioDevice(ui->audio);
    ui->note_active[note_midi] = true;
    voice_ajouter_note(ui, note_midi);
    SDL_UnlockAudioDevice(ui->audio);

    if (ui->effet_piano == 3) {
        /* Nouveau fondu : on part de la couleur affichee en ce moment */
        float t = (float)SDL_GetTicks() / 1000.0f;
        ui->teinte_depart = effet3_teinte(ui, t);
        ui->teinte_cible = palette_teintes[note_midi % 12];
        ui->t_changement = t;
    }

    if (ui->effet_piano == 4) {
        vague_ajouter(ui, note_midi);
    }
}

/* Position horizontale (0..27) d'une note sur le clavier, -1 si hors clavier */
static int note_position(int note_midi)
{
    for (int i = 0; i < NB_TOUCHES_BLANCHES; ++i) {
        if (notes_blanches[i] == note_midi) return i * 2;
    }
    for (int i = 0; i < 10; ++i) {
        if (notes_noires[i] == note_midi) return position_noires[i] * 2 + 1;
    }
    return -1;
}

/* Lance une onde depuis la touche jouee */
static void vague_ajouter(interface_t *ui, int note_midi)
{
    int position = note_position(note_midi);
    if (position < 0) return;

    /* Prendre un emplacement libre, sinon remplacer la plus ancienne */
    int choix = 0;
    for (int i = 0; i < MAX_VAGUES; ++i) {
        if (!ui->vagues[i].active) { choix = i; break; }
        if (ui->vagues[i].t0 < ui->vagues[choix].t0) choix = i;
    }

    ui->vagues[choix].active = true;
    ui->vagues[choix].origine = (float)position;
    ui->vagues[choix].t0 = (float)SDL_GetTicks() / 1000.0f;
    ui->vagues[choix].teinte = palette_teintes[note_midi % 12];
}

/* Desactive les ondes qui ont depasse les deux bords du clavier */
static void vagues_mettre_a_jour(interface_t *ui, float t)
{
    for (int i = 0; i < MAX_VAGUES; ++i) {
        vague_t *vg = &ui->vagues[i];
        if (!vg->active) continue;
        float front = (t - vg->t0) * VAGUE_VITESSE;
        float distance_max = fmaxf(vg->origine, (float)(NB_POSITIONS - 1) - vg->origine);
        if (front > distance_max + VAGUE_TRAINEE * VAGUE_VITESSE) vg->active = false;
    }
}

static void piano_relacher_note(interface_t *ui, int note_midi)
{
    if (ui == NULL || note_midi < 0 || note_midi >= 128) return;
    SDL_LockAudioDevice(ui->audio);
    ui->note_active[note_midi] = false;
    for (int i = 0; i < MAX_VOIX; ++i) {
        if (ui->voix[i].active && ui->voix[i].note_midi == note_midi && !ui->voix[i].released) {
            ui->voix[i].released = true;
            ui->voix[i].temps_release = ui->voix[i].temps;
            break;
        }
    }
    SDL_UnlockAudioDevice(ui->audio);
}

static void voice_ajouter_note(interface_t *ui, int note_midi)
{
    if (ui == NULL || note_midi < 0 || note_midi >= 128) {
        return;
    }

    for (int i = 0; i < MAX_VOIX; ++i) {
        if (ui->voix[i].active && ui->voix[i].note_midi == note_midi && !ui->voix[i].released) {
            return;
        }
    }

    voix_t *v = voice_disponible(ui);
    if (v == NULL) {
        return;
    }

    const float duree_note = 0.300f;
    /* Le decalage d'octave s'applique au son, pas aux touches affichees */
    int note_jouee = note_midi + 12 * ui->octave;
    float frequence = 440.0f * powf(2.0f, (note_jouee - 69) / 12.0f);

    v->note_midi = note_midi;
    v->frequence_hz = frequence;
    v->duree_note_s = duree_note;
    v->temps = 0.0f;
    v->temps_release = 0.0f;
    v->released = false;
    v->active = true;
    oscillateur_set_type(v->osc, ui->onde_selectionnee);
    oscillateur_set_frequence(v->osc, frequence);
    oscillateur_reset_phase(v->osc);
}

static void dessiner_icone_sinus(SDL_Renderer *rendu, SDL_Rect zone)
{
    SDL_SetRenderDrawColor(rendu, 255, 255, 255, 255);

    int centre_y = zone.y + zone.h / 2;
    int amplitude = zone.h / 3;

    for (int x = 0; x < zone.w - 1; x++)
    {
        float angle1 = 2.0f * 3.14159f * x / zone.w;
        float angle2 = 2.0f * 3.14159f * (x + 1) / zone.w;

        int y1 = centre_y - (int)(amplitude * sinf(angle1));
        int y2 = centre_y - (int)(amplitude * sinf(angle2));

        SDL_RenderDrawLine(rendu,
                           zone.x + x, y1,
                           zone.x + x + 1, y2);
    }
}


static void dessiner_icone_square(SDL_Renderer *rendu, SDL_Rect zone)
{
    SDL_SetRenderDrawColor(rendu, 255, 255, 255, 255);

    //int milieu = zone.y + zone.h / 2;
    int haut = zone.y + 5;
    int bas = zone.y + zone.h - 5;

    int x1 = zone.x;
    int x2 = zone.x + zone.w / 4;
    int x3 = zone.x + zone.w / 2;
    int x4 = zone.x + 3 * zone.w / 4;
    int x5 = zone.x + zone.w;

    SDL_RenderDrawLine(rendu, x1, bas, x1, haut);
    SDL_RenderDrawLine(rendu, x1, haut, x2, haut);
    SDL_RenderDrawLine(rendu, x2, haut, x2, bas);
    SDL_RenderDrawLine(rendu, x2, bas, x3, bas);
    SDL_RenderDrawLine(rendu, x3, bas, x3, haut);
    SDL_RenderDrawLine(rendu, x3, haut, x4, haut);
    SDL_RenderDrawLine(rendu, x4, haut, x4, bas);
    SDL_RenderDrawLine(rendu, x4, bas, x5, bas);
}


static void dessiner_icone_triangle(SDL_Renderer *rendu, SDL_Rect zone)
{
    SDL_SetRenderDrawColor(rendu, 255, 255, 255, 255);

    int milieu = zone.y + zone.h / 2;
    int haut = zone.y + 5;
    int bas = zone.y + zone.h - 5;

    int largeur = zone.w / 2;

    SDL_RenderDrawLine(rendu,
                       zone.x, milieu,
                       zone.x + largeur / 2, haut);

    SDL_RenderDrawLine(rendu,
                       zone.x + largeur / 2, haut,
                       zone.x + largeur, milieu);

    SDL_RenderDrawLine(rendu,
                       zone.x + largeur, milieu,
                       zone.x + 3 * largeur / 2, bas);

    SDL_RenderDrawLine(rendu,
                       zone.x + 3 * largeur / 2, bas,
                       zone.x + 2 * largeur, milieu);
}


static void dessiner_icone_saw(SDL_Renderer *rendu, SDL_Rect zone)
{
    SDL_SetRenderDrawColor(rendu, 255, 255, 255, 255);

    int haut = zone.y + 5;
    int bas = zone.y + zone.h - 5;

    int x1 = zone.x;
    int x2 = zone.x + zone.w / 2;
    int x3 = zone.x + zone.w;

    SDL_RenderDrawLine(rendu, x1, bas, x2, haut);
    SDL_RenderDrawLine(rendu, x2, haut, x2, bas);
    SDL_RenderDrawLine(rendu, x2, bas, x3, haut);
}

static void dessiner_slider_vertical(SDL_Renderer *rendu, int x, int y, int hauteur, float valeur, float minimum, float maximum)
{
    float ratio = (valeur - minimum) / (maximum - minimum);

    // Slider track
    SDL_Rect piste = {x, y, 8, hauteur};

    SDL_SetRenderDrawColor(rendu, 50, 150, 255, 255);
    SDL_RenderFillRect(rendu, &piste);

    // Handle position (bottom = minimum, top = maximum)
    int position_y = y + hauteur - (int)(ratio * hauteur);

    SDL_Rect curseur = {x - 6, position_y - 6, 20, 12};

    SDL_SetRenderDrawColor(rendu, 255, 255, 255, 255);
    SDL_RenderFillRect(rendu, &curseur);
}

static void dessiner_texte(SDL_Renderer *rendu, TTF_Font *police, const char *texte, int x, int y)
{
    typedef struct { char txt[32]; SDL_Texture *tex; int w, h; } cache_t;
    static cache_t cache[64];
    static int n = 0;

    cache_t *c = NULL;
    for (int i = 0; i < n; ++i)
        if (strcmp(cache[i].txt, texte) == 0) { c = &cache[i]; break; }

    if (c == NULL) {
        if (n >= 64) return;
        SDL_Color blanc = {255, 255, 255, 255};
        SDL_Surface *s = TTF_RenderText_Blended(police, texte, blanc);
        if (s == NULL) return;
        c = &cache[n++];
        strncpy(c->txt, texte, sizeof(c->txt) - 1);
        c->txt[sizeof(c->txt) - 1] = '\0';
        c->tex = SDL_CreateTextureFromSurface(rendu, s);
        c->w = s->w; c->h = s->h;
        SDL_FreeSurface(s);
    }
    if (c->tex == NULL) return;
    SDL_Rect dst = {x, y, c->w, c->h};
    SDL_RenderCopy(rendu, c->tex, NULL, &dst);
}

/* Dessiner un cercle rempli */
static void dessiner_cercle_rempli(SDL_Renderer *rendu, int cx, int cy, int rayon)
{
    for (int y = -rayon; y <= rayon; y++) {
        int x = (int)sqrtf((float)(rayon * rayon - y * y));

        SDL_RenderDrawLine(rendu,
                           cx - x, cy + y,
                           cx + x, cy + y);
    }
}

/* Dessiner le dial du filtre */
static void dessiner_dial(SDL_Renderer *rendu, int centre_x, int centre_y, int rayon, float frequence)
{
    const float FREQ_MIN = 500.0f;
    const float FREQ_MAX = 10000.0f;

    /* Limiter la fréquence */
    if (frequence < FREQ_MIN)
        frequence = FREQ_MIN;

    if (frequence > FREQ_MAX)
        frequence = FREQ_MAX;

    /* Convertir la fréquence (echelle logarithmique) en ratio 0..1 */
    float ratio = logf(frequence / FREQ_MIN) /
                  logf(FREQ_MAX / FREQ_MIN);

    dessiner_bouton_rotatif(rendu, centre_x, centre_y, rayon, ratio);
}

/* Dessiner un bouton rotatif (dial) a partir d'un ratio 0..1 */
static void dessiner_bouton_rotatif(SDL_Renderer *rendu, int centre_x, int centre_y, int rayon, float ratio)
{
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    /* Couleurs */
    SDL_Color fond = {36, 42, 53, 255};
    SDL_Color contour = {128, 128, 128, 255};
    SDL_Color graduations = {128, 128, 128, 255};
    SDL_Color orange = {235, 170, 55, 255};

    /* Fond du dial */
    SDL_SetRenderDrawColor(rendu,
                           fond.r, fond.g, fond.b, fond.a);

    dessiner_cercle_rempli(rendu, centre_x, centre_y, rayon);

    /* Contour circulaire */
    SDL_SetRenderDrawColor(rendu,
                           contour.r, contour.g,
                           contour.b, contour.a);

    for (int angle = 0; angle < 360; angle++) {
        float rad = angle * (float)M_PI / 180.0f;

        int x = centre_x + (int)(rayon * cosf(rad));
        int y = centre_y + (int)(rayon * sinf(rad));

        SDL_RenderDrawPoint(rendu, x, y);
    }

    /* Graduations autour du dial */
    SDL_SetRenderDrawColor(rendu,
                           graduations.r, graduations.g,
                           graduations.b, graduations.a);

    for (int angle = 135; angle <= 405; angle += 45) {

        float rad = angle * (float)M_PI / 180.0f;

        int x1 = centre_x + (int)((rayon + 6) * cosf(rad));
        int y1 = centre_y + (int)((rayon + 6) * sinf(rad));

        int x2 = centre_x + (int)((rayon + 13) * cosf(rad));
        int y2 = centre_y + (int)((rayon + 13) * sinf(rad));

        SDL_RenderDrawLine(rendu, x1, y1, x2, y2);
    }

    /* Convertir le ratio en position angulaire */
    float angle = (135.0f + ratio * 270.0f)
                  * (float)M_PI / 180.0f;

    /* Calculer la position de l'indicateur */
    int indicateur_x = centre_x +
                       (int)(rayon * 0.72f * cosf(angle));

    int indicateur_y = centre_y +
                       (int)(rayon * 0.72f * sinf(angle));

    /* Dessiner l'indicateur orange */
    SDL_SetRenderDrawColor(rendu,
                           orange.r, orange.g,
                           orange.b, orange.a);

    SDL_RenderDrawLine(rendu, centre_x, centre_y,
                       indicateur_x, indicateur_y);

    /* Épaissir légèrement l'indicateur */
    SDL_RenderDrawLine(rendu,
                       centre_x - 1, centre_y,
                       indicateur_x - 1, indicateur_y);

    /* Petit cercle orange au centre */
    dessiner_cercle_rempli(rendu, centre_x, centre_y, 4);
}

/*
 * Convertit la position de la souris en ratio 0..1 pour un bouton rotatif
 * qui va de 135 degres (minimum, en bas a gauche) a 405 degres (maximum, en bas a droite).
 */
static float bouton_rotatif_ratio(int centre_x, int centre_y, int souris_x, int souris_y)
{
    float dx = (float)(souris_x - centre_x);
    float dy = (float)(souris_y - centre_y);
    float angle = atan2f(dy, dx) * 180.0f / (float)M_PI;   /* -180..180, 90 = en bas */

    if (angle < 0.0f) angle += 360.0f;                     /* 0..360 */

    /* Zone morte en bas (45..135) : cote gauche = minimum, cote droit = maximum */
    if (angle > 45.0f && angle < 135.0f) {
        return (angle >= 90.0f) ? 0.0f : 1.0f;
    }

    if (angle < 135.0f) angle += 360.0f;                   /* 135..405 */
    return (angle - 135.0f) / 270.0f;
}

static float clamp_audio_sample(float value, float min_value, float max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static void audio_callback(void *userdata, Uint8 *stream, int len)
{
    interface_t *ui = (interface_t *)userdata;
    if (ui == NULL || stream == NULL || len <= 0) {
        return;
    }

    float *buffer = (float *)stream;
    int nb_echantillons = len / (int)sizeof(float);
    if (nb_echantillons > 2048) nb_echantillons = 2048;

    const float taux = (float)ui->audio_spec.freq;
    const float attack_s = enveloppe_attack(ui->env);
    const float decay_s = enveloppe_decay(ui->env);
    const float sustain = enveloppe_sustain(ui->env);
    const float release_s = enveloppe_release(ui->env);
    const float voix_gain = 0.4f;

    memset(buffer, 0, (size_t)len);

    if (t_input != 0) {
        printf("latence code = %d ms\n", (int)SDL_GetTicks() - t_input);
        t_input = 0;
    }

    for (int v = 0; v < MAX_VOIX; ++v) {
        if (!ui->voix[v].active) {
            continue;
        }

        if (ui->voix[v].note_midi >= 0 && ui->voix[v].note_midi < 128 &&
            !ui->note_active[ui->voix[v].note_midi] && !ui->voix[v].released) {
            ui->voix[v].released = true;
            ui->voix[v].temps_release = ui->voix[v].temps;
        }

        float *voice_buffer = ui->temp_voice_buffer;
        oscillateur_generer(ui->voix[v].osc, voice_buffer, nb_echantillons);

        for (int i = 0; i < nb_echantillons; ++i) {
            float temps = ui->voix[v].temps + (float)i / taux;
            float amplitude = 0.0f;

            if (!ui->voix[v].released) {
                if (attack_s > 0.0f && temps < attack_s) {
                    amplitude = temps / attack_s;
                } else if (temps < attack_s + decay_s) {
                    if (decay_s > 0.0f) {
                        amplitude = 1.0f - ((temps - attack_s) / decay_s) * (1.0f - sustain);
                    } else {
                        amplitude = sustain;
                    }
                } else {
                    amplitude = sustain;
                }
            } else {
                float release_elapsed = temps - ui->voix[v].temps_release;
                if (release_s > 0.0f) {
                    amplitude = sustain * (1.0f - (release_elapsed / release_s));
                }
            }

            amplitude = clamp_audio_sample(amplitude, 0.0f, 1.0f);
            buffer[i] += voice_buffer[i] * amplitude * voix_gain;
        }

        ui->voix[v].temps += (float)nb_echantillons / taux;

        if (ui->voix[v].released) {
            float release_elapsed = ui->voix[v].temps - ui->voix[v].temps_release;
            if (release_s <= 0.0f || release_elapsed >= release_s) {
                ui->voix[v].active = false;
                if (ui->voix[v].note_midi >= 0 && ui->voix[v].note_midi < 128) {
                    ui->note_active[ui->voix[v].note_midi] = false;
                }
                ui->voix[v].note_midi = -1;
                ui->voix[v].released = false;
                ui->voix[v].temps_release = 0.0f;
            }
        }
    }

    if (ui->flt != NULL) {
        filtre_appliquer(ui->flt, buffer, nb_echantillons);
    }

    /* Saturation douce puis volume general */
    const float volume = ui->volume;
    for (int i = 0; i < nb_echantillons; ++i) {
        buffer[i] = tanhf(buffer[i]) * volume;
    }
}

static void reseau_traiter(interface_t *ui)
{
    if (ui->srv_fd < 0) return;

    if (ui->cli_fd < 0) {
        ui->cli_fd = accept(ui->srv_fd, NULL, NULL);
        if (ui->cli_fd >= 0) {
            fcntl(ui->cli_fd, F_SETFL, O_NONBLOCK);
            ui->rx_len = 0;
        }
        return;
    }

    if (ui->rx_len >= (int)sizeof ui->rx - 1) ui->rx_len = 0; /* ligne trop longue */

    int n = recv(ui->cli_fd, ui->rx + ui->rx_len,
                 sizeof ui->rx - ui->rx_len - 1, 0);
    if (n == 0) {
        close(ui->cli_fd);
        ui->cli_fd = -1;
        ui->rx_len = 0;
        return;
    }
    if (n < 0) return;

    ui->rx_len += n;
    ui->rx[ui->rx_len] = '\0';

    char *debut = ui->rx, *fin;
    while ((fin = strchr(debut, '\n')) != NULL) {
        *fin = '\0';
        int note;
        if (sscanf(debut, "ON %d", &note) == 1) piano_jouer_test(ui, note);
        else if (sscanf(debut, "OFF %d", &note) == 1) piano_relacher_note(ui, note);
        debut = fin + 1;
    }
    ui->rx_len = (int)strlen(debut);
    memmove(ui->rx, debut, ui->rx_len + 1);
}

static void dessiner_courbe_adsr(SDL_Renderer *rendu,
                          SDL_Rect zone,
                          float attack,
                          float decay,
                          float sustain,
                          float release)
{
    SDL_SetRenderDrawColor(rendu, 255, 255, 255, 255);
    SDL_RenderDrawRect(rendu, &zone);

    // Marges du graphique
    int marge = 15;
    int x0 = zone.x + marge;
    int y0 = zone.y + zone.h - marge;
    int largeur = zone.w - 2 * marge;
    int hauteur = zone.h - 2 * marge;

    // Axe horizontal et vertical
    SDL_SetRenderDrawColor(rendu, 180, 180, 180, 255);
    SDL_RenderDrawLine(rendu, x0, y0 - hauteur, x0, y0);
    SDL_RenderDrawLine(rendu, x0, y0, x0 + largeur, y0);

    // Normaliser les durées pour calculer la largeur de chaque segment
    float total = attack + decay + 300.0f + release;

    if (total <= 0.0f)
        return;

    int largeur_A = (int)(largeur * attack / total);
    int largeur_D = (int)(largeur * decay / total);
    int largeur_S = (int)(largeur * 300.0f / total);
    int largeur_R = largeur - largeur_A - largeur_D - largeur_S;

    // Points de la courbe
    SDL_Point points[5];

    points[0].x = x0;
    points[0].y = y0;

    points[1].x = x0 + largeur_A;
    points[1].y = y0 - hauteur;

    points[2].x = points[1].x + largeur_D;
    points[2].y = y0 - (int)(hauteur * sustain);

    points[3].x = points[2].x + largeur_S;
    points[3].y = points[2].y;

    points[4].x = points[3].x + largeur_R;
    points[4].y = y0;

    // Dessiner la courbe
    SDL_SetRenderDrawColor(rendu, 0, 220, 120, 255);
    SDL_RenderDrawLines(rendu, points, 5);
}
