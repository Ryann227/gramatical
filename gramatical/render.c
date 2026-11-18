/*
 * render.c - Desenho do Gramatical Tower.
 *
 * Tudo é desenhado com primitivas (retângulos, círculos, linhas), então o
 * jogo roda sem nenhum arquivo de imagem. Quando o grupo tiver sprites,
 * basta trocar o corpo das funções desenha_* por al_draw_bitmap.
 */
#define _CRT_SECURE_NO_WARNINGS
#include "render.h"

#include <allegro5/allegro_primitives.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

 /* ------------------------------------------------------------------ */
 /* Acentos: o Visual Studio pode gravar os textos do código em Windows-1252
  * em vez de UTF-8. O Allegro só lê UTF-8 e PARA de desenhar no primeiro
  * acento ("HISTÓRIA" vira "HIST"). A função u8() confere o texto: se já for
  * UTF-8 válido, usa como está; se não, converte de Windows-1252 para UTF-8.
  * Os #define logo abaixo fazem todo texto deste arquivo passar por ela.   */
  /* ------------------------------------------------------------------ */
static int utf8_valido(const char* s)
{
    const unsigned char* p = (const unsigned char*)s;
    while (*p) {
        int extras;
        if (*p < 0x80) { p++; continue; }
        if ((*p & 0xE0) == 0xC0) extras = 1;
        else if ((*p & 0xF0) == 0xE0) extras = 2;
        else if ((*p & 0xF8) == 0xF0) extras = 3;
        else return 0;
        p++;
        while (extras-- > 0) {
            if ((*p & 0xC0) != 0x80) return 0;
            p++;
        }
    }
    return 1;
}

static const char* u8(const char* s)
{
    static char bufs[4][512];            /* vários buffers: usos seguidos não se atropelam */
    static int usado = 0;
    char* dst;
    int i, n = 0;

    if (utf8_valido(s)) return s;
    dst = bufs[usado];
    usado = (usado + 1) % 4;
    for (i = 0; s[i] != '\0' && n < 508; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 0x80) {
            dst[n++] = (char)ch;
        }
        else {                         /* Latin-1/Windows-1252 -> UTF-8 (2 bytes) */
            dst[n++] = (char)(0xC0 | (ch >> 6));
            dst[n++] = (char)(0x80 | (ch & 0x3F));
        }
    }
    dst[n] = '\0';
    return dst;
}

#define al_draw_text(f, c, x, y, fl, t) al_draw_text((f), (c), (x), (y), (fl), u8(t))
#define al_get_text_width(f, t) al_get_text_width((f), u8(t))
#define al_draw_multiline_text(f, c, x, y, w, lh, fl, t) \
        al_draw_multiline_text((f), (c), (x), (y), (w), (lh), (fl), u8(t))

/* ------------------------------------------------------------------ */
/* Cores e helpers                                                     */
/* ------------------------------------------------------------------ */

static ALLEGRO_COLOR C(int r, int g, int b) { return al_map_rgb(r, g, b); }

/* Cor com transparência. O Allegro 5 usa alfa pré-multiplicado por padrão,
 * então multiplicamos RGB pelo alfa antes de criar a cor. */
static ALLEGRO_COLOR CA(int r, int g, int b, int a)
{
    return al_map_rgba(r * a / 255, g * a / 255, b * a / 255, a);
}

static ALLEGRO_COLOR cor_classe(int cl)
{
    switch (cl) {
    case CL_SUBSTANTIVO: return C(206, 112, 48);
    case CL_ADJETIVO:    return C(58, 124, 220);
    case CL_VERBO:       return C(64, 176, 88);
    default:             return C(138, 88, 204);
    }
}

static void texto(const ALLEGRO_FONT* f, ALLEGRO_COLOR c, float x, float y, int flags, const char* s)
{
    al_draw_text((ALLEGRO_FONT*)f, c, x, y, flags, s);
}

static void texto_sombra(const ALLEGRO_FONT* f, ALLEGRO_COLOR c, float x, float y, int flags, const char* s)
{
    al_draw_text((ALLEGRO_FONT*)f, C(0, 0, 0), x + 1, y + 1, flags, s);
    al_draw_text((ALLEGRO_FONT*)f, c, x, y, flags, s);
}

static void maiusculas(char* dst, size_t n, const char* src)
{
    size_t i;
    for (i = 0; i + 1 < n && src[i]; i++) dst[i] = (char)toupper((unsigned char)src[i]);
    dst[i] = '\0';
}

static float centro_y(const ALLEGRO_FONT* f, float y, float h)
{
    return y + (h - al_get_font_line_height((ALLEGRO_FONT*)f)) / 2.0f;
}

