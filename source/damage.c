/* SPDX-License-Identifier: GPL-3.0-only
 * DAMAGE: portado em float a partir de krst_damage.c (Kriechstrom, GPL-3.0).
 * FITA: passa-baixa que escurece, wow/flutter e quedas que afundam (-24 dB).
 * DIGITAL: menos bits, amostra segurada e quedas que cortam.
 * Um "guarda de nivel" so abaixa: o DAMAGE muda o som, nao o volume. */
#include "damage.h"
#include <math.h>
#include <string.h>

#define SR 32728.0f
#define BLK 32

void damage_init(Damage *d) {
    memset(d, 0, sizeof *d);
    d->rng = 0x2545F491u;
    d->g = 1.0f;
}

static inline uint32_t xrnd(uint32_t *s) { uint32_t x = *s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return *s = x; }
static inline float tri(float ph) { return ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph; }

void damage_process(Damage *d, int16_t *io, int frames, float amount, float chr) {
    float a = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
    float c = chr < 0.0f ? 0.0f : (chr > 1.0f ? 1.0f : chr);
    if (a < 0.005f && d->wet <= 0.0f) return;            /* desligado: bit a bit igual */
    if (d->wet <= 0.0f) {                                /* ligando: sem restos antigos */
        memset(d->line, 0, sizeof d->line);
        d->lp[0] = d->lp[1] = d->hold[0] = d->hold[1] = 0.0f;
        d->cnt = d->drop = 0; d->loss = d->e_in = d->e_out = 0.0f; d->g = 1.0f;
    }
    float tape = 1.0f - c, at = a * tape;
    float drive = 1.0f + a * a * 15.0f;
    float k = 1.0f - at * 0.88f;                         /* 1 = aberto; ~0.12 = ~700 Hz */
    float wow = at * at * 118.0f;                        /* ate ~3.6 ms */
    int bits = 16 - (int)(a * c * 11.0f);
    float q = (float)(1 << (bits - 1)), iq = 1.0f / q;
    int hold = 1 + (int)(a * c * 11.0f);
    float tgt_wet = a < 0.005f ? 0.0f : 1.0f;

    for (int b0 = 0; b0 < frames; b0 += BLK) {
        int n = frames - b0 < BLK ? frames - b0 : BLK;
        /* quedas sorteadas por bloco */
        if (d->drop > 0) d->drop--;
        else if (a > 0.2f && (float)(xrnd(&d->rng) & 0xFFFFFF) < a * a * a * 0.005f * 16777216.0f)
            d->drop = c > 0.5f ? 4 + (int)(xrnd(&d->rng) & 15) : 20 + (int)(xrnd(&d->rng) & 47);
        float dt = d->drop > 0 ? (c > 0.5f ? 1.0f : 0.94f) : 0.0f;
        float slope = c > 0.5f ? 0.2f : 0.012f;

        float ei = 0.0f, eo = 0.0f;
        for (int i = 0; i < n; i++) {
            int16_t *p = io + (b0 + i) * 2;
            d->wet += (tgt_wet > d->wet) ? 0.002f : (tgt_wet < d->wet ? -0.002f : 0.0f);
            if (d->wet > 1.0f) d->wet = 1.0f;
            if (d->wet < 0.0f) d->wet = 0.0f;
            d->loss += (dt - d->loss) * slope;
            d->wow_ph += 0.6f / SR; if (d->wow_ph >= 1.0f) d->wow_ph -= 1.0f;
            d->flut_ph += 6.0f / SR; if (d->flut_ph >= 1.0f) d->flut_ph -= 1.0f;
            float dist = 128.0f + (tri(d->wow_ph) + 0.125f * tri(d->flut_ph)) * wow;
            int new_hold = d->cnt <= 0;
            if (new_hold) d->cnt = hold;
            d->cnt--;
            float y[2];
            for (int ch = 0; ch < 2; ch++) {
                float x = (float)p[ch] * (1.0f / 32768.0f);
                ei += x * x;
                float u = x * drive;
                u = u / (1.0f + fabsf(u));                /* saturacao suave */
                d->lp[ch] += (u - d->lp[ch]) * k;
                u = d->lp[ch];
                d->line[ch][d->wp] = u;
                if (wow > 0.5f) {
                    float rp = (float)d->wp - dist; if (rp < 0.0f) rp += (float)DMG_LINE;
                    int i0 = (int)rp; float fr = rp - (float)i0;
                    float s0 = d->line[ch][i0 & (DMG_LINE - 1)], s1 = d->line[ch][(i0 + 1) & (DMG_LINE - 1)];
                    u = s0 + (s1 - s0) * fr;
                }
                if (hold > 1) { if (new_hold) d->hold[ch] = u; u = d->hold[ch]; }
                if (bits < 16) u = floorf(u * q + 0.5f) * iq;
                y[ch] = u * (1.0f - d->loss);
                eo += y[ch] * y[ch];
            }
            d->wp = (d->wp + 1) & (DMG_LINE - 1);
            for (int ch = 0; ch < 2; ch++) {
                float x = (float)p[ch] * (1.0f / 32768.0f);
                float v = x + (y[ch] * d->g - x) * d->wet;
                if (v > 1.0f) v = 1.0f;
                if (v < -1.0f) v = -1.0f;
                p[ch] = (int16_t)(v * 32767.0f);
            }
        }
        /* guarda de nivel: saida volta ao RMS da entrada, so para baixo */
        d->e_in  += (ei / (float)(2 * n) - d->e_in)  * 0.06f;
        d->e_out += (eo / (float)(2 * n) - d->e_out) * 0.06f;
        float gt = 1.0f;
        if (d->e_out > 1e-6f && d->e_out > d->e_in) gt = sqrtf(d->e_in / d->e_out);
        if (gt < 0.2f) gt = 0.2f;
        d->g += (gt - d->g) * 0.2f;
    }
}
