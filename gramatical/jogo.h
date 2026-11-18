/*
 * jogo.h - Tipos, constantes e funções da lógica do Gramatical Tower.
 *
 * Este módulo NÃO depende do Allegro: toda a regra do jogo (física,
 * feitiços, inimigos, fases, pontuação) fica em logica.c. Assim dá para
 * testar a lógica sem abrir janela e trocar a parte gráfica sem mexer
 * nas regras.
 */
#ifndef JOGO_H
#define JOGO_H

#define LARGURA_TELA    960
#define ALTURA_TELA     544
#define TILE            32
#define MAPA_COLS       70
#define MAPA_LINHAS     17
#define MAX_PALAVRAS    12
#define MAX_INIMIGOS    10
#define MAX_CHECKS      6
#define MAX_TOCHAS      6
#define N_FASES         4
#define ALCANCE_FEITICO 380.0f

 /* Layout da barra de feitiços (usado pela lógica para detectar cliques
  * e pelo render para desenhar). */
#define SLOT_W   88
#define SLOT_H   50
#define SLOT_GAP 8
#define SLOT_Y   8
#define SLOT_X(i) (((LARGURA_TELA - (4 * SLOT_W + 3 * SLOT_GAP)) / 2) + (i) * (SLOT_W + SLOT_GAP))

  /* Botões do menu, centralizados na tela sobre o menu.png. */
#define BTN_X     380
#define BTN_Y(i)  (392 + (i) * 60)
#define BTN_W     200
#define BTN_H     46
   /* Botões das telas História e Introdução (linha de baixo) */
#define SUB_Y        470
#define SUB_W        200
#define SUB_H        42
#define SUB_VOLTAR_X_HIST    380
#define SUB_VOLTAR_X_INTRO   270
#define SUB_COMECAR_X        490

/* As quatro classes gramaticais = os quatro feitiços da Pena Tinteiro. */
typedef enum {
    CL_SUBSTANTIVO = 0,   /* cria matéria        (ex.: plataforma) */
    CL_ADJETIVO,          /* altera a física     (ex.: congelado)  */
    CL_VERBO,             /* ativa a dinâmica    (ex.: mover)      */
    CL_ADVERBIO,          /* ajusta intensidade  (ex.: lentamente) */
    CL_TOTAL
} Classe;

typedef enum {
    TELA_MENU = 0,
    TELA_JOGANDO,
    TELA_VITORIA,
    TELA_DERROTA,
    TELA_HISTORIA,
    TELA_INTRO
} TelaEstado;

/* Tipos de palavra do cenário. */
typedef enum {
    PAL_PLATAFORMA = 0,   /* vira ponte com substantivo, anda com verbo   */
    PAL_LAVA,             /* queima até ser congelada com adjetivo        */
    PAL_ESPINHO           /* machuca até ser congelada com adjetivo       */
} TipoPalavra;

/* Tipos de guardião corrompido. */
typedef enum {
    INIM_LIVRO = 0,   /* comum: patrulha o chão, 1 acerto        */
    INIM_MORCEGO,     /* voa em onda, rápido, 1 acerto           */
    INIM_GOLEM,       /* lento, resiste a 2 acertos              */
    INIM_ESPECTRO,    /* voa e se teletransporta, 1 acerto       */
    INIM_CHEFE        /* chefe: grande, rápido, 3 acertos        */
} TipoInimigo;

/* Faixas de ranking (calculadas pela pontuação). */
enum { RANK_D = 0, RANK_C, RANK_B, RANK_A, RANK_S, RANK_TOTAL };

enum { MSG_NEUTRA = 0, MSG_BOA = 1, MSG_RUIM = 2 };
enum { ALVO_NENHUM = 0, ALVO_INIMIGO = 1, ALVO_PALAVRA = 2 };

/* Palavra do cenário: começa "dormente" e vai sendo transformada pelos
 * feitiços do jogador. */
typedef struct {
    TipoPalavra tipo;
    char  texto[24];
    float x, y, w, h;
    float x_min, x_max;     /* limites do movimento (se x_max > x_min)     */
    int   materializada;    /* substantivo aplicado                        */
    int   congelada;        /* adjetivo aplicado                           */
    int   movendo;          /* verbo aplicado                              */
    int   lenta;            /* advérbio aplicado                           */
    int   desafiada;        /* já rendeu pontos de desafio                 */
    float dir;              /* +1 direita, -1 esquerda                     */
    float brilho;           /* efeito visual ao receber um feitiço         */
} Palavra;

