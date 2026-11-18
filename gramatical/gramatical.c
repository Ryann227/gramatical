/*
 * main.c - Inicialização do Allegro 5, laço principal e entrada.
 *
 * O laço usa passo fixo de 60 quadros por segundo: o timer dispara 60 vezes
 * por segundo e cada disparo chama jogo_atualizar() uma vez. O desenho só
 * acontece quando a fila de eventos está vazia.
 */
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_image.h>   /* carregar o menu.png */

#include "jogo.h"
#include "render.h"

#define FPS 60.0

 /* Procura uma fonte TTF. Para os acentos aparecerem direito, coloque um
  * arquivo "assets/font.ttf" ao lado do executável (qualquer fonte livre).
  * Se nada for encontrado, usa a fonte embutida do Allegro (sem acentos). */
static ALLEGRO_FONT* carregar_fonte(int tamanho)
{
    static const char* candidatos[] = {
        "assets/font.ttf",
        "font.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial.ttf"
    };
    size_t i;
    ALLEGRO_FONT* f;

    for (i = 0; i < sizeof(candidatos) / sizeof(candidatos[0]); i++) {
        f = al_load_ttf_font(candidatos[i], tamanho, 0);
        if (f) return f;
    }
    return al_create_builtin_font();
}

/* Carrega o menu.png: tenta a pasta do projeto e algumas pastas acima,
 * primeiro a partir da pasta de trabalho e depois da pasta do executável. */
static ALLEGRO_BITMAP* carregar_menu(void)
{
    const char* nomes[5] = { "menu.png", "../menu.png", "../../menu.png", "../../gramatical/menu.png", "assets/menu.png" };
    ALLEGRO_PATH* exe = al_get_standard_path(ALLEGRO_RESOURCES_PATH);
    ALLEGRO_BITMAP* b = NULL;
    char caminho[1024];
    int i;

    for (i = 0; i < 5 && !b; i++)
        b = al_load_bitmap(nomes[i]);
    for (i = 0; i < 5 && !b && exe; i++) {
        snprintf(caminho, sizeof(caminho), "%s/%s", al_path_cstr(exe, '/'), nomes[i]);
        b = al_load_bitmap(caminho);
    }
    if (exe) al_destroy_path(exe);
    return b;
}

