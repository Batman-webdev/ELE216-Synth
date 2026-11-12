#include "interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_ttf.h>
#include "../oscillateur/oscillateur.h"
#include "../filtre/filtre.h"
#include "../enveloppe/enveloppe.h"

#define PIANO_X 50
#define PIANO_Y 350
#define PIANO_LARGEUR 900
#define PIANO_HAUTEUR 180
#define NB_TOUCHES_BLANCHES 14

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

struct interface
{
    SDL_Window *fenetre;
    SDL_Renderer *rendu;
    SDL_AudioDeviceID audio;
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
};

static void piano_dessiner(SDL_Renderer *rendu);
static int piano_note_clique(int x, int y);
static void piano_jouer_test(interface_t *ui, int note_midi);
static void dessiner_icone_sinus(SDL_Renderer *rendu, SDL_Rect zone);
static void dessiner_icone_square(SDL_Renderer *rendu, SDL_Rect zone);
static void dessiner_icone_triangle(SDL_Renderer *rendu, SDL_Rect zone);
static void dessiner_icone_saw(SDL_Renderer *rendu, SDL_Rect zone);
static void dessiner_slider_vertical(SDL_Renderer *rendu, int x, int y, int hauteur, float valeur, float minimum, float maximum);
static void dessiner_texte(SDL_Renderer *rendu, TTF_Font *police, const char *texte, int x, int y);
static void dessiner_cercle_rempli(SDL_Renderer *rendu, int cx, int cy, int rayon);
static void dessiner_dial(SDL_Renderer *rendu, int centre_x, int centre_y, int rayon, float valeur);

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

    ui->fenetre = NULL;
    ui->rendu = NULL;
    ui->audio = 0;
    ui->etat_sdl = false;

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

    // AUDIO
    SDL_AudioSpec spec;
    SDL_zero(spec);

    spec.freq = 44100;
    spec.format = AUDIO_F32SYS;
    spec.channels = 1;
    spec.samples = 1024;
    spec.callback = NULL;
    spec.userdata = NULL;

    ui->audio = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0);
    if(ui->audio == 0){
        interface_detruire(ui);
        return NULL;
    }

    return ui;

}


void interface_detruire(interface_t *ui){
    if(ui == NULL) return;
    if(ui->audio != 0) SDL_CloseAudioDevice(ui->audio);
    if(ui->rendu != NULL) SDL_DestroyRenderer(ui->rendu);
    if(ui->fenetre != NULL) SDL_DestroyWindow(ui->fenetre);
    if (ui->police != NULL) TTF_CloseFont(ui->police);
    if (ui->etat_ttf) TTF_Quit();
    if(ui->etat_sdl) SDL_Quit();
    free(ui);
}


