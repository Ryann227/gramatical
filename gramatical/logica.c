/*
 * logica.c - Regras do Gramatical Tower (sem Allegro).
 *
 * Seções:
 *   1. Dados e utilitários
 *   2. Colisão
 *   3. Montagem da fase
 *   4. Feitiços (Pena Tinteiro)
 *   5. Atualização por frame (palavras, jogador, inimigos)
 */
#define _CRT_SECURE_NO_WARNINGS
#include "jogo.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

 /* ------------------------------------------------------------------ */
 /* 1. Dados e utilitários                                              */
 /* ------------------------------------------------------------------ */

const char* const NOME_CLASSE[CL_TOTAL] = { "Substantivo", "Adjetivo", "Verbo", "Advérbio" };
const char* const NOME_CLASSE_MIN[CL_TOTAL] = { "substantivo", "adjetivo", "verbo", "advérbio" };
const char* const FUNCAO_CLASSE[CL_TOTAL] = { "Nomear", "Qualificar", "Ação", "Modificar" };
const float       CUSTO_CLASSE[CL_TOTAL] = { 20.0f, 15.0f, 15.0f, 10.0f };

#define VEL_JOGADOR     190.0f
#define GRAVIDADE       1500.0f
#define FORCA_PULO      560.0f
#define VEL_QUEDA_MAX   900.0f
#define TINTA_MAX       100.0f
#define TINTA_REGEN     8.0f      /* tinta por segundo */
#define VEL_PLAT        100.0f    /* plataforma móvel normal  */
#define VEL_PLAT_LENTA  35.0f     /* com o advérbio "lentamente" */

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

/* ------------------------------------------------------------------ */
/* 2. Colisão                                                          */
/* ------------------------------------------------------------------ */

