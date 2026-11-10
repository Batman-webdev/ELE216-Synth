#include "interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_video.h>

#define PIANO_X 50
#define PIANO_Y 350
#define PIANO_LARGEUR 900
#define PIANO_HAUTEUR 180
#define NB_TOUCHES_BLANCHES 14

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
};

static void piano_dessiner(SDL_Renderer *rendu);
static int piano_note_clique(int x, int y);
static void piano_jouer_test(interface_t *ui, int note_midi);

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

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0){
        free(ui);
        return NULL;
    }

    ui->etat_sdl = true;

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
    if(ui->etat_sdl) SDL_Quit();
    free(ui);
}


void interface_boucle(interface_t *ui){

    if (ui == NULL || ui->rendu == NULL) return;

    bool continuer = true;

    SDL_Rect zone_oscillateur   = {50, 50, 200, 100};
    SDL_Rect zone_filtre        = {300, 50, 200, 100};
    SDL_Rect zone_adsr          = {550, 50, 350, 100};

    // FENETRE
    SDL_SetRenderDrawColor(ui->rendu, 30, 30, 35, 255);
    SDL_RenderClear(ui->rendu);

    // Zone Oscillateur
    SDL_SetRenderDrawColor(ui->rendu, 60, 130, 220, 255);
    SDL_RenderFillRect(ui->rendu, &zone_oscillateur);

    // Zone Filtre
    SDL_SetRenderDrawColor(ui->rendu, 60, 180, 120, 255);
    SDL_RenderFillRect(ui->rendu, &zone_filtre);

    // Zone Oscillateur
    SDL_SetRenderDrawColor(ui->rendu, 180, 100, 200, 255);
    SDL_RenderFillRect(ui->rendu, &zone_adsr);

    piano_dessiner(ui->rendu);

    SDL_RenderPresent(ui->rendu);

    while(continuer){
        SDL_Event evenement;

        while(SDL_PollEvent(&evenement)){
            if(evenement.type == SDL_QUIT) continuer = false;
            else if(evenement.type == SDL_MOUSEBUTTONDOWN){
                int x = evenement.button.x;
                int y = evenement.button.y;

                SDL_Point souris = {x,y};

                if(SDL_PointInRect(&souris, &zone_oscillateur)) printf("Oscillateur appuye\n");
                if(SDL_PointInRect(&souris, &zone_filtre)) printf("Filtre appuye\n");
                if(SDL_PointInRect(&souris, &zone_adsr)) printf("ASDR appuye\n");

                int note = piano_note_clique(x,y);
                if(note != -1) {
                    printf("Note MIDI jouee : %d\n", note);
                    piano_jouer_test(ui, note);
                }

            }
            else if(evenement.type == SDL_MOUSEBUTTONUP){
                int x = evenement.button.x;
                int y = evenement.button.y;
                printf("Bouton relache a (%d, %d)\n", x, y);
            }
            else if(evenement.type == SDL_MOUSEMOTION){
                int x = evenement.motion.x;
                int y = evenement.motion.y;
                printf("Souris en mouvement : (%d, %d)\n", x, y);

            }
        }

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
    if (ui == NULL || ui->audio == 0) return;

    const int frequence_echantillonnage = 44100;
    const double frequence = 440.0 * pow(2.0, (note_midi - 69) / 12.0);
    const double amplitude = 0.25;
    const double pi = 3.14159265358979323846;
    const int duree_ms = 300;

    int nb_echantillons =
        frequence_echantillonnage * duree_ms / 1000;

    float *buffer = malloc(
        (size_t)nb_echantillons * sizeof(float)
    );

    if (buffer == NULL)
    {
        return;
    }

    for (int i = 0; i < nb_echantillons; i++)
    {
        double temps = (double)i / frequence_echantillonnage;

        buffer[i] = (float)(
            amplitude * sin(2.0 * pi * frequence * temps)
        );
    }

    SDL_ClearQueuedAudio(ui->audio);

    SDL_QueueAudio(ui->audio, buffer, (Uint32)(nb_echantillons * sizeof(float)));

    SDL_PauseAudioDevice(ui->audio, 0);

    free(buffer);
}