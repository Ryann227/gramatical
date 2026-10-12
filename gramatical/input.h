#ifndef INPUT_H
#define INPUT_H

#include <allegro5/allegro.h>
#include <stdbool.h>

// Estrutura para os controlos
typedef struct {
    bool cima;
    bool baixo;
    bool esquerda;
    bool direita;
    bool sair;
} Controles;

// Apenas a assinatura da função (sem o código cá dentro)
void processar_teclado(ALLEGRO_EVENT* evento, Controles* c);

#endif
