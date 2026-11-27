/* SPDX-License-Identifier: GPL-3.0-only
 * Padroes de bumbo/caixa e regras dos humores portados de krst_dice.c; nomes e carater dos kits de
 * krst_kits.c; swing e ratchet como em seq.c (Kriechstrom, GPL-3.0). Sintese propria em float. */
#include "groove.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define SR 32728.0f

typedef struct {
    float kf0, kf1, kdec, kdrv;      /* bumbo: Hz final, Hz inicial, decaimento (s), drive */
    float sdec, stone, scol;         /* caixa: decaimento, tom, cor do ruido */
    float hdec, mdec, mf;            /* chimbal, metal (decaimento, Hz base) */
    int bits, hold;                  /* crush do barramento */
    float gate;                      /* > 0: corta seco depois de N s */
} Kit;

static const Kit KIT[GRV_KITS] = {
 /*            kf0  kf1  kdec  kdrv  sdec stone scol  hdec  mdec   mf   bits hold gate */
 /* ROOM    */ { 48, 150, .28f, 1.6f, .18f, 190, .55f, .045f, .35f, 410,  16,  1,  0 },
 /* GATE    */ { 52, 180, .40f, 2.5f, .45f, 170, .65f, .060f, .40f, 380,  12,  1, .16f },
 /* MARCH   */ { 60, 120, .18f, 4.0f, .12f, 240, .80f, .030f, .20f, 520,  16,  1,  0 },
 /* ARC     */ { 45, 420, .22f, 2.0f, .09f, 900, .95f, .018f, .12f, 1150, 16,  1,  0 },
 /* ROT     */ { 40, 130, .35f, 3.0f, .22f, 140, .40f, .080f, .50f, 230,  8,   4,  0 },
 /* SLAB    */ { 34,  90, .60f, 5.0f, .35f, 110, .30f, .050f, .80f, 190,  16,  2,  0 },
 /* DESTROY */ { 55, 300, .30f, 12.f, .20f, 330, .90f, .070f, .30f, 640,  6,   3,  0 },
};

const char *grv_kit_name(int k) {
    static const char *const n[GRV_KITS] = { "ROOM", "GATE", "MARCH", "ARC", "ROT", "SLAB", "DESTROY" };
    return n[(unsigned)k % GRV_KITS];
}
const char *grv_mood_name(int m) {
    static const char *const n[GRV_MOODS] = { "MACH", "GRIND", "MARCH", "GLTCH", "SPIRL", "GHOST", "SURGE", "HUSH" };
    return (m >= 0 && m < GRV_MOODS) ? n[m] : "FREE";
}
float grv_bpm(float x) { return 60.0f + 140.0f * x; }

static inline uint32_t xr(uint32_t *s) { uint32_t x = *s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return *s = x; }
static inline float noise(uint32_t *s) { return (float)(int32_t)xr(s) * (1.0f / 2147483648.0f); }

void grv_init(Groove *g) { memset(g, 0, sizeof *g); g->rng = 0x9E3779B9u; g->step_ui = -1; g->cur_ui = 0; }
void grv_clear(GrvPat *p) { memset(p->cell, 0, sizeof p->cell); }

void grv_default(GrvPat *p) {
    memset(p, 0, sizeof *p);
    p->len = 16; p->mood = -1; p->bpm = 0.43f; p->level = 0.6f;
    for (int i = 0; i < 16; i += 4) p->cell[0][i] = 1;
    p->cell[1][4] = p->cell[1][12] = 1;
    for (int i = 2; i < 16; i += 4) p->cell[2][i] = 1;
}

static void lane_str(GrvPat *p, int l, const char *s) {
    for (int i = 0; i < GRV_STEPS && s[i]; i++) if (s[i] != '.') p->cell[l][i] = 1;
}

