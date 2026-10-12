#include "input.h"
#include <stdbool.h>
#include <allegro5/allegro.h>
#include <allegro5/keyboard.h>

void processar_teclado(ALLEGRO_EVENT* evento, Controles* c) {
    if (evento->type == ALLEGRO_EVENT_KEY_DOWN) {
        switch (evento->keyboard.keycode) {
        case ALLEGRO_KEY_W:      c->cima = true; break;
        case ALLEGRO_KEY_S:      c->baixo = true; break;
        case ALLEGRO_KEY_A:      c->esquerda = true; break;
        case ALLEGRO_KEY_D:      c->direita = true; break;
        case ALLEGRO_KEY_ESCAPE: c->sair = true; break;
        }
    }
    else if (evento->type == ALLEGRO_EVENT_KEY_UP) {
        switch (evento->keyboard.keycode) {
        case ALLEGRO_KEY_W:      c->cima = false; break;
        case ALLEGRO_KEY_S:      c->baixo = false; break;
        case ALLEGRO_KEY_A:      c->esquerda = false; break;
        case ALLEGRO_KEY_D:      c->direita = false; break;
        }
    }
}