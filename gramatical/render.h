/*
 * render.h - Desenho do jogo com Allegro 5 (primitives + font).
 */
#ifndef RENDER_H
#define RENDER_H

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include "jogo.h"

typedef struct {
    ALLEGRO_FONT* pequena;   /* ~13 px: rótulos e chips   */
    ALLEGRO_FONT* normal;    /* ~18 px: mensagens e HUD   */
    ALLEGRO_FONT* grande;    /* ~42 px: títulos           */
    ALLEGRO_BITMAP* fundo_menu; /* menu.png (pode ser NULL) */
} Fontes;

void render_jogo(const Jogo* j, const Fontes* f);

#endif#pragma once
