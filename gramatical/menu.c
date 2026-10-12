#include <stdio.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_primitives.h>

#include "menu.h"

bool menu_executar(ALLEGRO_DISPLAY* janela)
{
    const float largura = (float)al_get_display_width(janela);
    const float altura = (float)al_get_display_height(janela);

    ALLEGRO_BITMAP* fundo = al_load_bitmap("menu.png");
    if (!fundo) {
        fprintf(stderr, "Nao foi possivel carregar menu.png\n");
        return false;
    }

    ALLEGRO_FONT* fonte = al_load_ttf_font("C:/Windows/Fonts/arial.ttf", 36, 0);
    if (!fonte) fonte = al_create_builtin_font();

    ALLEGRO_EVENT_QUEUE* fila = al_create_event_queue();
    al_register_event_source(fila, al_get_display_event_source(janela));
    al_register_event_source(fila, al_get_mouse_event_source());
    al_register_event_source(fila, al_get_keyboard_event_source());

    /* Botao Jogar: centralizado, na parte de baixo da tela */
    const float bw = 260, bh = 70;
    const float bx = (largura - bw) / 2.0f;
    const float by = altura * 0.85f;

    float mouse_x = 0, mouse_y = 0;
    bool jogar = false;
    bool rodando = true;

    while (rodando) {
        /* Desenha */
        al_draw_scaled_bitmap(fundo, 0, 0,
            (float)al_get_bitmap_width(fundo),
            (float)al_get_bitmap_height(fundo),
            0, 0, largura, altura, 0);

        bool hover = mouse_x >= bx && mouse_x <= bx + bw &&
            mouse_y >= by && mouse_y <= by + bh;

        al_draw_filled_rounded_rectangle(bx, by, bx + bw, by + bh, 14, 14,
            hover ? al_map_rgb(200, 160, 80) : al_map_rgb(40, 30, 50));
        al_draw_rounded_rectangle(bx, by, bx + bw, by + bh, 14, 14,
            al_map_rgb(235, 205, 140), 3);
        al_draw_text(fonte,
            hover ? al_map_rgb(30, 20, 10) : al_map_rgb(245, 225, 180),
            bx + bw / 2, by + (bh - al_get_font_line_height(fonte)) / 2,
            ALLEGRO_ALIGN_CENTRE, "Jogar");

        al_flip_display();

        /* Trata eventos */
        ALLEGRO_EVENT ev;
        while (al_get_next_event(fila, &ev)) {
            if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
                rodando = false;
            }
            else if (ev.type == ALLEGRO_EVENT_KEY_DOWN &&
                ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
                rodando = false;
            }
            else if (ev.type == ALLEGRO_EVENT_MOUSE_AXES) {
                mouse_x = (float)ev.mouse.x;
                mouse_y = (float)ev.mouse.y;
            }
            else if (ev.type == ALLEGRO_EVENT_MOUSE_BUTTON_UP &&
                ev.mouse.button == 1 && hover) {
                jogar = true;
                rodando = false;
            }
        }

        al_rest(1.0 / 60.0);
    }

    al_destroy_event_queue(fila);
    al_destroy_font(fonte);
    al_destroy_bitmap(fundo);

    return jogar;
}