int palavra_solida(const Palavra* p)
{
    if (p->tipo == PAL_PLATAFORMA) return p->materializada;
    return p->congelada;            /* lava só é sólida depois de congelada */
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
/* 3. Montagem da fase                                                 */
/* ------------------------------------------------------------------ */

static void bloco(Jogo* j, int c0, int c1, int l0, int l1)
{
    int c, l;
    for (l = l0; l <= l1; l++)
        for (c = c0; c <= c1; c++)
            j->mapa[l][c] = '#';
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

static void novo_inimigo(Jogo* j, const char* texto, int classe,
    float x, float y, float x_min, float x_max,
    float vx, int voador)
{
    Inimigo* e;
    if (j->n_inimigos >= MAX_INIMIGOS) return;
    e = &j->inimigos[j->n_inimigos++];
    memset(e, 0, sizeof(*e));
    snprintf(e->texto, sizeof(e->texto), "%s", texto);
    e->classe = classe;
    e->w = 36.0f; e->h = 34.0f;
    e->x = x; e->y = y; e->y_base = y;
    e->x_min = x_min; e->x_max = x_max; e->vx = vx;
    e->voador = voador; e->vivo = 1;
}

static void atualizar_camera(Jogo* j, float suavizar)
{
    float alvo = j->jog.x + j->jog.w / 2 - LARGURA_TELA / 2;
    alvo = CLAMP(alvo, 0.0f, (float)(MAPA_COLS * TILE - LARGURA_TELA));
    j->cam_x += (alvo - j->cam_x) * suavizar;
}

/* Fase 1: "Biblioteca de Sintaxia".
 *   cols  0-11  chão inicial
 *   cols 12-17  poço  -> palavra "plataforma" (substantivo)
 *   cols 18-27  chão  -> guardião "casa"
 *   cols 28-39  poço largo -> "plataforma" que precisa se mover (verbo)
 *   cols 40-47  chão  -> guardião "pontes"
 *   cols 48-53  lava  -> congelar com adjetivo
 *   cols 54-69  chão  -> 3 guardiões e a saída
 */
void jogo_iniciar(Jogo* j)
{
    int l, c;
    Jogador* p;

    memset(j, 0, sizeof(*j));
    j->tela = TELA_MENU;

    for (l = 0; l < MAPA_LINHAS; l++)
        for (c = 0; c < MAPA_COLS; c++)
            j->mapa[l][c] = '.';

    bloco(j, 0, 11, 14, 16);
    bloco(j, 18, 27, 14, 16);
    bloco(j, 40, 47, 14, 16);
    bloco(j, 54, 69, 14, 16);
    bloco(j, 43, 44, 12, 13);          /* degrau para pular */

    /* Palavras do cenário */
    nova_palavra(j, PAL_PLATAFORMA, "plataforma",
        13 * TILE, 14 * TILE, 4 * TILE, TILE,
        13 * TILE, 13 * TILE);                   /* sem movimento */
    nova_palavra(j, PAL_PLATAFORMA, "plataforma",
        29 * TILE, 14 * TILE, 4 * TILE, TILE,
        29 * TILE, 35 * TILE);                   /* vai e volta   */
    nova_palavra(j, PAL_LAVA, "lava",
        48 * TILE, 14 * TILE + 16, 6 * TILE, 80,
        0, 0);

    /* Guardiões corrompidos (palavra, classe correta, posição...) */
    novo_inimigo(j, "casa", CL_SUBSTANTIVO, 700.0f, 14 * TILE - 34.0f,
        19 * TILE, 26 * TILE, 50.0f, 0);
    novo_inimigo(j, "pontes", CL_SUBSTANTIVO, 1300.0f, 14 * TILE - 34.0f,
        1290.0f, 1336.0f, 30.0f, 0);
    novo_inimigo(j, "veloz", CL_ADJETIVO, 1800.0f, 14 * TILE - 34.0f,
        55 * TILE, 59 * TILE, 70.0f, 0);
    novo_inimigo(j, "destruiu", CL_VERBO, 1900.0f, 14 * TILE - 130.0f,
        56 * TILE, 62 * TILE, 60.0f, 1);
    novo_inimigo(j, "lentamente", CL_ADVERBIO, 2000.0f, 14 * TILE - 34.0f,
        61 * TILE, 65 * TILE, 40.0f, 0);

    p = &j->jog;
    p->w = 20.0f; p->h = 28.0f;
    p->x = 2 * TILE;
    p->y = 14 * TILE - p->h - 1.0f;
    p->olhando = 1;
    p->vidas = 3;
    p->tinta = TINTA_MAX;
    j->check_x = p->x;
    j->check_y = p->y;

    j->selecionada = CL_SUBSTANTIVO;
    atualizar_camera(j, 1.0f);
}

void jogo_comecar(Jogo* j)
{
    j->tela = TELA_JOGANDO;
    jogo_mensagem(j, "Clique numa palavra com a classe gramatical certa!", MSG_NEUTRA);
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

static void lancar_em_inimigo(Jogo* j, Inimigo* e)
{
    int cl = j->selecionada;
    char buf[128];

    if (!gastar(j, cl)) return;

    if (cl == e->classe) {
        e->vivo = 0;
        j->derrotados++;
        j->jog.tinta = MIN(TINTA_MAX, j->jog.tinta + 12.0f);   /* recompensa */
        j->analise.ativa = 1;
        j->analise.classe = cl;
        j->analise.tempo = 4.5f;
        snprintf(j->analise.texto, sizeof(j->analise.texto), "%s", e->texto);
        snprintf(buf, sizeof(buf), "\"%s\" é um %s! Palavra restaurada.",
            e->texto, NOME_CLASSE_MIN[cl]);
        jogo_mensagem(j, buf, MSG_BOA);
    }
    else {
        e->flash = 0.45f;
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
            jogo_mensagem(j, "Substantivo! A palavra virou matéria.", MSG_BOA);
        }
        else if (!p->materializada) {
            jogo_mensagem(j, "Ainda não há matéria. Comece com um substantivo!", MSG_RUIM);
        }
        else if (cl == CL_ADJETIVO) {
            if (p->congelada) { jogo_mensagem(j, "Já está congelada.", MSG_NEUTRA); return; }
            if (!gastar(j, cl)) return;
            p->congelada = 1; p->brilho = 0.6f;
            jogo_mensagem(j, "Adjetivo! A plataforma congelou (escorrega!).", MSG_BOA);
        }
        else if (cl == CL_VERBO) {
            if (p->x_max <= p->x_min) { jogo_mensagem(j, "Essa plataforma não tem para onde se mover.", MSG_NEUTRA); return; }
            if (p->movendo) { jogo_mensagem(j, "Já está em movimento.", MSG_NEUTRA); return; }
            if (!gastar(j, cl)) return;
            p->movendo = 1; p->brilho = 0.6f;
            jogo_mensagem(j, "Verbo! A plataforma ganhou movimento.", MSG_BOA);
        }
        else { /* CL_ADVERBIO */
            if (!p->movendo) { jogo_mensagem(j, "Um advérbio precisa de um verbo para modificar!", MSG_RUIM); return; }
            if (p->lenta) { jogo_mensagem(j, "Já está lenta.", MSG_NEUTRA); return; }
            if (!gastar(j, cl)) return;
            p->lenta = 1; p->brilho = 0.6f;
            jogo_mensagem(j, "Advérbio! Ela agora se move lentamente.", MSG_BOA);
        }
    }
    else { /* PAL_LAVA */
        if (cl == CL_ADJETIVO) {
            if (p->congelada) { jogo_mensagem(j, "A lava já está congelada.", MSG_NEUTRA); return; }
            if (!gastar(j, cl)) return;
            p->congelada = 1; p->brilho = 0.6f;
            p->y = 14 * TILE; p->h = 3 * TILE;          /* vira bloco de gelo */
            jogo_mensagem(j, "Adjetivo! A lava congelou. Agora dá para pisar!", MSG_BOA);
        }
        else if (cl == CL_SUBSTANTIVO) {
            jogo_mensagem(j, "A lava já é matéria. Que tal qualificá-la?", MSG_NEUTRA);
        }
        else {
            jogo_mensagem(j, "A lava não reage a essa classe...", MSG_NEUTRA);
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
        if (ponto_em(mx, my, BTN_X, (float)BTN_Y(0), BTN_W, BTN_H))      j->tela = TELA_INTRO;      /* JOGAR    */
        else if (ponto_em(mx, my, BTN_X, (float)BTN_Y(1), BTN_W, BTN_H)) j->tela = TELA_HISTORIA;   /* HISTÓRIA */
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
    if (p->vidas <= 0) { j->tela = TELA_DERROTA; return; }
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
    if (p->vidas <= 0) { j->tela = TELA_DERROTA; return; }
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

        e->x += e->vx * dt;
        if (e->x < e->x_min) { e->x = e->x_min; e->vx = fabsf(e->vx); }
        else if (e->x > e->x_max) { e->x = e->x_max; e->vx = -fabsf(e->vx); }
        if (e->voador) e->y = e->y_base + sinf(e->t * 2.2f) * 28.0f;

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
    static const float checkpoints[] = { 19.0f * TILE, 40.0f * TILE, 54.0f * TILE };
    Jogador* p = &j->jog;
    int i;

    j->tempo_anim += dt;
    if (j->msg_tempo > 0.0f) j->msg_tempo -= dt;
    if (j->analise.tempo > 0.0f) j->analise.tempo -= dt; else j->analise.ativa = 0;

    if (j->tela != TELA_JOGANDO) return;
    j->tempo_total += dt;

    atualizar_palavras(j, dt);
    mover_jogador(j, in, dt);
    atualizar_inimigos(j, dt);
    if (j->tela != TELA_JOGANDO) return;

    if (p->invulneravel > 0.0f) p->invulneravel -= dt;
    p->tinta = MIN(TINTA_MAX, p->tinta + TINTA_REGEN * dt);

    /* Checkpoints: pisou em chão firme depois da marca */
    for (i = 0; i < 3; i++) {
        if (p->no_chao && p->x > checkpoints[i] && j->check_x < checkpoints[i]) {
            j->check_x = checkpoints[i] + 4.0f;
            j->check_y = 14 * TILE - p->h - 1.0f;
        }
    }

    /* Lava (enquanto não congelada) e queda nos poços */
    for (i = 0; i < j->n_palavras; i++) {
        Palavra* w = &j->palavras[i];
        if (w->tipo == PAL_LAVA && !w->congelada &&
            retangulos(p->x, p->y, p->w, p->h, w->x, w->y, w->w, w->h)) {
            morrer(j, "A lava te queimou! Use um adjetivo para congelá-la.");
            return;
        }
    }
    if (p->y > MAPA_LINHAS * TILE + 40) {
        morrer(j, "Você caiu! Crie uma ponte com as palavras.");
        return;
    }

    /* Saída da torre */
    if (retangulos(p->x, p->y, p->w, p->h, 67 * TILE, 14 * TILE - 64, TILE, 64)) {
        j->tela = TELA_VITORIA;
        return;
    }

    atualizar_camera(j, MIN(1.0f, 8.0f * dt));
}