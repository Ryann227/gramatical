/*
 * logica.c - Regras do Gramatical Tower (sem Allegro).
 *
 * Seções:
 *   1. Dados, utilidades e ranking
 *   2. Colisão
 *   3. Montagem das fases (4 fases)
 *   4. Feitiços (Pena Tinteiro)
 *   5. Atualização por frame (palavras, jogador, inimigos)
 */
#define _CRT_SECURE_NO_WARNINGS
#include "jogo.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

 /* ------------------------------------------------------------------ */
 /* 1. Dados, utilidades e ranking                                      */
 /* ------------------------------------------------------------------ */

const char* const NOME_CLASSE[CL_TOTAL] = { "Substantivo", "Adjetivo", "Verbo", "Advérbio" };
const char* const NOME_CLASSE_MIN[CL_TOTAL] = { "substantivo", "adjetivo", "verbo", "advérbio" };
const char* const FUNCAO_CLASSE[CL_TOTAL] = { "Nomear", "Qualificar", "Ação", "Modificar" };
const float       CUSTO_CLASSE[CL_TOTAL] = { 20.0f, 15.0f, 15.0f, 10.0f };

const char* const NOME_FASE[N_FASES] = {
    "Biblioteca de Sintaxia",
    "Corredor dos Ecos",
    "Cripta dos Verbos",
    "Torre do Corruptor"
};

const char* const NOME_RANK[RANK_TOTAL] = {
    "D - Rabisco",
    "C - Aprendiz de Tinta",
    "B - Escriba Real",
    "A - Mestre da Gramática",
    "S - Escriba Lendário"
};

const char* const DESC_RANK[RANK_TOTAL] = {
    "Muitos guardiões ficaram corrompidos. Tente de novo!",
    "A biblioteca sobreviveu, mas vários desafios ficaram para trás.",
    "Bom trabalho: a maior parte das palavras voltou ao lugar.",
    "Excelente! Quase nenhum erro escapou da sua pena.",
    "Perfeição gramatical: a biblioteca inteira se curva à sua pena."
};

#define VEL_JOGADOR     190.0f
#define GRAVIDADE       1500.0f
#define FORCA_PULO      560.0f
#define VEL_QUEDA_MAX   900.0f
#define TINTA_MAX       100.0f
#define TINTA_REGEN     8.0f      /* tinta por segundo */
#define VEL_PLAT        100.0f    /* plataforma móvel normal  */
#define VEL_PLAT_LENTA  35.0f     /* com o advérbio "lentamente" */

/* Pontuação (não existe mais cronômetro: o ranking é por desempenho). */
#define PTS_DESAFIO     40        /* primeiro feitiço numa palavra         */
#define PTS_REPETIDO    10        /* feitiço extra na mesma palavra        */
#define PTS_PARCIAL     30        /* acerto parcial em golem/chefe         */
#define PTS_ERRO       -15        /* classe errada num guardião            */
#define PTS_DANO       -40        /* levar dano / queimar / cair           */
#define PTS_FASE       200        /* concluir a fase                       */
#define PTS_VIDA        50        /* bônus por vida restante               */

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(v, lo, hi) (MAX((lo), MIN((v), (hi))))

