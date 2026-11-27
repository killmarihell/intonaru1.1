/* GROOVE: bateria de ruido em 16 passos por cima do harsh / ambient.
 * Kits, humores (DICE), swing e ratchet: ideias de Kriechstrom (GPL-3.0). */
#ifndef GROOVE_H
#define GROOVE_H
#include <stdint.h>

#define GRV_LANES 4              /* KICK SNARE HAT METAL */
#define GRV_STEPS 16
#define GRV_KITS  7
#define GRV_MOODS 8
#define GRV_PATS  4              /* 1.1: 4 padroes (P1..P4), com CHAIN encadeando */

typedef struct {
    uint8_t on, kit, len;        /* len 16 (MARCH: 14) */
    int8_t  mood;                /* -1 = livre */
    float   bpm, swing, level;   /* 0..1 */
    uint8_t cell[GRV_LANES][GRV_STEPS];   /* 0 = vazio, 1..4 = batidas no passo (ratchet) */
} GrvPat;

/* 1.1: os 4 padroes guardados (o que esta sendo editado tambem fica espelhado aqui) */
typedef struct {
    uint8_t cell[GRV_PATS][GRV_LANES][GRV_STEPS];
    uint8_t len[GRV_PATS];
} GrvBank;

typedef struct { float env, ph, inc, t; int on; } GrvVoice;

typedef struct {
    uint32_t rng;
    int step, hit, playing, hold_cnt;
    int cur;                     /* padrao tocando (CHAIN) */
    volatile int cur_ui;
    float pos, len, hp, bp, lp, held, mph[6];
    GrvVoice v[GRV_LANES];
    volatile int step_ui;
} Groove;

void grv_init(Groove *g);
void grv_default(GrvPat *p);
void grv_clear(GrvPat *p);
void grv_dice(GrvPat *p, uint32_t seed);              /* novo padrao no humor atual (livre: humor sorteado) */
/* soma no audio. sel = padrao editado; chain = toca P1..P4 em sequencia (so os que tem batidas), um compasso cada */
void grv_render(Groove *g, const GrvPat *p, const GrvBank *bk, int sel, int chain, int16_t *io, int frames);
int  grv_used(const GrvBank *bk, int i);              /* 1 = o padrao i tem alguma batida */
/* 1.1: salvar / carregar tudo (4 padroes + kit, bpm, swing, level) num arquivo no SD. 0 = ok */
int  grv_save(const char *path, const GrvPat *p, const GrvBank *bk);
int  grv_load(const char *path, GrvPat *p, GrvBank *bk);
float grv_bpm(float x);                               /* 0..1 -> 60..200 */
const char *grv_kit_name(int k);
const char *grv_mood_name(int m);

#endif
