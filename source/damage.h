/* DAMAGE: a maquina se desgastando, no mix final (harsh e ambient).
 * Ideia e curvas portadas de krst_damage.c (Kriechstrom, GPL-3.0). */
#ifndef DAMAGE_H
#define DAMAGE_H
#include <stdint.h>

#define DMG_LINE 512

typedef struct {
    uint32_t rng;
    float lp[2], hold[2], line[2][DMG_LINE];
    float wow_ph, flut_ph, loss, wet, e_in, e_out, g;
    int wp, cnt, drop;
} Damage;

void damage_init(Damage *d);
/* amount 0..1 (0 = desligado, audio intacto); chr 0 = FITA .. 1 = DIGITAL */
void damage_process(Damage *d, int16_t *io, int frames, float amount, float chr);

#endif
