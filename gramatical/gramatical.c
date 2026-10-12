#include <stdio.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h> // Essencial para desenhar formas geométricas

#define LARGURA_TELA 800
#define ALTURA_TELA 600

int main() {
    // 1. Inicializar o Allegro
    if (!al_init()) {
        fprintf(stderr, "Falha ao inicializar o Allegro!\n");
        return -1;
    }

    // 2. Inicializar o add-on de primitivas (necessário para desenhar o círculo)
    if (!al_init_primitives_addon()) {
        fprintf(stderr, "Falha ao inicializar o add-on de primitivas!\n");
        return -1;
    }

    // 3. Instalar o teclado (para permitir sair com a tecla ESC)
    if (!al_install_keyboard()) {
        fprintf(stderr, "Falha ao instalar o teclado!\n");
        return -1;
    }

    // 4. Criar a Janela (Display)
    ALLEGRO_DISPLAY* display = al_create_display(LARGURA_TELA, ALTURA_TELA);
    if (!display) {
        fprintf(stderr, "Falha ao criar a janela!\n");
        return -1;
    }
    al_set_window_title(display, "Gramatical Tower - POC (Circulo)");

    // 5. Criar a Fila de Eventos
    ALLEGRO_EVENT_QUEUE* fila_eventos = al_create_event_queue();
    if (!fila_eventos) {
        fprintf(stderr, "Falha ao criar a fila de eventos!\n");
        al_destroy_display(display);
        return -1;
    }

    al_register_event_source(fila_eventos, al_get_display_event_source(display));
    al_register_event_source(fila_eventos, al_get_keyboard_event_source());

    int rodando = 1;

    // 6. O Game Loop
    while (rodando) {
        // Tratar eventos (teclado e fechar janela)
        while (!al_is_event_queue_empty(fila_eventos)) {
            ALLEGRO_EVENT evento;
            al_wait_for_event(fila_eventos, &evento);

            if (evento.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
                rodando = 0;
            }
            else if (evento.type == ALLEGRO_EVENT_KEY_DOWN) {
                if (evento.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
                    rodando = 0;
                }
            }
        }

        // --- RENDERIZAÇÃO ---
        // Limpa o ecrã com uma cor de fundo (ex: azul escuro)
        al_clear_to_color(al_map_rgb(20, 30, 50));

        // Desenhar um círculo colorido preenchido no centro da tela (X=400, Y=300, Raio=50)
        al_draw_filled_circle(400, 300, 50, al_map_rgb(0, 255, 128));

        // Atualiza a tela (Double Buffering)
        al_flip_display();

        // Pequena pausa para economizar processamento (~60 FPS)
        al_rest(0.016);
    }

    // 7. Destruir recursos antes de fechar
    al_destroy_event_queue(fila_eventos);
    al_destroy_display(display);

    return 0;
}