static int retangulos(float ax, float ay, float aw, float ah,
    float bx, float by, float bw, float bh)
{
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static int ponto_em(float px, float py, float x, float y, float w, float h)
{
    return px >= x && px <= x + w && py >= y && py <= y + h;
}

void jogo_mensagem(Jogo* j, const char* txt, int cor)
{
    snprintf(j->msg, sizeof(j->msg), "%s", txt);
    j->msg_cor = cor;
    j->msg_tempo = 3.2f;
}

static void avisar(Jogo* j, const char* txt, float t)
{
    snprintf(j->aviso, sizeof(j->aviso), "%s", txt);
    j->aviso_tempo = t;
}

static void pontuar(Jogo* j, int p)
{
    j->pontos += p;
    if (j->pontos < 0) j->pontos = 0;
}

static int pts_tipo(int tipo)
{
    switch (tipo) {
    case INIM_MORCEGO:  return 150;
    case INIM_ESPECTRO: return 180;
    case INIM_GOLEM:    return 240;
    case INIM_CHEFE:    return 500;
    default:            return 120;
    }
}

int jogo_rank(const Jogo* j)
{
    int pct;
    if (j->pontos_max <= 0) return RANK_D;
    pct = j->pontos * 100 / j->pontos_max;
    if (pct >= 90) return RANK_S;
    if (pct >= 75) return RANK_A;
    if (pct >= 58) return RANK_B;
    if (pct >= 40) return RANK_C;
    return RANK_D;
}

/* Recorde só em memória: vale enquanto o jogo estiver aberto. */
static void atualizar_recorde(Jogo* j)
{
    if (j->pontos > j->recorde) j->recorde = j->pontos;
}

/* ------------------------------------------------------------------ */
/* 2. Colisão                                                          */
/* ------------------------------------------------------------------ */

int palavra_solida(const Palavra* p)
{
    if (p->tipo == PAL_PLATAFORMA) return p->materializada;
    if (p->tipo == PAL_LAVA)       return p->congelada;
    return 0;   /* espinhos nunca são sólidos: congelar apenas os desarma */
}

static int tile_solido(const Jogo* j, int col, int lin)
{
    if (col < 0 || col >= MAPA_COLS) return 1;   /* paredes invisíveis nas bordas */
    if (lin < 0 || lin >= MAPA_LINHAS) return 0; /* céu aberto / poço */
    return j->mapa[lin][col] == '#';
}

int jogo_colide(const Jogo* j, float x, float y, float w, float h)
{
    int c0 = (int)floorf(x / TILE);
    int c1 = (int)floorf((x + w - 0.001f) / TILE);
    int l0 = (int)floorf(y / TILE);
    int l1 = (int)floorf((y + h - 0.001f) / TILE);
    int c, l, i;

    for (l = l0; l <= l1; l++)
        for (c = c0; c <= c1; c++)
            if (tile_solido(j, c, l)) return 1;

    for (i = 0; i < j->n_palavras; i++) {
        const Palavra* p = &j->palavras[i];
        if (palavra_solida(p) && retangulos(x, y, w, h, p->x, p->y, p->w, p->h))
            return 1;
    }
    return 0;
}

static int em_gelo(const Jogo* j)
{
    const Jogador* p = &j->jog;
    int i;
    for (i = 0; i < j->n_palavras; i++) {
        const Palavra* w = &j->palavras[i];
        if (palavra_solida(w) && w->congelada &&
            retangulos(p->x, p->y + p->h - 1.0f, p->w, 4.0f, w->x, w->y, w->w, w->h))
            return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* 3. Montagem das fases                                               */
/* ------------------------------------------------------------------ */

static void bloco(Jogo* j, int c0, int c1, int l0, int l1)
{
    int c, l;
    for (l = l0; l <= l1; l++)
        for (c = c0; c <= c1; c++)
            j->mapa[l][c] = '#';
}

static void marca_check(Jogo* j, int col)
{
    if (j->n_checks >= MAX_CHECKS) return;
    j->checks[j->n_checks++] = (float)(col * TILE);
}

static void tocha(Jogo* j, int col)
{
    if (j->n_tochas >= MAX_TOCHAS) return;
    j->tochas[j->n_tochas++] = (float)(col * TILE + 16);
}

static Palavra* nova_palavra(Jogo* j, TipoPalavra tipo, const char* texto,
    float x, float y, float w, float h,
    float x_min, float x_max)
{
    Palavra* p;
    if (j->n_palavras >= MAX_PALAVRAS) return NULL;
    p = &j->palavras[j->n_palavras++];
    memset(p, 0, sizeof(*p));
    p->tipo = tipo;
    snprintf(p->texto, sizeof(p->texto), "%s", texto);
    p->x = x; p->y = y; p->w = w; p->h = h;
    p->x_min = x_min; p->x_max = x_max;
    p->dir = 1.0f;
    return p;
}

static void dados_tipo(int tipo, float* w, float* h, int* vida, int* voador)
{
    switch (tipo) {
    case INIM_MORCEGO:  *w = 34.0f; *h = 26.0f; *vida = 1; *voador = 1; break;
    case INIM_GOLEM:    *w = 46.0f; *h = 46.0f; *vida = 2; *voador = 0; break;
    case INIM_ESPECTRO: *w = 32.0f; *h = 38.0f; *vida = 1; *voador = 1; break;
    case INIM_CHEFE:    *w = 64.0f; *h = 60.0f; *vida = 3; *voador = 0; break;
    default:            *w = 36.0f; *h = 34.0f; *vida = 1; *voador = 0; break;
    }
}

/* y <= 0 significa "encoste no chão" (usado pelos inimigos terrestres). */
static void novo_inimigo(Jogo* j, int tipo, const char* texto, int classe,
    float x, float y, float x_min, float x_max, float vx)
{
    Inimigo* e;
    float w, h;
    int vida, voador;

    if (j->n_inimigos >= MAX_INIMIGOS) return;
    dados_tipo(tipo, &w, &h, &vida, &voador);
    e = &j->inimigos[j->n_inimigos++];
    memset(e, 0, sizeof(*e));
    snprintf(e->texto, sizeof(e->texto), "%s", texto);
    e->classe = classe;
    e->tipo = tipo;
    e->w = w; e->h = h;
    e->vida = e->vida_max = vida;
    e->voador = voador;
    e->x = x;
    e->y = voador ? y : (14 * TILE - h);
    e->y_base = e->y;
    e->x_min = x_min; e->x_max = x_max;
    e->vx = vx;
    e->vivo = 1;
    e->tele = 2.0f;
    if (e->x < e->x_min) e->x = e->x_min;
    if (e->x > e->x_max) e->x = e->x_max;
}

static void atualizar_camera(Jogo* j, float suavizar)
{
    float alvo = j->jog.x + j->jog.w / 2 - LARGURA_TELA / 2;
    alvo = CLAMP(alvo, 0.0f, (float)(MAPA_COLS * TILE - LARGURA_TELA));
    j->cam_x += (alvo - j->cam_x) * suavizar;
}

static void limpar_mundo(Jogo* j)
{
    int l, c;
    for (l = 0; l < MAPA_LINHAS; l++) {
        for (c = 0; c < MAPA_COLS; c++) j->mapa[l][c] = '.';
        j->mapa[l][MAPA_COLS] = '\0';
    }
    j->n_palavras = 0;
    j->n_inimigos = 0;
    j->n_checks = 0;
    j->n_tochas = 0;
    j->derrotados = 0;
    j->desafios = 0;
    j->analise.ativa = 0;
    j->analise.tempo = 0.0f;
    j->msg_tempo = 0.0f;
    j->msg[0] = '\0';
    j->saida_x = 0.0f;
    j->saida_y = 0.0f;
}

/* Fase 1: "Biblioteca de Sintaxia" - tutorial de todas as classes.
 *   cols  0-11  chão inicial
 *   cols 12-17  poço  -> "plataforma" (substantivo)
 *   cols 18-27  chão  -> guardião "casa"
 *   cols 28-39  poço largo -> "plataforma" que precisa se mover (verbo)
 *   cols 40-47  chão  -> guardião "pontes"
 *   cols 48-53  lava  -> congelar com adjetivo
 *   cols 54-69  chão  -> 3 guardiões e a saída
 */
static void construir_fase1(Jogo* j)
{
    bloco(j, 0, 11, 14, 16);
    bloco(j, 18, 27, 14, 16);
    bloco(j, 40, 47, 14, 16);
    bloco(j, 54, 69, 14, 16);
    bloco(j, 43, 44, 12, 13);          /* degrau para pular */

    nova_palavra(j, PAL_PLATAFORMA, "plataforma",
        13 * TILE, 14 * TILE, 4 * TILE, TILE,
        13 * TILE, 13 * TILE);                   /* sem movimento */
    nova_palavra(j, PAL_PLATAFORMA, "plataforma",
        29 * TILE, 14 * TILE, 4 * TILE, TILE,
        29 * TILE, 35 * TILE);                   /* vai e volta   */
    nova_palavra(j, PAL_LAVA, "lava",
        48 * TILE, 14 * TILE, 6 * TILE, 3 * TILE, 0, 0);

    novo_inimigo(j, INIM_LIVRO, "casa", CL_SUBSTANTIVO, 20 * TILE, 0,
        19 * TILE, 26 * TILE, 50);
    novo_inimigo(j, INIM_LIVRO, "pontes", CL_SUBSTANTIVO, 41 * TILE, 0,
        41 * TILE, 46 * TILE, 30);
    novo_inimigo(j, INIM_LIVRO, "veloz", CL_ADJETIVO, 55 * TILE, 0,
        55 * TILE, 59 * TILE, 70);
    novo_inimigo(j, INIM_MORCEGO, "destruiu", CL_VERBO, 56 * TILE, 11 * TILE,
        56 * TILE, 62 * TILE, 60);
    novo_inimigo(j, INIM_LIVRO, "lentamente", CL_ADVERBIO, 61 * TILE, 0,
        61 * TILE, 65 * TILE, 40);

    j->saida_x = 67 * TILE;
    j->saida_y = 14 * TILE - 64;

    tocha(j, 9); tocha(j, 20); tocha(j, 42); tocha(j, 56); tocha(j, 65);
    marca_check(j, 19); marca_check(j, 40); marca_check(j, 54);
}

/* Fase 2: "Corredor dos Ecos" - morcegos, golem e travessias longas.
 *   0-9 chão | 10-19 poço (plataforma estreita que anda) | 20-25 chão
 *   26-33 lava | 34-39 chão | 40-49 poço com lajes | 50-57 chão
 *   58-65 poço (plataforma que anda) | 66-69 chão + saída
 */
static void construir_fase2(Jogo* j)
{
    bloco(j, 0, 9, 14, 16);
    bloco(j, 20, 25, 14, 16);
    bloco(j, 34, 39, 14, 16);
    bloco(j, 50, 57, 14, 16);
    bloco(j, 66, 69, 14, 16);

    bloco(j, 41, 43, 12, 12);          /* laje 1 */
    bloco(j, 46, 48, 12, 12);          /* laje 2 */

    nova_palavra(j, PAL_PLATAFORMA, "plataforma",
        10 * TILE, 14 * TILE, 2 * TILE, TILE,
        10 * TILE, 18 * TILE);                   /* precisa do verbo */
    nova_palavra(j, PAL_ESPINHO, "espinhos",
        22 * TILE, 14 * TILE - 16, 3 * TILE, 16, 0, 0);
    nova_palavra(j, PAL_LAVA, "lava",
        26 * TILE, 14 * TILE, 8 * TILE, 3 * TILE, 0, 0);
    nova_palavra(j, PAL_PLATAFORMA, "plataforma",
        58 * TILE, 14 * TILE, 2 * TILE, TILE,
        58 * TILE, 64 * TILE);                   /* precisa do verbo */

    novo_inimigo(j, INIM_MORCEGO, "sussurro", CL_SUBSTANTIVO, 11 * TILE, 11 * TILE,
        10 * TILE, 18 * TILE, 70);
    novo_inimigo(j, INIM_LIVRO, "corredor", CL_SUBSTANTIVO, 21 * TILE, 0,
        20 * TILE, 25 * TILE, 55);
    novo_inimigo(j, INIM_LIVRO, "racharam", CL_VERBO, 35 * TILE, 0,
        34 * TILE, 39 * TILE, 60);
    novo_inimigo(j, INIM_ESPECTRO, "gélido", CL_ADJETIVO, 42 * TILE, 9 * TILE,
        40 * TILE, 49 * TILE, 50);
    novo_inimigo(j, INIM_GOLEM, "muralha", CL_SUBSTANTIVO, 52 * TILE, 0,
        50 * TILE, 57 * TILE, 40);
    novo_inimigo(j, INIM_MORCEGO, "saltou", CL_VERBO, 60 * TILE, 10 * TILE,
        58 * TILE, 65 * TILE, 80);
    novo_inimigo(j, INIM_LIVRO, "brevemente", CL_ADVERBIO, 67 * TILE, 0,
        66 * TILE, 68 * TILE, 45);
    novo_inimigo(j, INIM_ESPECTRO, "fugaz", CL_ADJETIVO, 67 * TILE, 9 * TILE,
        66 * TILE, 68 * TILE, 60);

    j->saida_x = 67 * TILE;
    j->saida_y = 14 * TILE - 64;

    tocha(j, 4); tocha(j, 21); tocha(j, 35); tocha(j, 52); tocha(j, 66);
    marca_check(j, 20); marca_check(j, 34); marca_check(j, 50); marca_check(j, 66);
}

/* Fase 3: "Cripta dos Verbos" - sobe nas lajes, golems e espectros.
 *   0-7 chão | 8-17 poço (plataforma que anda) | 18-23 chão + espinhos
 *   24-31 lava | 32-37 chão | 38-47 poço com lajes | 48-55 chão
 *   56-63 poço (plataforma que anda) | 64-69 chão + saída
 */
static void construir_fase3(Jogo* j)
{
    bloco(j, 0, 7, 14, 16);
    bloco(j, 18, 23, 14, 16);
    bloco(j, 32, 37, 14, 16);
    bloco(j, 48, 55, 14, 16);
    bloco(j, 64, 69, 14, 16);

    bloco(j, 39, 41, 12, 12);
    bloco(j, 44, 46, 10, 10);

    nova_palavra(j, PAL_PLATAFORMA, "plataforma",
        8 * TILE, 14 * TILE, 4 * TILE, TILE,
        8 * TILE, 14 * TILE);                    /* vai e volta (verbo) */
    nova_palavra(j, PAL_ESPINHO, "espinhos",
        20 * TILE, 14 * TILE - 16, 3 * TILE, 16, 0, 0);
    nova_palavra(j, PAL_LAVA, "lava",
        24 * TILE, 14 * TILE, 8 * TILE, 3 * TILE, 0, 0);
    nova_palavra(j, PAL_PLATAFORMA, "plataforma",
        56 * TILE, 14 * TILE, 2 * TILE, TILE,
        56 * TILE, 62 * TILE);                   /* precisa do verbo */

    novo_inimigo(j, INIM_LIVRO, "desfizeram", CL_VERBO, 19 * TILE, 0,
        18 * TILE, 23 * TILE, 60);
    novo_inimigo(j, INIM_GOLEM, "pedra", CL_SUBSTANTIVO, 33 * TILE, 0,
        32 * TILE, 37 * TILE, 40);
    novo_inimigo(j, INIM_ESPECTRO, "sombrio", CL_ADJETIVO, 40 * TILE, 8 * TILE,
        38 * TILE, 47 * TILE, 55);
    novo_inimigo(j, INIM_MORCEGO, "sussurrou", CL_VERBO, 51 * TILE, 11 * TILE,
        48 * TILE, 55 * TILE, 70);
    novo_inimigo(j, INIM_LIVRO, "calada", CL_ADJETIVO, 50 * TILE, 0,
        48 * TILE, 55 * TILE, 50);
    novo_inimigo(j, INIM_MORCEGO, "tinteiro", CL_SUBSTANTIVO, 59 * TILE, 9 * TILE,
        56 * TILE, 63 * TILE, 65);
    novo_inimigo(j, INIM_LIVRO, "vagarosamente", CL_ADVERBIO, 65 * TILE, 0,
        64 * TILE, 67 * TILE, 45);
    novo_inimigo(j, INIM_GOLEM, "quebrou", CL_VERBO, 66 * TILE, 0,
        64 * TILE, 68 * TILE, 50);
    novo_inimigo(j, INIM_ESPECTRO, "imensa", CL_ADJETIVO, 66 * TILE, 9 * TILE,
        64 * TILE, 68 * TILE, 60);

    j->saida_x = 67 * TILE;
    j->saida_y = 14 * TILE - 64;

    tocha(j, 3); tocha(j, 19); tocha(j, 33); tocha(j, 49); tocha(j, 65);
    marca_check(j, 18); marca_check(j, 32); marca_check(j, 48); marca_check(j, 64);
}

/* Fase 4: "Torre do Corruptor" - arena final com o chefe.
 *   0-10 chão | 11-18 lava | 19-26 chão + espinhos | 27-36 poço (anda)
 *   37-45 chão | 46-53 poço com lajes | 54-69 arena do chefe
 */
static void construir_fase4(Jogo* j)
{
    bloco(j, 0, 10, 14, 16);
    bloco(j, 19, 26, 14, 16);
    bloco(j, 37, 45, 14, 16);
    bloco(j, 54, 69, 14, 16);

    bloco(j, 47, 49, 12, 12);
    bloco(j, 51, 53, 12, 12);

    nova_palavra(j, PAL_LAVA, "lava",
        11 * TILE, 14 * TILE, 8 * TILE, 3 * TILE, 0, 0);
    nova_palavra(j, PAL_ESPINHO, "espinhos",
        21 * TILE, 14 * TILE - 16, 3 * TILE, 16, 0, 0);
    nova_palavra(j, PAL_PLATAFORMA, "plataforma",
        27 * TILE, 14 * TILE, 2 * TILE, TILE,
        27 * TILE, 35 * TILE);

    novo_inimigo(j, INIM_GOLEM, "corrompeu", CL_VERBO, 20 * TILE, 0,
        19 * TILE, 26 * TILE, 45);
    novo_inimigo(j, INIM_LIVRO, "gélida", CL_ADJETIVO, 23 * TILE, 0,
        19 * TILE, 26 * TILE, 60);
    novo_inimigo(j, INIM_ESPECTRO, "abismo", CL_SUBSTANTIVO, 29 * TILE, 9 * TILE,
        27 * TILE, 36 * TILE, 60);
    novo_inimigo(j, INIM_MORCEGO, "sombra", CL_SUBSTANTIVO, 39 * TILE, 10 * TILE,
        37 * TILE, 45 * TILE, 70);
    novo_inimigo(j, INIM_MORCEGO, "desfez", CL_VERBO, 42 * TILE, 11 * TILE,
        37 * TILE, 45 * TILE, 60);
    novo_inimigo(j, INIM_LIVRO, "eterna", CL_ADJETIVO, 56 * TILE, 0,
        54 * TILE, 60 * TILE, 55);
    novo_inimigo(j, INIM_CHEFE, "Corruptor", CL_SUBSTANTIVO, 60 * TILE, 0,
        56 * TILE, 68 * TILE, 55);

    j->saida_x = 67 * TILE;
    j->saida_y = 14 * TILE - 64;

    tocha(j, 5); tocha(j, 20); tocha(j, 40); tocha(j, 56); tocha(j, 66);
    marca_check(j, 19); marca_check(j, 37); marca_check(j, 54);
}

static int calcular_max_fase(const Jogo* j)
{
    int i, total = PTS_FASE + 3 * PTS_VIDA;
    for (i = 0; i < j->n_inimigos; i++) total += pts_tipo(j->inimigos[i].tipo);
    for (i = 0; i < j->n_palavras; i++) total += PTS_DESAFIO;
    return total;
}

void jogo_carregar_fase(Jogo* j, int fase)
{
    Jogador* p = &j->jog;
    int i, soma;

    if (fase < 0) fase = 0;
    if (fase >= N_FASES) fase = N_FASES - 1;

    j->fase = fase;
    limpar_mundo(j);

    switch (fase) {
    case 0:  construir_fase1(j); break;
    case 1:  construir_fase2(j); break;
    case 2:  construir_fase3(j); break;
    default: construir_fase4(j); break;
    }

    j->desafios_total = j->n_inimigos + j->n_palavras;
    j->pontos_max_fase[fase] = calcular_max_fase(j);
    soma = 0;
    for (i = 0; i <= fase; i++) soma += j->pontos_max_fase[i];
    j->pontos_max = soma;

    p->w = 20.0f; p->h = 28.0f;
    p->x = 2 * TILE;
    p->y = 14 * TILE - p->h - 1.0f;
    p->vx = p->vy = 0.0f;
    p->olhando = 1;
    p->vidas = 3;
    p->tinta = TINTA_MAX;
    p->no_chao = p->no_gelo = 0;
    p->invulneravel = p->atordoado = 0.0f;
    p->coyote = p->buffer_pulo = 0.0f;

    j->check_x = p->x;
    j->check_y = p->y;
    j->cam_x = 0.0f;
    j->selecionada = CL_SUBSTANTIVO;
    atualizar_camera(j, 1.0f);
}

void jogo_iniciar(Jogo* j)
{
    int rec = j->recorde;   /* preserva o recorde da sessão */

    memset(j, 0, sizeof(*j));
    j->tela = TELA_MENU;
    j->recorde = rec;
    j->pontos = 0;
    jogo_carregar_fase(j, 0);
    j->tela = TELA_MENU;
}

void jogo_comecar(Jogo* j)
{
    char buf[64];
    j->tela = TELA_JOGANDO;
    snprintf(buf, sizeof(buf), "FASE 1: %s", NOME_FASE[0]);
    avisar(j, buf, 2.6f);
    jogo_mensagem(j, "Restaure todos os guardiões e chegue à saída!", MSG_NEUTRA);
}

/* Fim de fase: pontua, passa para a próxima ou fecha o jogo com o ranking. */
static void concluir_fase(Jogo* j)
{
    int vidas = j->jog.vidas;
    char buf[64];

    pontuar(j, PTS_FASE + vidas * PTS_VIDA);

    if (j->fase + 1 < N_FASES) {
        int prox = j->fase + 1;
        jogo_carregar_fase(j, prox);
        j->jog.vidas = vidas;                 /* mantém as vidas ganhas */
        snprintf(buf, sizeof(buf), "FASE %d: %s", prox + 1, NOME_FASE[prox]);
        avisar(j, buf, 2.6f);
        jogo_mensagem(j, "Fase concluída! Novos guardiões à frente.", MSG_BOA);
        j->tela = TELA_JOGANDO;
    }
    else {
        atualizar_recorde(j);
        j->tela = TELA_VITORIA;
    }
}

/* ------------------------------------------------------------------ */
/* 4. Feitiços (Pena Tinteiro)                                         */
/* ------------------------------------------------------------------ */

void jogo_selecionar(Jogo* j, int classe)
{
    if (classe >= 0 && classe < CL_TOTAL) j->selecionada = classe;
}

void jogo_ciclar_feitico(Jogo* j, int dir)
{
    j->selecionada = (j->selecionada + dir + CL_TOTAL) % CL_TOTAL;
}

int jogo_slot_em(float mx, float my)
{
    int i;
    for (i = 0; i < CL_TOTAL; i++)
        if (ponto_em(mx, my, (float)SLOT_X(i), (float)SLOT_Y, (float)SLOT_W, (float)SLOT_H))
            return i;
    return -1;
}

/* Descobre o que está sob o mouse. Inimigos têm prioridade sobre palavras. */
int jogo_alvo(const Jogo* j, float mx, float my, int* idx, int* em_alcance)
{
    float wx = mx + j->cam_x, wy = my;
    float pcx = j->jog.x + j->jog.w / 2, pcy = j->jog.y + j->jog.h / 2;
    int i;

    for (i = 0; i < j->n_inimigos; i++) {
        const Inimigo* e = &j->inimigos[i];
        if (e->vivo && ponto_em(wx, wy, e->x - 8, e->y - 24, e->w + 16, e->h + 32)) {
            float dx = e->x + e->w / 2 - pcx, dy = e->y + e->h / 2 - pcy;
            *idx = i;
            *em_alcance = sqrtf(dx * dx + dy * dy) <= ALCANCE_FEITICO;
            return ALVO_INIMIGO;
        }
    }
    for (i = 0; i < j->n_palavras; i++) {
        const Palavra* p = &j->palavras[i];
        if (ponto_em(wx, wy, p->x - 4, p->y - 4, p->w + 8, p->h + 8)) {
            float dx = p->x + p->w / 2 - pcx, dy = p->y + p->h / 2 - pcy;
            *idx = i;
            *em_alcance = sqrtf(dx * dx + dy * dy) <= ALCANCE_FEITICO;
            return ALVO_PALAVRA;
        }
    }
    return ALVO_NENHUM;
}

/* Cobra a tinta. Retorna 0 se não tem o bastante. */
static int gastar(Jogo* j, int classe)
{
    if (j->jog.tinta < CUSTO_CLASSE[classe]) {
        jogo_mensagem(j, "Tinta insuficiente! Espere a pena recarregar.", MSG_RUIM);
        return 0;
    }
    j->jog.tinta -= CUSTO_CLASSE[classe];
    return 1;
}

static void pontuar_palavra(Jogo* j, Palavra* p)
{
    if (!p->desafiada) {
        p->desafiada = 1;
        j->desafios++;
        pontuar(j, PTS_DESAFIO);
    }
    else {
        pontuar(j, PTS_REPETIDO);
    }
}

static void lancar_em_inimigo(Jogo* j, Inimigo* e)
{
    int cl = j->selecionada;
    char buf[128];

    if (!gastar(j, cl)) return;

    if (cl == e->classe) {
        e->vida--;
        e->flash = 0.35f;

        if (e->vida > 0) {
            pontuar(j, PTS_PARCIAL);
            snprintf(buf, sizeof(buf), "\"%s\" resistiu! Faltam %d acerto(s).",
                e->texto, e->vida);
            jogo_mensagem(j, buf, MSG_BOA);
            return;
        }

        e->vivo = 0;
        j->derrotados++;
        j->desafios++;
        pontuar(j, pts_tipo(e->tipo));
        j->jog.tinta = MIN(TINTA_MAX, j->jog.tinta + 12.0f);   /* recompensa */

        j->analise.ativa = 1;
        j->analise.classe = cl;
        j->analise.tempo = 4.5f;
        snprintf(j->analise.texto, sizeof(j->analise.texto), "%s", e->texto);

        snprintf(buf, sizeof(buf), "\"%s\" é um %s! +%d pontos.",
            e->texto, NOME_CLASSE_MIN[cl], pts_tipo(e->tipo));
        jogo_mensagem(j, buf, MSG_BOA);
    }
    else {
        e->flash = 0.45f;
        pontuar(j, PTS_ERRO);
        snprintf(buf, sizeof(buf), "\"%s\" não é um %s. Tente outra classe!",
            e->texto, NOME_CLASSE_MIN[cl]);
        jogo_mensagem(j, buf, MSG_RUIM);
    }
}

static void lancar_em_palavra(Jogo* j, Palavra* p, int cl)
{
    if (p->tipo == PAL_PLATAFORMA) {
        if (cl == CL_SUBSTANTIVO) {
            if (p->materializada) { jogo_mensagem(j, "Essa plataforma já existe.", MSG_NEUTRA); return; }
            if (!gastar(j, cl)) return;
            p->materializada = 1; p->brilho = 0.6f;
            pontuar_palavra(j, p);
            jogo_mensagem(j, "Substantivo! A palavra virou matéria.", MSG_BOA);
        }
        else if (!p->materializada) {
            jogo_mensagem(j, "Ainda não há matéria. Comece com um substantivo!", MSG_RUIM);
        }
        else if (cl == CL_ADJETIVO) {
            if (p->congelada) { jogo_mensagem(j, "Já está congelada.", MSG_NEUTRA); return; }
            if (!gastar(j, cl)) return;
            p->congelada = 1; p->brilho = 0.6f;
            pontuar_palavra(j, p);
            jogo_mensagem(j, "Adjetivo! A plataforma congelou (escorrega!).", MSG_BOA);
        }
        else if (cl == CL_VERBO) {
            if (p->x_max <= p->x_min) { jogo_mensagem(j, "Essa plataforma não tem para onde se mover.", MSG_NEUTRA); return; }
            if (p->movendo) { jogo_mensagem(j, "Já está em movimento.", MSG_NEUTRA); return; }
            if (!gastar(j, cl)) return;
            p->movendo = 1; p->brilho = 0.6f;
            pontuar_palavra(j, p);
            jogo_mensagem(j, "Verbo! A plataforma ganhou movimento.", MSG_BOA);
        }
        else { /* CL_ADVERBIO */
            if (!p->movendo) { jogo_mensagem(j, "Um advérbio precisa de um verbo para modificar!", MSG_RUIM); return; }
            if (p->lenta) { jogo_mensagem(j, "Já está lenta.", MSG_NEUTRA); return; }
            if (!gastar(j, cl)) return;
            p->lenta = 1; p->brilho = 0.6f;
            pontuar_palavra(j, p);
            jogo_mensagem(j, "Advérbio! Ela agora se move lentamente.", MSG_BOA);
        }
    }
    else { /* PAL_LAVA ou PAL_ESPINHO: só o adjetivo resolve */
        if (cl == CL_ADJETIVO) {
            if (p->congelada) { jogo_mensagem(j, "Já está congelado.", MSG_NEUTRA); return; }
            if (!gastar(j, cl)) return;
            p->congelada = 1; p->brilho = 0.6f;
            pontuar_palavra(j, p);
            if (p->tipo == PAL_LAVA)
                jogo_mensagem(j, "Adjetivo! A lava congelou. Agora dá para pisar!", MSG_BOA);
            else
                jogo_mensagem(j, "Adjetivo! Os espinhos viraram gelo liso e seguro.", MSG_BOA);
        }
        else if (cl == CL_SUBSTANTIVO) {
            jogo_mensagem(j, p->tipo == PAL_LAVA
                ? "A lava já é matéria. Que tal qualificá-la?"
                : "Os espinhos já são matéria. Qualifique-os!", MSG_NEUTRA);
        }
        else {
            jogo_mensagem(j, "Isso não reage a essa classe...", MSG_NEUTRA);
        }
    }
}

/* ENTER: do menu vai para a introdução; da introdução começa o jogo. */
void jogo_confirmar(Jogo* j)
{
    if (j->tela == TELA_MENU)       j->tela = TELA_INTRO;
    else if (j->tela == TELA_INTRO) jogo_comecar(j);
}

/* Clique nos botões das telas de menu. */
static void clique_menu(Jogo* j, float mx, float my)
{
    if (j->tela == TELA_MENU) {
        if (ponto_em(mx, my, BTN_X, (float)BTN_Y(0), BTN_W, BTN_H))      j->tela = TELA_INTRO;
        else if (ponto_em(mx, my, BTN_X, (float)BTN_Y(1), BTN_W, BTN_H)) j->tela = TELA_HISTORIA;
    }
    else if (j->tela == TELA_HISTORIA) {
        if (ponto_em(mx, my, SUB_VOLTAR_X_HIST, SUB_Y, SUB_W, SUB_H)) j->tela = TELA_MENU;
    }
    else if (j->tela == TELA_INTRO) {
        if (ponto_em(mx, my, SUB_VOLTAR_X_INTRO, SUB_Y, SUB_W, SUB_H)) j->tela = TELA_MENU;
        else if (ponto_em(mx, my, SUB_COMECAR_X, SUB_Y, SUB_W, SUB_H)) jogo_comecar(j);
    }
}

void jogo_clique(Jogo* j, float mx, float my)
{
    int slot, idx = 0, alcance = 0, tipo;

    if (j->tela == TELA_MENU || j->tela == TELA_HISTORIA || j->tela == TELA_INTRO) {
        clique_menu(j, mx, my);
        return;
    }
    if (j->tela != TELA_JOGANDO) return;

    slot = jogo_slot_em(mx, my);
    if (slot >= 0) { j->selecionada = slot; return; }

    tipo = jogo_alvo(j, mx, my, &idx, &alcance);
    if (tipo == ALVO_NENHUM) return;
    if (!alcance) {
        jogo_mensagem(j, "Muito longe! Aproxime-se da palavra.", MSG_RUIM);
        return;
    }
    if (tipo == ALVO_INIMIGO) lancar_em_inimigo(j, &j->inimigos[idx]);
    else                      lancar_em_palavra(j, &j->palavras[idx], j->selecionada);
}

/* ------------------------------------------------------------------ */
/* 5. Atualização por frame                                            */
/* ------------------------------------------------------------------ */

static void morrer(Jogo* j, const char* motivo)
{
    Jogador* p = &j->jog;
    p->vidas--;
    pontuar(j, PTS_DANO);
    if (p->vidas <= 0) {
        atualizar_recorde(j);
        j->tela = TELA_DERROTA;
        return;
    }
    p->x = j->check_x; p->y = j->check_y;
    p->vx = p->vy = 0.0f;
    p->invulneravel = 1.2f;
    jogo_mensagem(j, motivo, MSG_RUIM);
}

static void dano_inimigo(Jogo* j, const Inimigo* e)
{
    Jogador* p = &j->jog;
    float dir = (p->x + p->w / 2 < e->x + e->w / 2) ? -1.0f : 1.0f;
    p->vidas--;
    pontuar(j, PTS_DANO);
    if (p->vidas <= 0) {
        atualizar_recorde(j);
        j->tela = TELA_DERROTA;
        return;
    }
    p->invulneravel = 1.6f;
    p->atordoado = 0.25f;
    p->vx = dir * 260.0f;
    p->vy = -300.0f;
    jogo_mensagem(j, "Ai! Uma palavra corrompida te atingiu.", MSG_RUIM);
}

static void atualizar_palavras(Jogo* j, float dt)
{
    Jogador* a = &j->jog;
    int i;

    for (i = 0; i < j->n_palavras; i++) {
        Palavra* p = &j->palavras[i];
        if (p->brilho > 0.0f) p->brilho -= dt;

        if (p->tipo == PAL_PLATAFORMA && p->materializada && p->movendo) {
            float vel = p->lenta ? VEL_PLAT_LENTA : VEL_PLAT;
            float nx = p->x + p->dir * vel * dt, dx;
            int em_cima;

            if (nx >= p->x_max) { nx = p->x_max; p->dir = -1.0f; }
            else if (nx <= p->x_min) { nx = p->x_min; p->dir = 1.0f; }
            dx = nx - p->x;

            em_cima = a->vy >= 0.0f &&
                fabsf((a->y + a->h) - p->y) < 2.5f &&
                a->x + a->w > p->x + 1.0f && a->x < p->x + p->w - 1.0f;

            p->x = nx;
            if (em_cima) {
                a->x += dx;                                 /* carrega o jogador */
            }
            else if (retangulos(a->x, a->y, a->w, a->h, p->x, p->y, p->w, p->h)) {
                a->x = (dx > 0) ? p->x + p->w : p->x - a->w; /* empurra de lado   */
            }
        }
    }
}

static void mover_jogador(Jogo* j, const Entrada* in, float dt)
{
    Jogador* p = &j->jog;
    float alvo, acc;

    p->no_gelo = p->no_chao ? em_gelo(j) : 0;

    if (p->atordoado > 0.0f) {
        p->atordoado -= dt;
        alvo = p->vx;                       /* sem controle durante o tranco */
        acc = 0.0f;
    }
    else {
        alvo = (float)(in->direita - in->esquerda) * VEL_JOGADOR;
        acc = p->no_gelo ? 380.0f : (p->no_chao ? 3200.0f : 2000.0f);
        if (in->direita && !in->esquerda) p->olhando = 1;
        else if (in->esquerda && !in->direita) p->olhando = -1;
    }
    if (p->vx < alvo)      p->vx = MIN(p->vx + acc * dt, alvo);
    else if (p->vx > alvo) p->vx = MAX(p->vx - acc * dt, alvo);

    /* Pulo com "coyote time" e "jump buffer" (mais perdão ao jogador). */
    if (p->no_chao) p->coyote = 0.10f; else p->coyote -= dt;
    if (in->pulo_apertou) p->buffer_pulo = 0.12f; else p->buffer_pulo -= dt;
    if (p->buffer_pulo > 0.0f && p->coyote > 0.0f) {
        p->vy = -FORCA_PULO;
        p->buffer_pulo = 0.0f;
        p->coyote = 0.0f;
        p->no_chao = 0;
    }
    if (!in->pulo_segurando && p->vy < -240.0f) p->vy = -240.0f; /* pulo variável */

    p->vy += GRAVIDADE * dt;
    if (p->vy > VEL_QUEDA_MAX) p->vy = VEL_QUEDA_MAX;

    /* Eixo X */
    p->x += p->vx * dt;
    if (jogo_colide(j, p->x, p->y, p->w, p->h)) {
        float dir = (p->vx > 0.0f) ? 1.0f : -1.0f;
        int n = 0;
        while (jogo_colide(j, p->x, p->y, p->w, p->h) && n++ < 200) p->x -= dir * 0.25f;
        p->vx = 0.0f;
    }

    /* Eixo Y */
    p->no_chao = 0;
    p->y += p->vy * dt;
    if (jogo_colide(j, p->x, p->y, p->w, p->h)) {
        float dir = (p->vy > 0.0f) ? 1.0f : -1.0f;
        int n = 0;
        while (jogo_colide(j, p->x, p->y, p->w, p->h) && n++ < 400) p->y -= dir * 0.25f;
        if (p->vy > 0.0f) p->no_chao = 1;
        p->vy = 0.0f;
    }
}

static void atualizar_inimigos(Jogo* j, float dt)
{
    Jogador* p = &j->jog;
    int i;

    for (i = 0; i < j->n_inimigos; i++) {
        Inimigo* e = &j->inimigos[i];
        if (!e->vivo) continue;

        e->t += dt;
        if (e->flash > 0.0f) e->flash -= dt;

        if (e->tipo == INIM_ESPECTRO) {
            /* Fantasma: fica parado e "pisca" para o outro lado do corredor. */
            e->tele -= dt;
            if (e->tele <= 0.0f) {
                e->tele = 2.4f;
                e->x = (e->x - e->x_min < e->x_max - e->x) ? e->x_max : e->x_min;
            }
        }
        else {
            e->x += e->vx * dt;
            if (e->x < e->x_min) { e->x = e->x_min; e->vx = fabsf(e->vx); }
            else if (e->x > e->x_max) { e->x = e->x_max; e->vx = -fabsf(e->vx); }
        }

        if (e->tipo == INIM_MORCEGO)
            e->y = e->y_base + sinf(e->t * 2.6f) * 34.0f;
        else if (e->tipo == INIM_ESPECTRO)
            e->y = e->y_base + sinf(e->t * 1.7f) * 20.0f;
        else if (e->tipo == INIM_CHEFE)
            e->y = e->y_base - fabsf(sinf(e->t * 3.4f)) * 10.0f;

        if (p->invulneravel <= 0.0f &&
            retangulos(p->x, p->y, p->w, p->h,
                e->x + 4, e->y + 4, e->w - 8, e->h - 8)) {
            dano_inimigo(j, e);
            if (j->tela != TELA_JOGANDO) return;
        }
    }
}

void jogo_atualizar(Jogo* j, const Entrada* in, float dt)
{
    Jogador* p = &j->jog;
    char buf[96];
    int i;

    j->tempo_anim += dt;
    if (j->msg_tempo > 0.0f) j->msg_tempo -= dt;
    if (j->aviso_tempo > 0.0f) j->aviso_tempo -= dt;
    if (j->analise.tempo > 0.0f) j->analise.tempo -= dt; else j->analise.ativa = 0;

    if (j->tela != TELA_JOGANDO) return;

    atualizar_palavras(j, dt);
    mover_jogador(j, in, dt);
    atualizar_inimigos(j, dt);
    if (j->tela != TELA_JOGANDO) return;

    if (p->invulneravel > 0.0f) p->invulneravel -= dt;
    p->tinta = MIN(TINTA_MAX, p->tinta + TINTA_REGEN * dt);

    /* Checkpoints: pisou em chão firme depois da marca */
    for (i = 0; i < j->n_checks; i++) {
        if (p->no_chao && p->x > j->checks[i] && j->check_x < j->checks[i]) {
            j->check_x = j->checks[i] + 4.0f;
            j->check_y = 14 * TILE - p->h - 1.0f;
        }
    }

    /* Perigos: lava e espinhos enquanto não forem congelados */
    for (i = 0; i < j->n_palavras; i++) {
        Palavra* w = &j->palavras[i];
        if (w->tipo == PAL_PLATAFORMA || w->congelada) continue;
        if (w->tipo == PAL_LAVA &&
            retangulos(p->x, p->y, p->w, p->h, w->x, w->y, w->w, w->h)) {
            morrer(j, "A lava te queimou! Use um adjetivo para congelá-la.");
            return;
        }
        if (w->tipo == PAL_ESPINHO &&
            retangulos(p->x, p->y, p->w, p->h, w->x + 4, w->y + 6, w->w - 8, w->h - 6)) {
            morrer(j, "Espinhos! Um adjetivo os transforma em gelo seguro.");
            return;
        }
    }
    if (p->y > MAPA_LINHAS * TILE + 40) {
        morrer(j, "Você caiu! Crie uma ponte com as palavras.");
        return;
    }

    /* Saída da fase: só abre com todos os guardiões restaurados */
    if (retangulos(p->x, p->y, p->w, p->h, j->saida_x, j->saida_y, TILE, 64)) {
        if (j->derrotados >= j->n_inimigos) {
            concluir_fase(j);
            return;
        }
        if (j->msg_tempo <= 0.0f) {
            snprintf(buf, sizeof(buf), "A saída está selada: faltam %d guardiões.",
                j->n_inimigos - j->derrotados);
            jogo_mensagem(j, buf, MSG_RUIM);
        }
    }

    atualizar_camera(j, MIN(1.0f, 8.0f * dt));
}