#ifndef MENU_H
#define MENU_H

#include <stdbool.h>
#include <allegro5/allegro.h>

/*
 * Mostra o menu com o botao "Jogar".
 * Retorna true se o jogador clicou em Jogar,
 * ou false se fechou a janela / apertou ESC.
 *
 * Precisa de "menu.png" na pasta do executavel.
 */
bool menu_executar(ALLEGRO_DISPLAY* janela);

#endif /* MENU_H */