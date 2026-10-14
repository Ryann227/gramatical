#include <stdio.h>
#include <stdbool.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_image.h>   
#include <allegro5/allegro_font.h>    
#include <allegro5/allegro_ttf.h>    
#include "input.h" 
#include "menu.h" 

#define LARGURA_TELA 1280  
#define ALTURA_TELA 720

int main() {
    
    if (!al_init()) {
        fprintf(stderr, "Falha ao inicializar o Allegro!\n");
        return -1;
    }

    if (!al_install_keyboard()) {
        fprintf(stderr, "Falha ao instalar o teclado!\n");
        return -1;
    }

    
    if (!al_install_mouse()) {
        fprintf(stderr, "Falha ao instalar o mouse!\n");
        return -1;
    }

    if (!al_init_primitives_addon()) {
        fprintf(stderr, "Falha ao inicializar as primitivas!\n");
        return -1;
    }

    
    if (!al_init_image_addon() || !al_init_font_addon() || !al_init_ttf_addon()) {
        fprintf(stderr, "Falha ao inicializar imagem/fonte!\n");
        return -1;
    }

    
    ALLEGRO_DISPLAY* display = al_create_display(LARGURA_TELA, ALTURA_TELA);
    if (!display) {
        fprintf(stderr, "Falha ao criar a janela!\n");
        return -1;
    }
    al_set_window_title(display, "Gramatical Tower - POC (Modularizado)");

    
    if (!menu_executar(display)) {
        al_destroy_display(display);
        return 0;
    }

    
    ALLEGRO_EVENT_QUEUE* fila_eventos = al_create_event_queue();
    ALLEGRO_TIMER* timer = al_create_timer(1.0 / 60.0);

    al_register_event_source(fila_eventos, al_get_display_event_source(display));
    al_register_event_source(fila_eventos, al_get_keyboard_event_source());
    al_register_event_source(fila_eventos, al_get_timer_event_source(timer));

    al_start_timer(timer);

    
    float x = LARGURA_TELA / 2.0f;
    float y = ALTURA_TELA / 2.0f;
    float raio = 20.0f;
    float velocidade = 5.0f;

    Controles controles = { 0 }; 
    bool rodando = true;
    bool redesenhar = true;

    
    while (rodando) {
        ALLEGRO_EVENT evento;
        al_wait_for_event(fila_eventos, &evento);

        if (evento.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
            rodando = false;
        }
        else if (evento.type == ALLEGRO_EVENT_TIMER) {
            
            if (controles.cima) y -= velocidade;
            if (controles.baixo) y += velocidade;
            if (controles.esquerda) x -= velocidade;
            if (controles.direita) x += velocidade;

            redesenhar = true;
        }
        else {
            
            processar_teclado(&evento, &controles);
            if (controles.sair) {
                rodando = false;
            }
        }

        if (redesenhar && al_is_event_queue_empty(fila_eventos)) {
            redesenhar = false;

            al_clear_to_color(al_map_rgb(20, 20, 30)); 
            al_draw_filled_circle(x, y, raio, al_map_rgb(255, 100, 100)); //circulo

            al_flip_display();
        }
    }

    
    al_destroy_timer(timer);
    al_destroy_event_queue(fila_eventos);
    al_destroy_display(display);

    return 0;
}