void interface_boucle(interface_t *ui){

    if (ui == NULL || ui->rendu == NULL) return;

    bool continuer = true;

    SDL_Rect zone_oscillateur   = {50, 50, 300, 250};
    SDL_Rect zone_adsr          = {400, 50, 250, 250};
    SDL_Rect zone_filtre        = {700, 50, 250, 250};

    SDL_Rect zone_sine          = {70, 100, 60, 60};
    SDL_Rect zone_square        = {140, 100, 60, 60};
    SDL_Rect zone_tri           = {210, 100, 60, 60};
    SDL_Rect zone_saw           = {280, 100, 60, 60};

    SDL_Rect icone_sine          = {80, 110, 40, 40};
    SDL_Rect icone_square        = {150, 110, 40, 40};
    SDL_Rect icone_tri           = {220, 110, 40, 40};
    SDL_Rect icone_saw           = {290, 110, 40, 40};

    while(continuer){
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
                if(SDL_PointInRect(&souris, &zone_adsr)){
                    //printf("ASDR appuye\n");

                }

                int note = piano_note_clique(x,y);
                if(note != -1) {
                    printf("Note MIDI jouee : %d\n", note);
                    piano_jouer_test(ui, note);
                }

                if (y >= 60 && y <= 230)
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

                // Selection du dial
                if (x >= 770 && x <= 880 &&
                    y >= 90 && y <= 200) {
                    ui->dial_actif = true;
                }

            }
            else if(evenement.type == SDL_MOUSEBUTTONUP){
                int x = evenement.button.x;
                int y = evenement.button.y;
                printf("Bouton relache a (%d, %d)\n", x, y);

                ui->slider_actif = -1;

                ui->dial_actif = false;
            }
            else if(evenement.type == SDL_MOUSEMOTION){
                int x = evenement.motion.x;
                int y = evenement.motion.y;
                printf("Souris en mouvement : (%d, %d)\n", x, y);

                if(ui->slider_actif != -1){
                    float ratio = 1.0f - (float)(y - 70) / 150.0f;

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

                if (ui->dial_actif) {

                    float ratio = 1.0f - (float)(y - 90) / 110.0f;

                    if (ratio < 0.0f) ratio = 0.0f;
                    if (ratio > 1.0f) ratio = 1.0f;

                    // Frequence entre 100 Hz et 10 000 Hz
                    ui->frequence_coupure_hz = 100.0f * powf(100.0f, ratio);

                    // Mettre a jour le filtre
                    filtre_set_coupure(ui->flt, ui->frequence_coupure_hz);
                }
            }
        }

        // Redraw interface here
        SDL_SetRenderDrawColor(ui->rendu, 30, 30, 30, 255);
        SDL_RenderClear(ui->rendu);

        // Zone Oscillateur
        SDL_SetRenderDrawColor(ui->rendu, 64, 64, 89, 255);
        SDL_RenderFillRect(ui->rendu, &zone_oscillateur);
        dessiner_texte(ui->rendu, ui->police, "OSCILLATEUR", 70, 70);

        // Bouton SINE
        if (ui->onde_selectionnee == ONDE_SINE){
            SDL_SetRenderDrawColor(ui->rendu, 50, 150, 255, 255); // Bleu sélectionné
            SDL_RenderFillRect(ui->rendu, &zone_sine);
            SDL_RenderFillRect(ui->rendu, &icone_sine);
            dessiner_icone_sinus(ui->rendu, icone_sine);
            dessiner_texte(ui->rendu, ui->police, "SINE", 75, 170);
        }
        else {
            SDL_SetRenderDrawColor(ui->rendu, 30, 30, 35, 255);
            SDL_RenderFillRect(ui->rendu, &zone_sine);
            SDL_SetRenderDrawColor(ui->rendu, 30, 30, 35, 255);
            SDL_RenderFillRect(ui->rendu, &icone_sine);
            dessiner_icone_sinus(ui->rendu, icone_sine);
            dessiner_texte(ui->rendu, ui->police, "SINE", 75, 170);
        }
        

        // Bouton SQUARE
        if (ui->onde_selectionnee == ONDE_SQUARE){
            SDL_SetRenderDrawColor(ui->rendu, 50, 150, 255, 255);
            SDL_RenderFillRect(ui->rendu, &zone_square);
            SDL_RenderFillRect(ui->rendu, &icone_square);
            dessiner_icone_square(ui->rendu, icone_square);
            dessiner_texte(ui->rendu, ui->police, "SQUARE", 130, 170);
        }
        else{
            SDL_SetRenderDrawColor(ui->rendu, 30, 30, 35, 255);
            SDL_RenderFillRect(ui->rendu, &zone_square);
            SDL_SetRenderDrawColor(ui->rendu, 30, 30, 35, 255);
            SDL_RenderFillRect(ui->rendu, &icone_square);
            dessiner_icone_square(ui->rendu, icone_square);
            dessiner_texte(ui->rendu, ui->police, "SQUARE", 130, 170);
        }
        

        // Bouton TRIANGLE
        if (ui->onde_selectionnee == ONDE_TRI){
            SDL_SetRenderDrawColor(ui->rendu, 50, 150, 255, 255);
            SDL_RenderFillRect(ui->rendu, &zone_tri);
            SDL_RenderFillRect(ui->rendu, &icone_tri);
            dessiner_icone_triangle(ui->rendu, icone_tri);
            dessiner_texte(ui->rendu, ui->police, "TRI", 225, 170);
        }
        else{
            SDL_SetRenderDrawColor(ui->rendu, 30, 30, 35, 255);
            SDL_RenderFillRect(ui->rendu, &zone_tri);
            SDL_SetRenderDrawColor(ui->rendu, 30, 30, 35, 255);
            SDL_RenderFillRect(ui->rendu, &icone_tri);
            dessiner_icone_triangle(ui->rendu, icone_tri);
            dessiner_texte(ui->rendu, ui->police, "TRI", 225, 170);
        }
        

        // Bouton SAW
        if (ui->onde_selectionnee == ONDE_SAW){
            SDL_SetRenderDrawColor(ui->rendu, 50, 150, 255, 255);
            SDL_RenderFillRect(ui->rendu, &zone_saw);
            SDL_RenderFillRect(ui->rendu, &icone_saw);
            dessiner_icone_saw(ui->rendu, icone_saw);
            dessiner_texte(ui->rendu, ui->police, "SAW", 290, 170);
        }
        else{
            SDL_SetRenderDrawColor(ui->rendu, 30, 30, 35, 255);
            SDL_RenderFillRect(ui->rendu, &zone_saw);
            SDL_SetRenderDrawColor(ui->rendu, 30, 30, 35, 255);
            SDL_RenderFillRect(ui->rendu, &icone_saw);
            dessiner_icone_saw(ui->rendu, icone_saw);
            dessiner_texte(ui->rendu, ui->police, "SAW", 290, 170);
        }


        // Zone Filtre
        SDL_SetRenderDrawColor(ui->rendu, 64, 64, 89, 255);
        SDL_RenderFillRect(ui->rendu, &zone_filtre);
        dessiner_texte(ui->rendu, ui->police, "FILTRE", 720, 70);

        // Dial de fréquence
        dessiner_dial(ui->rendu, 825, 170, 55, ui->frequence_coupure_hz);
        dessiner_texte(ui->rendu, ui->police, "CUTOFF", 790, 240);
        dessiner_texte(ui->rendu, ui->police, "LOW-PASS", 780, 260);

        // Zone ADSR
        SDL_SetRenderDrawColor(ui->rendu, 64, 64, 89, 255);
        SDL_RenderFillRect(ui->rendu, &zone_adsr);
        dessiner_texte(ui->rendu, ui->police, "ENVELOPPE", 420, 70);

        // Draw sliders
        dessiner_slider_vertical(ui->rendu, 425, 100, 150, ui->attack_ms, 0, 100);
        dessiner_slider_vertical(ui->rendu, 485, 100, 150, ui->decay_ms, 0, 1000);
        dessiner_slider_vertical(ui->rendu, 545, 100, 150, ui->sustain, 0, 1);
        dessiner_slider_vertical(ui->rendu, 605, 100, 150, ui->release_ms, 0, 1000);
        // ADSR labels
        dessiner_texte(ui->rendu, ui->police, "A", 422, 260);
        dessiner_texte(ui->rendu, ui->police, "D", 481, 260);
        dessiner_texte(ui->rendu, ui->police, "S", 543, 260);
        dessiner_texte(ui->rendu, ui->police, "R", 603, 260);

        piano_dessiner(ui->rendu);

        SDL_RenderPresent(ui->rendu);

        SDL_Delay(16);
    }
}