/* Guardião corrompido: uma palavra solta. Só cai se o jogador lançar a
 * classe gramatical correta sobre ela (alguns precisam de mais de uma). */
typedef struct {
    char  texto[24];
    int   classe;           /* Classe correta                              */
    int   tipo;             /* INIM_*                                      */
    int   vida, vida_max;   /* acertos ainda necessários                   */
    float x, y, w, h;
    float x_min, x_max, vx;
    float y_base;
    int   voador, vivo;
    float t, flash, tele;
} Inimigo;

typedef struct {
    float x, y, vx, vy, w, h;
    int   no_chao, no_gelo, olhando;
    int   vidas;
    float tinta;
    float invulneravel, atordoado;
    float coyote, buffer_pulo;
} Jogador;

typedef struct {
    int   ativa;
    char  texto[24];
    int   classe;
    float tempo;
} Analise;

/* Entrada do jogador no frame atual (preenchida pelo main.c). */
typedef struct {
    int esquerda, direita;
    int pulo_segurando;
    int pulo_apertou;       /* vale só no frame em que a tecla foi apertada */
} Entrada;

typedef struct {
    TelaEstado tela;
    char       mapa[MAPA_LINHAS][MAPA_COLS + 1];
    Jogador    jog;
    Palavra    palavras[MAX_PALAVRAS];
    int        n_palavras;
    Inimigo    inimigos[MAX_INIMIGOS];
    int        n_inimigos;
    int        selecionada;         /* classe escolhida na barra de feitiços */
    float      cam_x;
    float      check_x, check_y;    /* último checkpoint                     */
    float      checks[MAX_CHECKS];  /* colunas dos checkpoints da fase       */
    int        n_checks;
    float      tochas[MAX_TOCHAS];  /* posições das tochas da fase           */
    int        n_tochas;
    float      saida_x, saida_y;    /* porta de saída da fase                */
    char       msg[128];
    int        msg_cor;
    float      msg_tempo;
    Analise    analise;
    float      tempo_anim;
    int        derrotados;
    float      mouse_x, mouse_y;    /* posição do mouse em coords de tela    */

    /* --- fases e ranking --- */
    int        fase;                       /* 0 .. N_FASES-1                */
    int        pontos;                     /* pontuação acumulada           */
    int        desafios;                   /* desafios já cumpridos         */
    int        desafios_total;             /* desafios possíveis na fase    */
    int        pontos_max_fase[N_FASES];   /* teto de pontos de cada fase   */
    int        pontos_max;                 /* teto acumulado até esta fase  */
    int        recorde;                    /* melhor pontuação (ranking.txt)*/
    char       aviso[64];                  /* faixa "FASE 2: ..."           */
    float      aviso_tempo;
} Jogo;

extern const char* const NOME_CLASSE[CL_TOTAL];      /* "Substantivo"...  */
extern const char* const NOME_CLASSE_MIN[CL_TOTAL];  /* "substantivo"...  */
extern const char* const FUNCAO_CLASSE[CL_TOTAL];    /* "Nomear"...       */
extern const float       CUSTO_CLASSE[CL_TOTAL];     /* tinta por feitiço */
extern const char* const NOME_FASE[N_FASES];         /* nome das fases    */
extern const char* const NOME_RANK[RANK_TOTAL];      /* "S - Escriba..."  */
extern const char* const DESC_RANK[RANK_TOTAL];      /* frase do rank     */

void jogo_iniciar(Jogo* j);                  /* recria o jogo (tela = menu) */
void jogo_comecar(Jogo* j);                  /* sai do menu e joga          */
void jogo_carregar_fase(Jogo* j, int fase);  /* monta a fase pedida         */
void jogo_atualizar(Jogo* j, const Entrada* in, float dt);
void jogo_clique(Jogo* j, float mx, float my);
void jogo_confirmar(Jogo* j);                /* ENTER: avança de tela       */
void jogo_selecionar(Jogo* j, int classe);
void jogo_ciclar_feitico(Jogo* j, int dir);
void jogo_mensagem(Jogo* j, const char* txt, int cor);

int  jogo_slot_em(float mx, float my);       /* slot sob o mouse ou -1      */
int  jogo_alvo(const Jogo* j, float mx, float my, int* idx, int* em_alcance);
int  jogo_colide(const Jogo* j, float x, float y, float w, float h);
int  palavra_solida(const Palavra* p);
int  jogo_rank(const Jogo* j);               /* RANK_* conforme a pontuação */

#endif