void grv_dice(GrvPat *p, uint32_t seed) {
    static const char *const KICK[GRV_MOODS][3] = {
        {"x...x...x...x...", "x...x...x...x..x", "x...x...x..xx..."},
        {"x......x..x.....", "x.........x.....", "x..x......x..x.."},
        {"x.x...x.x.x...", "x.x...x.xx.x..", "x.x..xx.x.x..."},
        {"x..x.....x..x...", "x....x.x..x....x", "xx.....x..x..x.."},
        {"x.......x.......", "x......x..x.....", "x.......x.....x."},
        {"x...............", "................", "x.......x......."},
        {"x...x...x...x...", "x..xx...x..xx...", "x.x.x...x.x.x..x"},
        {"x.......x.......", "x.......x...x...", "x.........x....."}};
    static const char *const SNARE[GRV_MOODS][3] = {
        {"....x.......x...", "....x.......x...", "....x.......x..x"},
        {"........x.......", "........x.......", "........x.....x."},
        {"....x.....x...", "....x.....x..x", "....x...x.x..."},
        {"...x.....x....x.", ".....x..x...x...", "....x..x...x.x.."},
        {"....x.......x...", "........x.......", "....x.......x.x."},
        {"................", "................", "................"},
        {"....x.......x...", "....x..x....x...", "....x.......xxxx"},
        {"................", "............x...", "................"}};
    static const uint8_t mood_kit[GRV_MOODS] = { 2, 5, 2, 3, 4, 0, 6, 0 };
    uint32_t r = seed ? seed : 0x51EDu;
    xr(&r); xr(&r);
    int m = p->mood >= 0 ? p->mood : (int)(xr(&r) % GRV_MOODS);
    grv_clear(p);
    p->len = m == 2 ? 14 : 16;
    if (p->mood >= 0) p->kit = mood_kit[m];
    lane_str(p, 0, KICK[m][xr(&r) % 3]);
    lane_str(p, 1, SNARE[m][xr(&r) % 3]);
    for (int i = 0; i < p->len; i++) {
        uint32_t a = xr(&r) % 100, b = xr(&r) % 100;
        int hat = 0;
        switch (m) {
            case 0: hat = (i % 4) == 2; break;                    /* MACHINE */
            case 1: hat = !(i & 1) && ((i % 4) == 0 || a < 50); break;
            case 2: case 6: hat = 1; break;                       /* MARCH, SURGE */
            case 3: hat = a < 28; break;                          /* GLITCH */
            case 4: hat = (i % 4) == 2; break;
            case 7: hat = i == 4 || i == 12; break;               /* HUSH */
            default: break;                                       /* GHOST */
        }
        if (hat) p->cell[2][i] = 1;
        if (hat && ((m == 3 && b < 35) || (m == 6 && b < 12))) p->cell[2][i] = (uint8_t)(2 + (xr(&r) % 3));
        if (m != 5 && m != 7 && !p->cell[0][i] && !p->cell[1][i] && xr(&r) % 100 < (uint32_t)((m == 3 || m == 2) ? 18 : 10))
            p->cell[3][i] = 1;
    }
    if (m == 5) p->cell[3][4 + 4 * (xr(&r) % 3)] = 1;            /* GHOST: um sino solitario */
}

static void trigger(Groove *g, int l) {
    GrvVoice *v = &g->v[l];
    v->env = 1.0f; v->t = 0.0f; v->ph = 0.0f; v->on = 1;
}

int grv_used(const GrvBank *bk, int i) {
    for (int l = 0; l < GRV_LANES; l++) for (int s = 0; s < GRV_STEPS; s++) if (bk->cell[i][l][s]) return 1;
    return 0;
}