int main(void)
{
    ALLEGRO_DISPLAY* tela = NULL;
    ALLEGRO_TIMER* timer = NULL;
    ALLEGRO_EVENT_QUEUE* fila = NULL;
    Fontes fontes;
    static Jogo jogo;
    Entrada in = { 0, 0, 0, 0 };
    int rodando = 1, redesenhar = 1;

    if (!al_init()) { fprintf(stderr, "Falha ao iniciar o Allegro.\n"); return 1; }
    if (!al_init_primitives_addon()) { fprintf(stderr, "Falha no addon de primitivas.\n"); return 1; }
    al_init_font_addon();
    al_init_ttf_addon();
    al_init_image_addon();
    if (!al_install_keyboard() || !al_install_mouse()) {
        fprintf(stderr, "Falha ao instalar teclado/mouse.\n");
        return 1;
    }

    tela = al_create_display(LARGURA_TELA, ALTURA_TELA);
    if (!tela) { fprintf(stderr, "Falha ao criar a janela.\n"); return 1; }
    al_set_window_title(tela, "Gramatical Tower");

    timer = al_create_timer(1.0 / FPS);
    fila = al_create_event_queue();
    if (!timer || !fila) { fprintf(stderr, "Falha ao criar timer/fila.\n"); return 1; }

    al_register_event_source(fila, al_get_display_event_source(tela));
    al_register_event_source(fila, al_get_timer_event_source(timer));
    al_register_event_source(fila, al_get_keyboard_event_source());
    al_register_event_source(fila, al_get_mouse_event_source());

    fontes.pequena = carregar_fonte(13);
    fontes.normal = carregar_fonte(18);
    fontes.grande = carregar_fonte(42);

    fontes.fundo_menu = carregar_menu();

    jogo_iniciar(&jogo);
    al_start_timer(timer);

    while (rodando) {
        ALLEGRO_EVENT ev;
        al_wait_for_event(fila, &ev);

        switch (ev.type) {
        case ALLEGRO_EVENT_DISPLAY_CLOSE:
            rodando = 0;
            break;

        case ALLEGRO_EVENT_TIMER: {
            ALLEGRO_KEYBOARD_STATE ks;
            al_get_keyboard_state(&ks);
            in.esquerda = al_key_down(&ks, ALLEGRO_KEY_A) || al_key_down(&ks, ALLEGRO_KEY_LEFT);
            in.direita = al_key_down(&ks, ALLEGRO_KEY_D) || al_key_down(&ks, ALLEGRO_KEY_RIGHT);
            in.pulo_segurando = al_key_down(&ks, ALLEGRO_KEY_W) || al_key_down(&ks, ALLEGRO_KEY_UP) ||
                al_key_down(&ks, ALLEGRO_KEY_SPACE);
            jogo_atualizar(&jogo, &in, (float)(1.0 / FPS));
            in.pulo_apertou = 0;
            redesenhar = 1;
            break;
        }

        case ALLEGRO_EVENT_KEY_DOWN:
            switch (ev.keyboard.keycode) {
            case ALLEGRO_KEY_ESCAPE:
                if (jogo.tela == TELA_MENU) rodando = 0;   /* no menu, ESC fecha        */
                else jogo_iniciar(&jogo);                  /* nas outras, volta ao menu */
                break;
            case ALLEGRO_KEY_ENTER:
            case ALLEGRO_KEY_PAD_ENTER:
                jogo_confirmar(&jogo);                     /* menu -> introdução -> jogo */
                break;
            case ALLEGRO_KEY_R:
                if (jogo.tela == TELA_JOGANDO || jogo.tela == TELA_VITORIA || jogo.tela == TELA_DERROTA) {
                    jogo_iniciar(&jogo);
                    jogo_comecar(&jogo);
                }
                break;
            case ALLEGRO_KEY_W:
            case ALLEGRO_KEY_UP:
            case ALLEGRO_KEY_SPACE:
                in.pulo_apertou = 1;
                break;
            case ALLEGRO_KEY_1: case ALLEGRO_KEY_PAD_1: jogo_selecionar(&jogo, CL_SUBSTANTIVO); break;
            case ALLEGRO_KEY_2: case ALLEGRO_KEY_PAD_2: jogo_selecionar(&jogo, CL_ADJETIVO);    break;
            case ALLEGRO_KEY_3: case ALLEGRO_KEY_PAD_3: jogo_selecionar(&jogo, CL_VERBO);       break;
            case ALLEGRO_KEY_4: case ALLEGRO_KEY_PAD_4: jogo_selecionar(&jogo, CL_ADVERBIO);    break;
            default: break;
            }
            break;

        case ALLEGRO_EVENT_MOUSE_AXES:
            jogo.mouse_x = (float)ev.mouse.x;
            jogo.mouse_y = (float)ev.mouse.y;
            if (ev.mouse.dz != 0) jogo_ciclar_feitico(&jogo, ev.mouse.dz > 0 ? -1 : 1);
            break;

        case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
            if (ev.mouse.button == 1) {
                jogo.mouse_x = (float)ev.mouse.x;
                jogo.mouse_y = (float)ev.mouse.y;
                jogo_clique(&jogo, (float)ev.mouse.x, (float)ev.mouse.y);
            }
            break;

        default:
            break;
        }

        if (redesenhar && al_is_event_queue_empty(fila)) {
            render_jogo(&jogo, &fontes);
            al_flip_display();
            redesenhar = 0;
        }
    }

    al_destroy_font(fontes.pequena);
    al_destroy_font(fontes.normal);
    al_destroy_font(fontes.grande);
    if (fontes.fundo_menu) al_destroy_bitmap(fontes.fundo_menu);
    al_destroy_event_queue(fila);
    al_destroy_timer(timer);
    al_destroy_display(tela);
    return 0;
}