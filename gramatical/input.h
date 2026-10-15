#ifndef INPUT_H
#define INPUT_H

#include <allegro5/allegro.h>
#include <stdbool.h>


typedef struct {
    bool cima;
    bool baixo;
    bool esquerda;
    bool direita;
    bool sair;
} Controles;

void processar_teclado(ALLEGRO_EVENT* evento, Controles* c);

#endif