static void piano_dessiner(SDL_Renderer *rendu)
{
    int largeur_blanche = PIANO_LARGEUR / NB_TOUCHES_BLANCHES;
    int largeur_noire = 38;
    int hauteur_noire = 110;

    /* Dessiner les touches blanches */
    for (int i = 0; i < NB_TOUCHES_BLANCHES; i++)
    {
        SDL_Rect touche = {
            PIANO_X + i * largeur_blanche,
            PIANO_Y,
            largeur_blanche,
            PIANO_HAUTEUR
        };

        SDL_SetRenderDrawColor(rendu, 245, 245, 245, 255);
        SDL_RenderFillRect(rendu, &touche);

        SDL_SetRenderDrawColor(rendu, 20, 20, 20, 255);
        SDL_RenderDrawRect(rendu, &touche);
    }

    /* Dessiner les touches noires par-dessus les blanches */
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

        SDL_SetRenderDrawColor(rendu, 25, 25, 25, 255);
        SDL_RenderFillRect(rendu, &touche);
    }
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


static void piano_jouer_test(interface_t *ui, int note_midi)
{
    const float duree_note = 0.300f;

    // Calcul de la frequence de la note MIDI
    float frequence = 440.0f * powf(2.0f, (note_midi - 69) / 12.0f);

    // Mise a jour de la frequence de l'oscillateur
    oscillateur_set_frequence(ui->osc, frequence);

    // Calcul du nombre d'echantillons necessaires
    int nb_echantillons = enveloppe_duree_totale(ui->env, duree_note);

    if (nb_echantillons <= 0) {
        return;
    }

    // Allocation du tampon audio
    float *buffer = malloc((size_t)nb_echantillons * sizeof(float));

    if (buffer == NULL) {
        return;
    }

    // 1. Generation de la forme d'onde
    oscillateur_generer(ui->osc, buffer, nb_echantillons);

    // 2. Application du filtre
    filtre_appliquer(ui->flt, buffer, nb_echantillons);

    // 3. Application de l'enveloppe ADSR
    enveloppe_appliquer(ui->env, buffer, nb_echantillons, duree_note);

    // 4. Envoi du son vers SDL
    SDL_ClearQueuedAudio(ui->audio);

    SDL_QueueAudio(
        ui->audio,
        buffer,
        (Uint32)(nb_echantillons * sizeof(float))
    );

    SDL_PauseAudioDevice(ui->audio, 0);

    // Liberation du tampon
    free(buffer);
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
    SDL_Color blanc = {255, 255, 255, 255};

    SDL_Surface *surface = TTF_RenderText_Blended(
        police,
        texte,
        blanc
    );

    if (surface == NULL) {
        return;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(
        rendu,
        surface
    );

    if (texture != NULL) {

        SDL_Rect destination = {
            x,
            y,
            surface->w,
            surface->h
        };

        SDL_RenderCopy(rendu, texture, NULL, &destination);

        SDL_DestroyTexture(texture);
    }

    SDL_FreeSurface(surface);
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
    const float FREQ_MIN = 100.0f;
    const float FREQ_MAX = 10000.0f;

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

    /* Limiter la fréquence */
    if (frequence < FREQ_MIN)
        frequence = FREQ_MIN;

    if (frequence > FREQ_MAX)
        frequence = FREQ_MAX;

    /* Convertir la fréquence en position angulaire */
    float ratio = logf(frequence / FREQ_MIN) /
                  logf(FREQ_MAX / FREQ_MIN);

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
