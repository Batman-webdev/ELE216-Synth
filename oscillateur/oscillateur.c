#include "module_A.h"
#include <stdlib.h>

struct oscillateur_t{
    onde_type_t type_onde;
    float frequence;
    float amplitude; 
    int taux_echantillon;
    float phase_accumulee;
};

oscillateur_t *oscillateur_creer(onde_type_t type, float frequence, float amplitude, int taux){

    if (taux <= 0 || frequence < 0.0f)
    return NULL;
 
    oscillateur_t *o = malloc(sizeof *o);
    if (o == NULL)
        return NULL;
        
    if (amplitude < 0.0f) amplitude = 0.0f;
    if (amplitude > 1.0f) amplitude = 1.0f;
 
    o->type_onde = type;
    o->frequence = frequence;
    o->amplitude = amplitude;
    o->taux_echantillon = taux;
    o->phase_accumulee = 0.0f;
    
    return o;
}


void oscillateur_detruire(oscillateur_t *o)
{

    if(o==NULL)
        return;

    free(o);
}
 

void oscillateur_generer(oscillateur_t *o, float *buffer, int nb_echantillons)
{
    if (o == NULL || buffer == NULL || nb_echantillons <= 0)
        return;
 
    const float increment = o->frequence / (float)o->taux_echantillon;
    float phase = o->phase_accumulee;
 
    for (int i = 0; i < nb_echantillons; i++) {
        buffer[i] = o->amplitude * echantillon(o->type_onde, phase);
 
        phase += increment;
        while (phase >= 1.0f)
            phase -= 1.0f;
    }
 
    o->phase_accumulee = phase;
}


void oscillateur_set_frequence(oscillateur_t *o, float freq)
{
    if (o == NULL || freq < 0.0f)
        return;
    o->frequence = freq;
}
 
void oscillateur_set_type(oscillateur_t *o, onde_type_t type)
{
    if (o == NULL)
        return;
    o->type_onde = type;
}
 
void oscillateur_reset_phase(oscillateur_t *o)
{
    if (o == NULL)
        return;
    o->phase_accumulee = 0.0f;
}
 
float oscillateur_frequence(const oscillateur_t *o)
{
    return (o != NULL) ? o->frequence : 0.0f;
}
 
onde_type_t oscillateur_type(const oscillateur_t *o)
{
    return (o != NULL) ? o->type_onde : ONDE_SINE;
}