static void desenha_coracao(float x, float y, float s, ALLEGRO_COLOR c)
{
    al_draw_filled_circle(x - s * 0.25f, y, s * 0.28f, c);
    al_draw_filled_circle(x + s * 0.25f, y, s * 0.28f, c);
    al_draw_filled_triangle(x - s * 0.52f, y + s * 0.08f, x + s * 0.52f, y + s * 0.08f,
        x, y + s * 0.62f, c);
}

/* Ícone redondo de cada classe gramatical (igual ao One Sheet). */
static void desenha_icone(int cl, float cx, float cy, float r)
{
    ALLEGRO_COLOR branco = C(250, 245, 230);
    int k;

    al_draw_filled_circle(cx, cy, r, cor_classe(cl));
    al_draw_circle(cx, cy, r, C(30, 24, 20), 2.0f);

    switch (cl) {
    case CL_SUBSTANTIVO:   /* cubo */
        al_draw_rectangle(cx - r * 0.42f, cy - r * 0.32f, cx + r * 0.32f, cy + r * 0.42f, branco, 1.8f);
        al_draw_line(cx - r * 0.42f, cy - r * 0.32f, cx - r * 0.18f, cy - r * 0.56f, branco, 1.8f);
        al_draw_line(cx + r * 0.32f, cy - r * 0.32f, cx + r * 0.56f, cy - r * 0.56f, branco, 1.8f);
        al_draw_line(cx - r * 0.18f, cy - r * 0.56f, cx + r * 0.56f, cy - r * 0.56f, branco, 1.8f);
        al_draw_line(cx + r * 0.56f, cy - r * 0.56f, cx + r * 0.56f, cy + r * 0.18f, branco, 1.8f);
        break;
    case CL_ADJETIVO:      /* floco de neve */
        for (k = 0; k < 3; k++) {
            float a = k * 3.14159265f / 3.0f;
            al_draw_line(cx - cosf(a) * r * 0.6f, cy - sinf(a) * r * 0.6f,
                cx + cosf(a) * r * 0.6f, cy + sinf(a) * r * 0.6f, branco, 1.8f);
        }
        break;
    case CL_VERBO:         /* seta de ação */
        al_draw_filled_triangle(cx - r * 0.35f, cy - r * 0.55f, cx - r * 0.35f, cy + r * 0.55f,
            cx + r * 0.6f, cy, branco);
        break;
    default:               /* ampulheta */
        al_draw_filled_triangle(cx - r * 0.45f, cy - r * 0.55f, cx + r * 0.45f, cy - r * 0.55f, cx, cy, branco);
        al_draw_filled_triangle(cx - r * 0.45f, cy + r * 0.55f, cx + r * 0.45f, cy + r * 0.55f, cx, cy, branco);
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Mundo                                                               */
/* ------------------------------------------------------------------ */

static void desenha_fundo(const Jogo* j)
{
    float off = fmodf(j->cam_x * 0.35f, 220.0f);
    int i;
    al_clear_to_color(C(14, 16, 26));
    for (i = -1; i < 6; i++) {
        float x = i * 220.0f - off;
        al_draw_filled_rectangle(x + 40, 130, x + 160, ALTURA_TELA, C(20, 23, 38));
        al_draw_filled_circle(x + 100, 130, 60, C(20, 23, 38));
        al_draw_filled_rectangle(x + 78, 150, x + 122, 300, C(26, 30, 50));
        al_draw_filled_circle(x + 100, 150, 22, C(26, 30, 50));
    }
}

static void desenha_tiles(const Jogo* j)
{
    int c0 = (int)(j->cam_x / TILE) - 1, c1 = (int)((j->cam_x + LARGURA_TELA) / TILE) + 1;
    int c, l;
    if (c0 < 0) c0 = 0;
    if (c1 >= MAPA_COLS) c1 = MAPA_COLS - 1;

    for (l = 0; l < MAPA_LINHAS; l++) {
        for (c = c0; c <= c1; c++) {
            float x, y;
            if (j->mapa[l][c] != '#') continue;
            x = (float)(c * TILE); y = (float)(l * TILE);
            al_draw_filled_rectangle(x, y, x + TILE, y + TILE, C(52, 56, 74));
            al_draw_rectangle(x + 0.5f, y + 0.5f, x + TILE - 0.5f, y + TILE - 0.5f, C(30, 32, 46), 1.0f);
            al_draw_line(x, y + TILE / 2, x + TILE, y + TILE / 2, C(36, 38, 54), 1.0f);
            al_draw_line(x + ((l % 2) ? 8 : 20), y, x + ((l % 2) ? 8 : 20), y + TILE / 2, C(36, 38, 54), 1.0f);
            if (l == 0 || j->mapa[l - 1][c] != '#')
                al_draw_filled_rectangle(x, y, x + TILE, y + 4, C(92, 98, 124));
        }
    }
}

static void desenha_tochas(const Jogo* j)
{
    static const int cols[] = { 9, 20, 42, 56, 65 };
    int i;
    for (i = 0; i < 5; i++) {
        float x = cols[i] * TILE + 16.0f, y = 14 * TILE - 26.0f;
        float f = sinf(j->tempo_anim * 9.0f + i * 1.7f);
        al_draw_filled_rectangle(x - 3, y, x + 3, y + 26, C(70, 50, 34));
        al_draw_filled_circle(x, y - 4, 22 + f * 2, CA(255, 150, 40, 40));
        al_draw_filled_circle(x, y - 3, 6 + f, C(255, 160, 40));
        al_draw_filled_circle(x, y - 4, 3, C(255, 230, 140));
    }
}

static void desenha_saida(const Jogo* j, const Fontes* f)
{
    float x = 67 * TILE, y = 14 * TILE - 64.0f;
    float pulso = 0.5f + 0.5f * sinf(j->tempo_anim * 3.0f);
    al_draw_filled_circle(x + 16, y + 30, 46, CA(255, 210, 90, (int)(30 + pulso * 40)));
    al_draw_filled_rounded_rectangle(x - 4, y, x + TILE + 4, y + 64, 14, 14, C(120, 90, 40));
    al_draw_filled_rounded_rectangle(x, y + 4, x + TILE, y + 64, 12, 12, C(18, 14, 30));
    al_draw_filled_circle(x + 16, y + 38, 8, CA(255, 220, 120, (int)(90 + pulso * 120)));
    texto_sombra(f->pequena, C(255, 225, 140), x + 16, y - 18, ALLEGRO_ALIGN_CENTRE, "SAÍDA");
}

static void desenha_chips(const Palavra* p, const Fontes* f)
{
    float cx = p->x + 6, cy = p->y - 10;
    (void)f;
    if (p->materializada && p->tipo == PAL_PLATAFORMA) {
        al_draw_filled_circle(cx, cy, 5, cor_classe(CL_SUBSTANTIVO)); cx += 14;
    }
    if (p->congelada) { al_draw_filled_circle(cx, cy, 5, cor_classe(CL_ADJETIVO)); cx += 14; }
    if (p->movendo) { al_draw_filled_circle(cx, cy, 5, cor_classe(CL_VERBO));    cx += 14; }
    if (p->lenta) { al_draw_filled_circle(cx, cy, 5, cor_classe(CL_ADVERBIO)); }
}

static void desenha_palavra(const Jogo* j, const Palavra* p, const Fontes* f)
{
    float x = p->x, y = p->y, w = p->w, h = p->h;
    float ty = centro_y(f->normal, y, h);
    int k;

    if (p->tipo == PAL_PLATAFORMA) {
        if (!p->materializada) {                 /* palavra dormente */
            al_draw_filled_rectangle(x, y, x + w, y + h, CA(90, 160, 255, 28));
            al_draw_rectangle(x, y, x + w, y + h, CA(120, 190, 255, 170), 1.5f);
            texto(f->normal, C(160, 205, 255), x + w / 2, ty, ALLEGRO_ALIGN_CENTRE, p->texto);
        }
        else if (!p->congelada) {              /* matéria */
            al_draw_filled_rectangle(x - 3, y - 3, x + w + 3, y + h + 3, CA(80, 190, 255, 45));
            al_draw_filled_rectangle(x, y, x + w, y + h, C(36, 64, 140));
            al_draw_rectangle(x + 1, y + 1, x + w - 1, y + h - 1, C(90, 200, 255), 2.0f);
            texto(f->normal, C(235, 245, 255), x + w / 2, ty, ALLEGRO_ALIGN_CENTRE, p->texto);
        }
        else {                                 /* congelada */
            al_draw_filled_rectangle(x, y, x + w, y + h, C(160, 220, 255));
            al_draw_rectangle(x + 1, y + 1, x + w - 1, y + h - 1, C(255, 255, 255), 2.0f);
            for (k = 0; k < 6; k++) {
                float ix = x + 10 + k * ((w - 20) / 5.0f);
                al_draw_filled_triangle(ix - 5, y + h, ix + 5, y + h, ix, y + h + 8 + (k % 2) * 4, C(200, 240, 255));
            }
            texto(f->normal, C(20, 50, 100), x + w / 2, ty, ALLEGRO_ALIGN_CENTRE, p->texto);
        }
        if (p->movendo) {                        /* setinhas laterais */
            al_draw_filled_triangle(x - 12, y + h / 2, x - 4, y + h / 2 - 6, x - 4, y + h / 2 + 6, C(110, 230, 130));
            al_draw_filled_triangle(x + w + 12, y + h / 2, x + w + 4, y + h / 2 - 6, x + w + 4, y + h / 2 + 6, C(110, 230, 130));
        }
    }
    else {                                     /* lava */
        if (!p->congelada) {
            al_draw_filled_rectangle(x, y, x + w, y + h, C(190, 56, 18));
            for (k = 0; k < 5; k++) {
                float ly = y + 8 + k * 14 + sinf(j->tempo_anim * 3.0f + k) * 2;
                al_draw_line(x, ly, x + w, ly, CA(255, 170, 50, 120), 2.0f);
            }
            for (k = 0; k < 8; k++) {
                float bx = x + 12 + k * (w - 24) / 7.0f;
                float by = y + 4 + fabsf(sinf(j->tempo_anim * 2.0f + k * 1.3f)) * 10;
                al_draw_filled_circle(bx, by, 3, C(255, 190, 70));
            }
            texto_sombra(f->normal, C(255, 235, 200), x + w / 2, centro_y(f->normal, y, h), ALLEGRO_ALIGN_CENTRE, p->texto);
        }
        else {
            al_draw_filled_rectangle(x, y, x + w, y + h, C(140, 205, 245));
            al_draw_rectangle(x + 1, y + 1, x + w - 1, y + h - 1, C(255, 255, 255), 2.0f);
            al_draw_line(x + 20, y + 10, x + 70, y + 60, CA(255, 255, 255, 130), 1.5f);
            al_draw_line(x + w - 40, y + 6, x + w - 90, y + 70, CA(255, 255, 255, 130), 1.5f);
            texto(f->normal, C(20, 50, 100), x + w / 2, centro_y(f->normal, y, 32), ALLEGRO_ALIGN_CENTRE, p->texto);
        }
    }
    if (p->brilho > 0.0f)
        al_draw_filled_rectangle(x - 6, y - 6, x + w + 6, y + h + 6, CA(255, 255, 255, (int)(p->brilho * 140)));
    desenha_chips(p, f);
}

static void desenha_inimigo(const Jogo* j, const Inimigo* e, const Fontes* f)
{
    float x = e->x, y = e->y, w = e->w, h = e->h;
    float px = j->jog.x + j->jog.w / 2 - (x + w / 2);
    float olhar = (px > 0) ? 2.0f : -2.0f;
    char rotulo[32];
    float tw, tx;
    int k;

    for (k = 0; k < 3; k++) {                    /* perninhas de aranha-livro */
        float ph = sinf(e->t * 8.0f + k * 1.6f) * 3.0f;
        al_draw_line(x + 4, y + h - 8 + k * 3, x - 8, y + h + 2 + ph, C(70, 30, 100), 2.0f);
        al_draw_line(x + w - 4, y + h - 8 + k * 3, x + w + 8, y + h + 2 - ph, C(70, 30, 100), 2.0f);
    }
    al_draw_filled_rounded_rectangle(x, y, x + w, y + h, 10, 10, C(150, 72, 192));
    al_draw_rounded_rectangle(x, y, x + w, y + h, 10, 10, C(80, 36, 110), 2.0f);
    al_draw_line(x + w / 2, y + 2, x + w / 2, y + h - 2, C(110, 50, 150), 2.0f);   /* lombada */
    al_draw_filled_circle(x + 11, y + 14, 6, C(250, 250, 250));
    al_draw_filled_circle(x + w - 11, y + 14, 6, C(250, 250, 250));
    al_draw_filled_circle(x + 11 + olhar, y + 15, 3, C(30, 10, 40));
    al_draw_filled_circle(x + w - 11 + olhar, y + 15, 3, C(30, 10, 40));
    al_draw_line(x + 10, y + 26, x + w - 10, y + 26, C(60, 20, 80), 2.0f);

    maiusculas(rotulo, sizeof(rotulo), e->texto);
    tw = (float)al_get_text_width((ALLEGRO_FONT*)f->pequena, rotulo);
    tx = x + w / 2 - tw / 2;
    al_draw_filled_rounded_rectangle(tx - 5, y - 20, tx + tw + 5, y - 4, 4, 4, CA(30, 8, 12, 190));
    al_draw_rounded_rectangle(tx - 5, y - 20, tx + tw + 5, y - 4, 4, 4, C(200, 60, 60), 1.0f);
    texto(f->pequena, C(255, 100, 100), x + w / 2, y - 20, ALLEGRO_ALIGN_CENTRE, rotulo);

    if (e->flash > 0.0f)
        al_draw_filled_rounded_rectangle(x, y, x + w, y + h, 10, 10, CA(255, 80, 80, (int)(e->flash / 0.45f * 200)));
}

static void desenha_jogador(const Jogo* j)
{
    const Jogador* p = &j->jog;
    float cx = p->x + p->w / 2, cy = p->y + p->h / 2;
    float mx = j->mouse_x + j->cam_x - cx, my = j->mouse_y - cy;
    float len = sqrtf(mx * mx + my * my), pas;
    float hx, hy, ponta_x, ponta_y;

    if (p->invulneravel > 0.0f && ((int)(j->tempo_anim * 14.0f)) % 2) return;   /* piscar */

    pas = (p->no_chao && fabsf(p->vx) > 20.0f) ? sinf(j->tempo_anim * 16.0f) * 4.0f : 0.0f;
    al_draw_line(cx - 4, p->y + p->h - 6, cx - 4 + pas, p->y + p->h, C(40, 30, 30), 3.0f);   /* pernas */
    al_draw_line(cx + 4, p->y + p->h - 6, cx + 4 - pas, p->y + p->h, C(40, 30, 30), 3.0f);
    al_draw_filled_rounded_rectangle(p->x, p->y + 10, p->x + p->w, p->y + p->h - 4, 4, 4, C(46, 70, 132));   /* capa */
    al_draw_filled_rectangle(p->x + 2, p->y + 11, p->x + p->w - 2, p->y + 15, C(80, 120, 210));              /* cachecol */
    al_draw_filled_circle(cx, p->y + 7, 7, C(240, 200, 160));                                                /* cabeça */
    al_draw_filled_rectangle(cx - 7, p->y, cx + 7, p->y + 5, C(92, 60, 40));                                 /* cabelo */
    al_draw_filled_circle(cx + 3 * p->olhando, p->y + 8, 1.5f, C(30, 20, 20));                               /* olho */

    if (len < 1.0f) { mx = (float)p->olhando; my = 0.0f; len = 1.0f; }
    hx = cx + 4 * p->olhando; hy = cy + 2;
    ponta_x = hx + mx / len * 18.0f; ponta_y = hy + my / len * 18.0f;
    al_draw_line(hx, hy, ponta_x, ponta_y, C(150, 110, 70), 2.5f);                                            /* Pena Tinteiro */
    al_draw_filled_circle(ponta_x, ponta_y, 5, CA(120, 220, 255, 70));
    al_draw_filled_circle(ponta_x, ponta_y, 2.5f, C(190, 245, 255));
}

/* ------------------------------------------------------------------ */
/* HUD                                                                 */
/* ------------------------------------------------------------------ */

static void desenha_hud(const Jogo* j, const Fontes* f)
{
    const Jogador* p = &j->jog;
    char buf[96];
    int i;

    /* Vidas e tinta */
    for (i = 0; i < 3; i++)
        desenha_coracao(26.0f + i * 28, 24, 24, i < p->vidas ? C(220, 50, 60) : C(70, 40, 50));
    al_draw_filled_rectangle(14, 44, 14 + 150, 56, C(20, 24, 40));
    al_draw_filled_rectangle(14, 44, 14 + 150 * (p->tinta / 100.0f), 56, C(60, 130, 255));
    al_draw_rectangle(14, 44, 164, 56, C(180, 190, 220), 1.5f);
    texto_sombra(f->pequena, C(220, 230, 255), 170, 42, ALLEGRO_ALIGN_LEFT, "tinta");

    /* Progresso */
    snprintf(buf, sizeof(buf), "Palavras restauradas: %d/%d", j->derrotados, j->n_inimigos);
    texto_sombra(f->pequena, C(235, 225, 200), LARGURA_TELA - 14, 14, ALLEGRO_ALIGN_RIGHT, buf);
    snprintf(buf, sizeof(buf), "Tempo: %d:%02d", (int)j->tempo_total / 60, (int)j->tempo_total % 60);
    texto_sombra(f->pequena, C(235, 225, 200), LARGURA_TELA - 14, 32, ALLEGRO_ALIGN_RIGHT, buf);

    /* Barra de feitiços */
    for (i = 0; i < CL_TOTAL; i++) {
        float x = (float)SLOT_X(i), y = (float)SLOT_Y;
        int sel = (i == j->selecionada);
        al_draw_filled_rounded_rectangle(x, y, x + SLOT_W, y + SLOT_H, 8, 8, CA(24, 20, 34, 220));
        al_draw_rounded_rectangle(x, y, x + SLOT_W, y + SLOT_H, 8, 8,
            sel ? C(255, 210, 90) : C(110, 100, 130), sel ? 3.0f : 1.2f);
        desenha_icone(i, x + 24, y + 26, 15);
        snprintf(buf, sizeof(buf), "%d", i + 1);
        texto(f->pequena, C(200, 190, 170), x + 5, y + 2, ALLEGRO_ALIGN_LEFT, buf);
        texto(f->pequena, sel ? C(255, 230, 150) : C(225, 220, 235), x + 44, y + 9, ALLEGRO_ALIGN_LEFT, NOME_CLASSE[i]);
        snprintf(buf, sizeof(buf), "tinta %d", (int)CUSTO_CLASSE[i]);
        texto(f->pequena, C(140, 160, 210), x + 44, y + 26, ALLEGRO_ALIGN_LEFT, buf);
    }

    /* Mensagem de feedback */
    if (j->msg_tempo > 0.0f && j->msg[0]) {
        int a = j->msg_tempo < 0.6f ? (int)(j->msg_tempo / 0.6f * 255) : 255;
        ALLEGRO_COLOR cor = j->msg_cor == MSG_BOA ? CA(150, 235, 160, a)
            : j->msg_cor == MSG_RUIM ? CA(255, 130, 120, a)
            : CA(240, 240, 240, a);
        float tw = (float)al_get_text_width((ALLEGRO_FONT*)f->normal, j->msg);
        al_draw_filled_rounded_rectangle(LARGURA_TELA / 2 - tw / 2 - 12, 68, LARGURA_TELA / 2 + tw / 2 + 12, 96, 8, 8, CA(10, 10, 20, a * 170 / 255));
        texto(f->normal, cor, LARGURA_TELA / 2, centro_y(f->normal, 68, 28), ALLEGRO_ALIGN_CENTRE, j->msg);
    }

    /* Painel ANÁLISE (feedback imediato depois de acertar um guardião) */
    if (j->analise.ativa) {
        float x = LARGURA_TELA - 238, y = 108, w = 224, h = 122;
        int a = j->analise.tempo < 0.8f ? (int)(j->analise.tempo / 0.8f * 255) : 255;
        float tw;
        al_draw_filled_rounded_rectangle(x, y, x + w, y + h, 6, 6, CA(232, 222, 196, a));
        al_draw_rounded_rectangle(x, y, x + w, y + h, 6, 6, CA(120, 90, 60, a), 2.0f);
        texto(f->normal, CA(70, 50, 30, a), x + w / 2, y + 6, ALLEGRO_ALIGN_CENTRE, "ANÁLISE");
        al_draw_line(x + 14, y + 30, x + w - 14, y + 30, CA(120, 90, 60, a), 1.0f);
        al_draw_filled_rounded_rectangle(x + 24, y + 38, x + w - 24, y + 66, 5, 5, CA(32, 56, 110, a));
        texto(f->normal, CA(240, 245, 255, a), x + w / 2, centro_y(f->normal, y + 38, 28), ALLEGRO_ALIGN_CENTRE, j->analise.texto);
        snprintf(buf, sizeof(buf), "%s", NOME_CLASSE[j->analise.classe]);
        tw = (float)al_get_text_width((ALLEGRO_FONT*)f->normal, buf);
        al_draw_filled_circle(x + 24, y + 84, 7, cor_classe(j->analise.classe));
        texto(f->normal, CA(60, 40, 30, a), x + 38, y + 74, ALLEGRO_ALIGN_LEFT, buf);
        (void)tw;
        snprintf(buf, sizeof(buf), "Função: %s", FUNCAO_CLASSE[j->analise.classe]);
        texto(f->pequena, CA(80, 60, 40, a), x + 24, y + 98, ALLEGRO_ALIGN_LEFT, buf);
    }
}

static void desenha_tooltip(const Jogo* j, const Fontes* f)
{
    int idx = 0, alcance = 0, tipo;
    char buf[96];
    float tw, x, y;

    tipo = jogo_alvo(j, j->mouse_x, j->mouse_y, &idx, &alcance);
    if (tipo == ALVO_NENHUM || jogo_slot_em(j->mouse_x, j->mouse_y) >= 0) return;

    if (tipo == ALVO_INIMIGO) {
        snprintf(buf, sizeof(buf), "\"%s\": qual é a classe?", j->inimigos[idx].texto);
    }
    else {
        const Palavra* p = &j->palavras[idx];
        const char* estado = p->tipo == PAL_LAVA
            ? (p->congelada ? "congelada" : "perigosa")
            : (!p->materializada ? "dormente" : p->movendo ? (p->lenta ? "em movimento lento" : "em movimento")
                : p->congelada ? "congelada" : "sólida");
        snprintf(buf, sizeof(buf), "\"%s\" (%s)", p->texto, estado);
    }
    if (!alcance) {
        size_t n = strlen(buf);
        snprintf(buf + n, sizeof(buf) - n, " - longe demais");
    }

    tw = (float)al_get_text_width((ALLEGRO_FONT*)f->pequena, buf);
    x = j->mouse_x + 16; y = j->mouse_y + 18;
    if (x + tw + 12 > LARGURA_TELA) x = LARGURA_TELA - tw - 14;
    al_draw_filled_rounded_rectangle(x - 6, y - 3, x + tw + 6, y + 18, 5, 5, CA(10, 10, 20, 210));
    texto(f->pequena, alcance ? C(255, 230, 150) : C(255, 140, 130), x, y, ALLEGRO_ALIGN_LEFT, buf);
}

/* ------------------------------------------------------------------ */
/* Telas                                                               */
/* ------------------------------------------------------------------ */

/* Botão de menu: muda de cor quando o mouse está em cima. */
static void botao(const Jogo* j, const Fontes* f, float x, float y, float w, float h, const char* t)
{
    int sobre = j->mouse_x >= x && j->mouse_x <= x + w && j->mouse_y >= y && j->mouse_y <= y + h;
    al_draw_filled_rounded_rectangle(x, y, x + w, y + h, 10, 10, sobre ? CA(60, 48, 90, 235) : CA(24, 20, 34, 215));
    al_draw_rounded_rectangle(x, y, x + w, y + h, 10, 10, sobre ? C(255, 214, 120) : C(160, 140, 100), sobre ? 3.0f : 2.0f);
    texto(f->normal, sobre ? C(255, 240, 170) : C(235, 225, 200), x + w / 2, centro_y(f->normal, y, h), ALLEGRO_ALIGN_CENTRE, t);
}

/* Fundo do menu: o menu.png esticado na tela. Sem imagem, usa cor lisa + título. */
static void fundo_do_menu(const Fontes* f)
{
    if (f->fundo_menu) {
        al_draw_scaled_bitmap(f->fundo_menu, 0, 0,
            (float)al_get_bitmap_width(f->fundo_menu), (float)al_get_bitmap_height(f->fundo_menu),
            0, 0, LARGURA_TELA, ALTURA_TELA, 0);
    }
    else {
        al_clear_to_color(C(16, 14, 24));      /* sem imagem: fundo liso (o título já vem no menu.png) */
    }
}

static void tela_menu(const Jogo* j, const Fontes* f)
{
    fundo_do_menu(f);
    botao(j, f, BTN_X, (float)BTN_Y(0), BTN_W, BTN_H, "JOGAR");
    botao(j, f, BTN_X, (float)BTN_Y(1), BTN_W, BTN_H, "HISTÓRIA");
}

/* Painel escuro sobre o fundo, usado nas telas História e Introdução. */
static void painel(const Fontes* f, const char* titulo)
{
    fundo_do_menu(f);
    al_draw_filled_rectangle(0, 0, LARGURA_TELA, ALTURA_TELA, CA(8, 8, 16, 170));
    al_draw_filled_rounded_rectangle(130, 50, LARGURA_TELA - 130, 450, 14, 14, CA(24, 20, 34, 235));
    al_draw_rounded_rectangle(130, 50, LARGURA_TELA - 130, 450, 14, 14, C(160, 140, 100), 2.0f);
    texto_sombra(f->grande, C(255, 214, 120), LARGURA_TELA / 2.0f, 62, ALLEGRO_ALIGN_CENTRE, titulo);
}

static void tela_historia(const Jogo* j, const Fontes* f)
{
    ALLEGRO_FONT* fn = (ALLEGRO_FONT*)f->normal;
    painel(f, "História");
    al_draw_multiline_text(fn, C(230, 225, 240), 170, 140, LARGURA_TELA - 340, 26, ALLEGRO_ALIGN_LEFT,
        "Você controla Enoq, o último escriba real da biblioteca de Sintaxia. "
        "Um feitiço corrompeu as escrituras sagradas do reino, fazendo com que pontes "
        "desaparecessem, mecanismos travassem e criaturas surgissem a partir de palavras incompletas.");
    al_draw_multiline_text(fn, C(230, 225, 240), 170, 270, LARGURA_TELA - 340, 26, ALLEGRO_ALIGN_LEFT,
        "Use sua Pena Tinteiro para explorar as ruínas, recuperar os fragmentos gramaticais "
        "e derrotar os guardiões corrompidos. Cada palavra tem um poder!");
    botao(j, f, SUB_VOLTAR_X_HIST, SUB_Y, SUB_W, SUB_H, "VOLTAR");
}

/* Introdução: aparece depois do JOGAR e explica os controles e as classes. */
static void tela_intro(const Jogo* j, const Fontes* f)
{
    static const char* classes[CL_TOTAL] = {
        "Substantivo: cria matéria (a palavra vira plataforma)",
        "Adjetivo: altera a física (congela plataforma ou lava)",
        "Verbo: ativa a dinâmica (a plataforma passa a se mover)",
        "Advérbio: ajusta a intensidade (move mais devagar)"
    };
    int i;

    painel(f, "Como jogar");
    texto(f->normal, C(230, 225, 240), 170, 125, ALLEGRO_ALIGN_LEFT, "Mover: A/D ou setas      Pular: W, seta para cima ou Espaço");
    texto(f->normal, C(230, 225, 240), 170, 150, ALLEGRO_ALIGN_LEFT, "Escolher a classe: teclas 1 a 4, roda do mouse ou clique na barra");
    texto(f->normal, C(230, 225, 240), 170, 175, ALLEGRO_ALIGN_LEFT, "Lançar o feitiço: clique esquerdo na palavra (alcance limitado)");
    texto(f->normal, C(230, 225, 240), 170, 200, ALLEGRO_ALIGN_LEFT, "R: reiniciar a fase      ESC: voltar ao menu");

    for (i = 0; i < CL_TOTAL; i++) {
        desenha_icone(i, 184, 256.0f + i * 34, 13);
        texto(f->normal, C(235, 225, 200), 210, 247.0f + i * 34, ALLEGRO_ALIGN_LEFT, classes[i]);
    }
    al_draw_multiline_text((ALLEGRO_FONT*)f->normal, C(255, 214, 120), 170, 392, LARGURA_TELA - 340, 24, ALLEGRO_ALIGN_LEFT,
        "Guardiões: lance a classe gramatical certa na palavra para restaurá-la.");

    botao(j, f, SUB_VOLTAR_X_INTRO, SUB_Y, SUB_W, SUB_H, "VOLTAR");
    botao(j, f, SUB_COMECAR_X, SUB_Y, SUB_W, SUB_H, "COMEÇAR");
}

static void tela_fim(const Jogo* j, const Fontes* f, int vitoria)
{
    float cx = LARGURA_TELA / 2.0f;
    char buf[96];

    al_draw_filled_rectangle(0, 0, LARGURA_TELA, ALTURA_TELA, CA(8, 8, 16, 190));
    if (vitoria) {
        texto_sombra(f->grande, C(255, 214, 120), cx, 150, ALLEGRO_ALIGN_CENTRE, "Biblioteca restaurada!");
        snprintf(buf, sizeof(buf), "Palavras restauradas: %d/%d", j->derrotados, j->n_inimigos);
        texto(f->normal, C(230, 230, 240), cx, 240, ALLEGRO_ALIGN_CENTRE, buf);
        snprintf(buf, sizeof(buf), "Tempo: %d:%02d    Vidas restantes: %d",
            (int)j->tempo_total / 60, (int)j->tempo_total % 60, j->jog.vidas);
        texto(f->normal, C(230, 230, 240), cx, 270, ALLEGRO_ALIGN_CENTRE, buf);
    }
    else {
        texto_sombra(f->grande, C(255, 110, 100), cx, 150, ALLEGRO_ALIGN_CENTRE, "Você foi corrompido...");
        texto(f->normal, C(230, 230, 240), cx, 240, ALLEGRO_ALIGN_CENTRE, "Lembre: cada palavra tem um poder!");
    }
    texto_sombra(f->normal, C(255, 240, 160), cx, 360, ALLEGRO_ALIGN_CENTRE, "Pressione R para jogar de novo");
}

/* ------------------------------------------------------------------ */
/* Entrada principal do desenho                                        */
/* ------------------------------------------------------------------ */

void render_jogo(const Jogo* j, const Fontes* f)
{
    ALLEGRO_TRANSFORM t;
    int idx = 0, alcance = 0, tipo, i;

    if (j->tela == TELA_MENU) { tela_menu(j, f);     return; }
    if (j->tela == TELA_HISTORIA) { tela_historia(j, f); return; }
    if (j->tela == TELA_INTRO) { tela_intro(j, f);    return; }

    desenha_fundo(j);

    al_identity_transform(&t);
    al_translate_transform(&t, -j->cam_x, 0.0f);
    al_use_transform(&t);

    desenha_tiles(j);
    desenha_tochas(j);
    desenha_saida(j, f);
    for (i = 0; i < j->n_palavras; i++) desenha_palavra(j, &j->palavras[i], f);
    for (i = 0; i < j->n_inimigos; i++) if (j->inimigos[i].vivo) desenha_inimigo(j, &j->inimigos[i], f);
    desenha_jogador(j);

    /* Destaque do alvo sob o mouse (amarelo = no alcance, vermelho = longe) */
    if (j->tela == TELA_JOGANDO) {
        tipo = jogo_alvo(j, j->mouse_x, j->mouse_y, &idx, &alcance);
        if (tipo != ALVO_NENHUM && jogo_slot_em(j->mouse_x, j->mouse_y) < 0) {
            ALLEGRO_COLOR c = alcance ? C(255, 225, 110) : C(255, 90, 80);
            if (tipo == ALVO_INIMIGO) {
                const Inimigo* e = &j->inimigos[idx];
                al_draw_rounded_rectangle(e->x - 5, e->y - 5, e->x + e->w + 5, e->y + e->h + 5, 8, 8, c, 2.0f);
            }
            else {
                const Palavra* p = &j->palavras[idx];
                al_draw_rectangle(p->x - 4, p->y - 4, p->x + p->w + 4, p->y + p->h + 4, c, 2.0f);
            }
        }
    }

    al_identity_transform(&t);
    al_use_transform(&t);

    desenha_hud(j, f);
    if (j->tela == TELA_JOGANDO) desenha_tooltip(j, f);
    if (j->tela == TELA_VITORIA) tela_fim(j, f, 1);
    if (j->tela == TELA_DERROTA) tela_fim(j, f, 0);
}