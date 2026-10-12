#include <stdio.h>
#include <stdbool.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_image.h>   // NOVO: necessario para carregar o menu.png
#include <allegro5/allegro_font.h>    // NOVO: necessario para escrever o texto do botao
#include <allegro5/allegro_ttf.h>     // NOVO: necessario para a fonte do botao
#include "input.h" // Inclui o nosso módulo de teclado separado
#include "menu.h"  // NOVO: modulo do menu

#define LARGURA_TELA 1280   // Alterado para combinar com a proporcao da imagem do menu
#define ALTURA_TELA 720

int main() {
    // 1. Inicializar o Allegro e os complementos necessários
    if (!al_init()) {
        fprintf(stderr, "Falha ao inicializar o Allegro!\n");
        return -1;
    }

    if (!al_install_keyboard()) {
        fprintf(stderr, "Falha ao instalar o teclado!\n");
        return -1;
    }

    // NOVO: o menu usa o mouse para clicar no botao Jogar
    if (!al_install_mouse()) {
        fprintf(stderr, "Falha ao instalar o mouse!\n");
        return -1;
    }

    if (!al_init_primitives_addon()) {
        fprintf(stderr, "Falha ao inicializar as primitivas!\n");
        return -1;
    }

    // NOVO: addons usados pelo menu (imagem e texto)
    if (!al_init_image_addon() || !al_init_font_addon() || !al_init_ttf_addon()) {
        fprintf(stderr, "Falha ao inicializar imagem/fonte!\n");
        return -1;
    }

    // 2. Criar Janela
    ALLEGRO_DISPLAY* display = al_create_display(LARGURA_TELA, ALTURA_TELA);
    if (!display) {
        fprintf(stderr, "Falha ao criar a janela!\n");
        return -1;
    }
    al_set_window_title(display, "Gramatical Tower - POC (Modularizado)");

    // NOVO: mostra o menu ANTES do jogo. So continua se o jogador clicar em Jogar.
    if (!menu_executar(display)) {
        al_destroy_display(display);
        return 0;
    }

    // Fila de Eventos e Temporizador (60 FPS) - criados depois do menu
    ALLEGRO_EVENT_QUEUE* fila_eventos = al_create_event_queue();
    ALLEGRO_TIMER* timer = al_create_timer(1.0 / 60.0);

    al_register_event_source(fila_eventos, al_get_display_event_source(display));
    al_register_event_source(fila_eventos, al_get_keyboard_event_source());
    al_register_event_source(fila_eventos, al_get_timer_event_source(timer));

    al_start_timer(timer);

    // 3. Posição inicial do círculo e estrutura de controlos
    float x = LARGURA_TELA / 2.0f;
    float y = ALTURA_TELA / 2.0f;
    float raio = 20.0f;
    float velocidade = 5.0f;

    Controles controles = { 0 }; // Inicializa todos os estados a falso
    bool rodando = true;
    bool redesenhar = true;

    // 4. Loop Principal do Jogo
    while (rodando) {
        ALLEGRO_EVENT evento;
        al_wait_for_event(fila_eventos, &evento);

        if (evento.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
            rodando = false;
        }
        else if (evento.type == ALLEGRO_EVENT_TIMER) {
            // Atualizar a posição do círculo com base nas teclas ativas do módulo input
            if (controles.cima) y -= velocidade;
            if (controles.baixo) y += velocidade;
            if (controles.esquerda) x -= velocidade;
            if (controles.direita) x += velocidade;

            redesenhar = true;
        }
        else {
            // Envia os eventos de teclado para a nossa função separada em input.c
            processar_teclado(&evento, &controles);
            if (controles.sair) {
                rodando = false;
            }
        }

        // Renderização gráfica
        if (redesenhar && al_is_event_queue_empty(fila_eventos)) {
            redesenhar = false;

            al_clear_to_color(al_map_rgb(20, 20, 30)); // Fundo escuro
            al_draw_filled_circle(x, y, raio, al_map_rgb(255, 100, 100)); // Círculo

            al_flip_display();
        }
    }

    // 5. Libertar memória antes de fechar
    al_destroy_timer(timer);
    al_destroy_event_queue(fila_eventos);
    al_destroy_display(display);

    return 0;
}