void grv_render(Groove *g, const GrvPat *p, const GrvBank *bk, int sel, int chain, int16_t *io, int frames) {
    if (!p->on) { g->playing = 0; g->step_ui = -1; g->cur = sel; g->cur_ui = sel; for (int l = 0; l < GRV_LANES; l++) g->v[l].on = 0; return; }
    if (sel < 0 || sel >= GRV_PATS) sel = 0;
    if (!chain || !g->playing) g->cur = sel;
    const Kit *k = &KIT[p->kit % GRV_KITS];
    const uint8_t (*cells)[GRV_STEPS] = chain ? bk->cell[g->cur] : p->cell;
    int plen = (chain ? bk->len[g->cur] : p->len) == 14 ? 14 : 16;
    float base = SR * 60.0f / (grv_bpm(p->bpm) * 4.0f);          /* um 16avo */
    float sw = p->swing * 0.4f;                                  /* passo par mais longo, impar mais curto */
    if (!g->playing) { g->playing = 1; g->step = 0; g->pos = 0.0f; g->hit = 0; g->len = base * (1.0f + sw); }

    float dk = expf(-1.0f / (k->kdec * SR * 0.35f)), ds = expf(-1.0f / (k->sdec * SR * 0.35f));
    float dh = expf(-1.0f / (k->hdec * SR * 0.35f)), dm = expf(-1.0f / (k->mdec * SR * 0.35f));
    float dec[GRV_LANES] = { dk, ds, dh, dm };
    float q = (float)(1 << (k->bits - 1)), iq = 1.0f / q;
    static const float ratio[6] = { 1.0f, 1.483f, 1.8f, 2.546f, 2.63f, 3.9f };
    float lvl = p->level * 0.85f;

    for (int n = 0; n < frames; n++) {
        /* ---- relogio: ratchet divide o passo em partes iguais ---- */
        for (int l = 0; l < GRV_LANES; l++) {
            int c = cells[l][g->step];
            if (!c) continue;
            if (c > 4) c = 4;
            for (int h = 0; h < c; h++) {
                float at = floorf(g->len * (float)h / (float)c);
                if (g->pos >= at && g->pos < at + 1.0f) trigger(g, l);
            }
        }
        g->step_ui = g->step; g->cur_ui = g->cur;
        g->pos += 1.0f;
        if (g->pos >= g->len) {
            g->pos -= floorf(g->len);
            g->step = (g->step + 1) % plen;
            if (chain && g->step == 0) {                          /* fim do compasso: proximo padrao com batidas */
                for (int k = 1; k <= GRV_PATS; k++) { int c2 = (g->cur + k) % GRV_PATS; if (grv_used(bk, c2)) { g->cur = c2; break; } }
                cells = bk->cell[g->cur]; plen = bk->len[g->cur] == 14 ? 14 : 16;
            }
            g->len = floorf(base * ((g->step & 1) ? 1.0f - sw : 1.0f + sw));
        }

        /* ---- vozes ---- */
        float mix = 0.0f, nz = noise(&g->rng);
        g->hp += (nz - g->hp) * 0.45f;
        float hiss = nz - g->hp;
        for (int l = 0; l < GRV_LANES; l++) {
            GrvVoice *v = &g->v[l];
            if (!v->on) continue;
            float s = 0.0f;
            if (l == 0) {                                        /* bumbo: seno que cai + drive */
                float f = k->kf0 + (k->kf1 - k->kf0) * expf(-v->t * 38.0f);
                v->ph += f / SR; if (v->ph >= 1.0f) v->ph -= 1.0f;
                float u = sinf(6.2831853f * v->ph) * k->kdrv;
                s = u / (1.0f + fabsf(u)) * 1.25f * v->env;
            } else if (l == 1) {                                 /* caixa: corpo + ruido */
                v->ph += k->stone / SR; if (v->ph >= 1.0f) v->ph -= 1.0f;
                g->lp += (nz - g->lp) * (0.1f + 0.85f * k->scol);
                s = (sinf(6.2831853f * v->ph) * 0.4f * v->env + g->lp * 0.9f) * v->env;
            } else if (l == 2) {
                s = hiss * 0.6f * v->env;
            } else {                                             /* metal: seis quadradas */
                float m = 0.0f;
                for (int i = 0; i < 6; i++) {
                    g->mph[i] += k->mf * ratio[i] / SR; if (g->mph[i] >= 1.0f) g->mph[i] -= 1.0f;
                    m += g->mph[i] < 0.5f ? 1.0f : -1.0f;
                }
                s = (m * 0.09f + hiss * 0.15f) * v->env;
            }
            float att = v->t * SR / 24.0f; if (att < 1.0f) s *= att;
            mix += s;
            v->t += 1.0f / SR;
            if (k->gate > 0.0f && (l == 0 || l == 1) && v->t > k->gate) v->env *= 0.99f;
            else v->env *= dec[l];
            if (v->env < 0.001f) v->on = 0;
        }
        if (k->hold > 1) {
            if (g->hold_cnt <= 0) { g->held = mix; g->hold_cnt = k->hold; }
            g->hold_cnt--; mix = g->held;
        }
        if (k->bits < 16) mix = floorf(mix * q + 0.5f) * iq;
        mix *= lvl;
        for (int ch = 0; ch < 2; ch++) {
            float o = (float)io[n * 2 + ch] * (1.0f / 32768.0f) + mix;
            float a = fabsf(o);
            if (a > 0.8f) o = (o > 0 ? 1.0f : -1.0f) * (0.8f + 0.19f * (a - 0.8f) / (a - 0.8f + 0.19f));
            io[n * 2 + ch] = (int16_t)(o * 32767.0f);
        }
    }
}

/* ---- arquivo: "IDRM" v1 | kit len bpm swing level (floats) | len[4] | cell[4][4][16] ---- */
int grv_save(const char *path, const GrvPat *p, const GrvBank *bk) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    uint8_t hd[8] = { 'I', 'D', 'R', 'M', 1, p->kit, 0, 0 };
    float fl[3] = { p->bpm, p->swing, p->level };
    int ok = fwrite(hd, 1, 8, f) == 8 && fwrite(fl, sizeof fl, 1, f) == 1 &&
             fwrite(bk->len, 1, GRV_PATS, f) == GRV_PATS && fwrite(bk->cell, 1, sizeof bk->cell, f) == sizeof bk->cell;
    if (fclose(f) != 0) ok = 0;
    return ok ? 0 : -1;
}

int grv_load(const char *path, GrvPat *p, GrvBank *bk) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    uint8_t hd[8]; float fl[3]; GrvBank t;
    int ok = fread(hd, 1, 8, f) == 8 && hd[0] == 'I' && hd[1] == 'D' && hd[2] == 'R' && hd[3] == 'M' && hd[4] == 1 &&
             fread(fl, sizeof fl, 1, f) == 1 && fread(t.len, 1, GRV_PATS, f) == GRV_PATS && fread(t.cell, 1, sizeof t.cell, f) == sizeof t.cell;
    fclose(f);
    if (!ok) return -1;
    for (int i = 0; i < GRV_PATS; i++) { t.len[i] = t.len[i] == 14 ? 14 : 16; for (int l = 0; l < GRV_LANES; l++) for (int s = 0; s < GRV_STEPS; s++) if (t.cell[i][l][s] > 4) t.cell[i][l][s] = 4; }
    for (int k = 0; k < 3; k++) if (!(fl[k] >= 0.0f && fl[k] <= 1.0f)) fl[k] = 0.5f;
    *bk = t; p->kit = (uint8_t)(hd[5] % GRV_KITS); p->bpm = fl[0]; p->swing = fl[1]; p->level = fl[2];
    return 0;
}
