/* Testes no PC do motor de som e da UI. NAO e usado no 3DS. */
#include "../source/synth.h"
#include "../source/ui.h"
#include "../source/amb.h"
#include "../source/rec.h"
#include "../source/damage.h"
#include "../source/groove.h"
#include "../source/goetia.h"
#include "../source/art.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

static void wav(const char *fn, const int16_t *m, int n, float gainfix) {
    FILE *f = fopen(fn, "wb"); uint32_t sr = SYN_RATE, br = sr * 2, ds = n * 2, rs = 36 + ds;
    uint16_t one = 1, bits = 16, ba = 2, pcm = 1; uint32_t fmt = 16;
    fwrite("RIFF",1,4,f); fwrite(&rs,4,1,f); fwrite("WAVEfmt ",1,8,f); fwrite(&fmt,4,1,f);
    fwrite(&pcm,2,1,f); fwrite(&one,2,1,f); fwrite(&sr,4,1,f); fwrite(&br,4,1,f);
    fwrite(&ba,2,1,f); fwrite(&bits,2,1,f); fwrite("data",1,4,f); fwrite(&ds,4,1,f);
    for (int i = 0; i < n; i++) { int16_t q = (int16_t)(m[i] * gainfix); fwrite(&q,2,1,f); }
    fclose(f);
}

static void save_ppm(const char *fn, Surf *s) {
    FILE *f = fopen(fn, "wb"); fprintf(f, "P6\n%d %d\n255\n", s->w, s->h);
    for (int y = 0; y < s->h; y++) for (int x = 0; x < s->w; x++) {
        uint16_t c = s->px[x * s->h + (s->h - 1 - y)];
        uint8_t rgb[3] = { (uint8_t)(((c >> 11) & 31) << 3), (uint8_t)(((c >> 5) & 63) << 2), (uint8_t)((c & 31) << 3) };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

int main(int argc, char **argv) {
    const char *outdir = argc > 1 ? argv[1] : ".";
    char path[256];
    static Synth S; static int16_t buf[512 * 2];
    int bad = 0;

    printf("== PRESETS: 3 s cada (taxa %d Hz) ==\n", SYN_RATE);
    printf("%-11s %7s %7s %8s %6s %7s %s\n", "preset", "pico", "rms", "dc", "zcr", "noTeto", "status");
    for (int p = 0; p < N_PRESETS; p++) {
        float v[P_COUNT]; patch_default(v); patch_preset(v, p); v[P_VOL] = 0.6f;
        synth_init(&S, 0xABCD1234u + p);
        int total = SYN_RATE * 3 / 512 * 512, k = 0;
        int16_t *mono = malloc(total * 2);
        for (int b = 0; b < total / 512; b++) {
            synth_render(&S, v, 0, 0, 0, 0, buf, 512);
            for (int i = 0; i < 512; i++) mono[k++] = buf[i * 2];
        }
        double sum = 0, sq = 0; int pk = 0, zc = 0;
        for (int i = 4000; i < k; i++) {
            sum += mono[i]; sq += (double)mono[i] * mono[i];
            if (abs(mono[i]) > pk) pk = abs(mono[i]);
            if ((mono[i] < 0) != (mono[i-1] < 0)) zc++;
        }
        int atcap = 0; for (int i = 4000; i < k; i++) if (abs(mono[i]) >= pk - 40) atcap++;
        int n = k - 4000; double rms = sqrt(sq / n) / 32768.0, dc = sum / n / 32768.0;
        const char *st = "ok";
        if (pk == 0 || rms < 0.01) { st = "SILENCIO?"; bad++; }
        printf("%-11s %7.3f %7.3f %8.4f %6.3f %6.1f%% %s\n", preset_names[p], pk / 32768.0, rms, dc, (double)zc / n, 100.0 * atcap / n, st);
        float norm = pk ? 16000.0f / pk : 1.0f;
        snprintf(path, sizeof path, "%s/preview_%d_%s.wav", outdir, p + 1, preset_names[p]);
        wav(path, mono, k, norm);
        free(mono);
    }

    printf("\n== ESTRESSE: 2000 patches aleatorios/extremos, com bend/burst/mute ==\n");
    patch_seed(99);
    int fails = 0; float worst = 0; clock_t t0 = clock(); long frames = 0;
    for (int t = 0; t < 2000; t++) {
        float v[P_COUNT]; patch_default(v);
        if (t % 2) patch_randomize(v); else for (int i = 0; i < P_COUNT; i++) v[i] = (rand() & 1) ? (rand() & 1) : (rand() / (float)RAND_MAX);
        v[P_VOL] = 1.0f;
        synth_init(&S, 1000 + t);
        for (int b = 0; b < 6; b++) {
            synth_render(&S, v, (b - 3) * 0.4f, (b % 2) ? 1.f : -1.f, b == 2, b == 4, buf, 512);
            frames += 512;
            for (int i = 0; i < 1024; i++) {
                float a = fabsf(buf[i] / 32768.0f); if (a > worst) worst = a;
            }
            for (int i = 0; i < P_COUNT; i++) if (!(S.sm[i] == S.sm[i])) fails++;
            if (!(S.dc_y == S.dc_y) || !(S.gain == S.gain)) fails++;
        }
    }
    double dt = (double)(clock() - t0) / CLOCKS_PER_SEC;
    printf("falhas (NaN/overflow): %d | pico maximo: %.3f | custo medio no PC: %.0f ns/amostra\n",
           fails, worst, dt / frames * 1e9);
    bad += fails;

    printf("\n== AMBIENT/DRONE 0.8: 16 presets, 5 modos do lead, acordes em tempo real ==\n");
    static Amb A; static int16_t ab[512 * 2];
    for (int pr = 0; pr < A_NPRESETS; pr++) {            /* cada preset tem que soar, sem NaN, sem estourar e SEM glide entre acordes */
        AmbPatch ap; amb_default(&ap); amb_preset(&ap, pr); ap.p[A_VOL] = 0.8f; ap.p[A_DRATE] = 0.0f;
        AmbIn in = { 0, 0.4f, 0.6f, 0, 0, 0 };
        amb_init(&A, 42 + pr);
        int tot = SYN_RATE * 20 / 512, k = 0; double sq = 0, sum = 0; int pk = 0, nan = 0; long slide = 0, voices = 0;
        int16_t *mono = malloc(sizeof(int16_t) * tot * 512);
        for (int b = 0; b < tot; b++) {
            in.touch = amb_ival(&ap, A_LMODE) == L_KEYS && (b / 20) % 2 == 0;
            amb_render(&A, &ap, &in, ab, 512);
            for (int i = 0; i < 512; i++) {
                int16_t l = ab[i * 2]; mono[k++] = l; if (b > 20) { sq += (double)l * l; sum += l; if (abs(l) > pk) pk = abs(l); } }
            for (int i = 0; i < A_COUNT; i++) if (!(A.sm[i] == A.sm[i])) nan++;
            if (!(A.gain == A.gain)) nan++;
            for (int i = 0; i < AMB_DV; i++) { AVoice *v = &A.v[i]; if (v->env > 0.02f) { voices++; if (fabsf(1200.0f * log2f(v->freq / v->tfreq)) > 15.0f) slide++; } }
        }
        double rms = sqrt(sq / (k - 20 * 512)) / 32768.0;
        const char *st = "ok";
        if (rms < 0.004) { st = "SILENCIO?"; bad++; }
        if (nan) { st = "NaN"; bad++; }
        if (slide > voices / 50) { st = "GLIDE (sirene)"; bad++; }
        printf("%-10s %-9s pico %.3f  rms %.3f  glide %ld/%ld  %s\n", amb_preset_name(pr), amb_lmode_name(amb_ival(&ap, A_LMODE)), pk / 32768.0, rms, slide, voices, st);
        if (pr < 4) { snprintf(path, sizeof path, "%s/preview_amb_%s.wav", outdir, amb_preset_name(pr)); wav(path, mono, k, 16000.0f / (pk ? pk : 1)); }
        free(mono);
    }
    {   /* os 5 modos do lead em 2 escalas, com o lead isolado */
        for (int md = 0; md < A_NLMODE; md++) for (int sc = 0; sc < 2; sc++) {
            AmbPatch ap; amb_default(&ap); ap.drone_on = 0; amb_set_index(&ap, A_LMODE, md); amb_set_index(&ap, A_SCALE, sc ? 6 : 1);
            ap.p[A_VOL] = 0.8f; ap.p[A_DENS] = 1.0f;
            AmbIn in = { md == L_KEYS, 0.4f, 0.6f, 0, 0, 0 };
            amb_init(&A, 99 + md); double sq = 0; int n = 0;
            for (int b = 0; b < SYN_RATE * 8 / 512; b++) { amb_render(&A, &ap, &in, ab, 512); if (b > 10) for (int i = 0; i < 512; i++) { sq += (double)ab[i * 2] * ab[i * 2]; n++; } }
            double rms = sqrt(sq / n) / 32768.0;
            if (rms < 0.002) { printf("lead %s escala %d: SILENCIO (rms %.4f)\n", amb_lmode_name(md), sc, rms); bad++; }
        }
        printf("5 modos do lead x 2 escalas: soam\n");
    }
    {   /* os 8 timbres do drone e do lead tem que soar */
        for (int sn = 0; sn < A_NSND; sn++) {
            AmbPatch ap; amb_default(&ap); amb_set_index(&ap, A_DSND, sn); amb_set_index(&ap, A_LSND, sn); ap.p[A_VOL] = 0.8f;
            AmbIn in = { 0, 0, 0, 0, 0, 0 }; amb_init(&A, 500 + sn);
            double sq = 0; int n = 0, nan = 0;
            for (int b = 0; b < SYN_RATE * 6 / 512; b++) {
                amb_render(&A, &ap, &in, ab, 512);
                if (b > 30) for (int i = 0; i < 512; i++) { sq += (double)ab[i * 2] * ab[i * 2]; n++; }
                if (!(A.gain == A.gain)) nan++;
            }
            double rms = sqrt(sq / (n ? n : 1)) / 32768.0;
            if (rms < 0.004 || nan) { printf("timbre %s: SILENCIO/NaN (rms %.4f)\n", amb_snd_name(sn), rms); bad++; }
        }
        printf("8 timbres drone+lead: soam\n");
    }
    {   /* FREEZE: com a sala congelada a cauda do reverb segue soando depois que as vozes calam; sem freeze ela cai */
        double rr[2];
        for (int fz = 0; fz < 2; fz++) {
            AmbPatch ap; amb_default(&ap); ap.p[A_VOL] = 0.8f; ap.p[A_REV] = 0.6f; ap.lead_on = 0;
            AmbIn in = { 0, 0, 0, 0, 0, 0, 0 }; amb_init(&A, 11);
            for (int b = 0; b < 300; b++) amb_render(&A, &ap, &in, ab, 512);
            in.freeze = fz; for (int b = 0; b < 150; b++) amb_render(&A, &ap, &in, ab, 512);
            ap.p[A_DVOL] = 0.0f; double sq = 0; int n = 0;
            for (int b = 0; b < 600; b++) { amb_render(&A, &ap, &in, ab, 512); if (b >= 500) for (int i = 0; i < 512; i++) { sq += (double)ab[i * 2] * ab[i * 2]; n++; } }
            rr[fz] = sqrt(sq / n) / 32768.0;
        }
        int ok = rr[1] > 0.004 && rr[1] > rr[0] * 2.0;
        printf("FREEZE: cauda sem %.4f | com %.4f -> %s\n", rr[0], rr[1], ok ? "ok" : "FALHOU"); if (!ok) bad++;
    }
    {   /* THROW: segurando, o eco/reverb crescem (mais energia) e sem estourar; solto, volta */
        double e[2]; float pk = 0;
        for (int th = 0; th < 2; th++) {
            AmbPatch ap; amb_default(&ap); ap.p[A_VOL] = 0.8f; ap.lead_on = 1; ap.p[A_ECHO] = 0.4f;
            AmbIn in = { 1, 0.5f, 0.5f, 0, 0, 0, 0, 0 }; amb_init(&A, 21);
            for (int b = 0; b < 300; b++) amb_render(&A, &ap, &in, ab, 512);
            in.thr = th; double sq = 0; int n = 0;
            for (int b = 0; b < 400; b++) { amb_render(&A, &ap, &in, ab, 512); if (b >= 200) for (int i = 0; i < 512; i++) { float v = fabsf(ab[i * 2] / 32768.0f); if (v > pk) pk = v; sq += (double)ab[i * 2] * ab[i * 2]; n++; } }
            e[th] = sqrt(sq / n) / 32768.0;
        }
        int ok = e[1] > e[0] * 1.15 && pk < 0.98f;
        printf("THROW: rms sem %.4f | com %.4f | pico %.3f -> %s\n", e[0], e[1], pk, ok ? "ok" : "FALHOU"); if (!ok) bad++;
    }
    {   /* ALIVE: 0 = vozes sempre cheias; 3 = vozes entram/saem (ganho cai) e a densidade vagueia, sem estourar */
        float mn[2], dmax[2], pk = 0;
        for (int al = 0; al < 4; al += 3) {
            AmbPatch ap; amb_default(&ap); ap.p[A_VOL] = 0.8f; AmbIn in = { 0, 0, 0, 0, 0, 0, 0, 0, al }; amb_init(&A, 31);
            mn[al ? 1 : 0] = 1.0f; dmax[al ? 1 : 0] = 0.0f;
            for (int b = 0; b < 32000 * 60 / 512; b++) {
                amb_render(&A, &ap, &in, ab, 512);
                for (int k = 0; k < 4; k++) if (A.al_g[k] < mn[al ? 1 : 0]) mn[al ? 1 : 0] = A.al_g[k];
                if (fabsf(A.al_d) > dmax[al ? 1 : 0]) dmax[al ? 1 : 0] = fabsf(A.al_d);
                for (int i = 0; i < 512; i++) { float v = fabsf(ab[i * 2] / 32768.0f); if (v > pk) pk = v; }
            }
        }
        int ok = mn[0] > 0.999f && dmax[0] < 1e-6f && mn[1] < 0.6f && dmax[1] > 0.05f && pk < 0.98f;
        printf("ALIVE: OFF min %.2f dens %.2f | HIGH min %.2f dens %.2f | pico %.2f -> %s\n", mn[0], dmax[0], mn[1], dmax[1], pk, ok ? "ok" : "FALHOU"); if (!ok) bad++;
    }
    {   /* JOURNEY: amb_travel -> t=0 e a, t=1 e b, A_VOL intacto, discretos so trocam no meio */
        AmbPatch a, b, o; amb_default(&a); amb_default(&b); amb_preset(&a, 2); amb_preset(&b, 9); a.p[A_VOL] = 0.33f;
        int ok = 1; o = a; amb_travel(&o, &a, &b, 0.0f); for (int i = 0; i < A_COUNT; i++) if (fabsf(o.p[i] - a.p[i]) > 1e-6f) ok = 0;
        o = a; amb_travel(&o, &a, &b, 1.0f); for (int i = 0; i < A_COUNT; i++) if (i != A_VOL && fabsf(o.p[i] - b.p[i]) > 1e-6f) ok = 0;
        if (o.p[A_VOL] != a.p[A_VOL]) ok = 0;
        o = a; amb_travel(&o, &a, &b, 0.49f); if (o.cmod != a.cmod || o.scene != a.scene || o.p[A_ROOT] != a.p[A_ROOT]) ok = 0;
        o = a; amb_travel(&o, &a, &b, 0.51f); if (o.scene != b.scene || o.p[A_ROOT] != b.p[A_ROOT]) ok = 0;
        printf("JOURNEY (amb_travel): %s\n", ok ? "ok" : "FALHOU"); if (!ok) bad++;
    }
    {   /* ACORDES EM TEMPO REAL: pad muda o grau na hora; modificadores mudam as notas; MANUAL nao anda sozinho */
        AmbPatch ap; amb_default(&ap); amb_set_index(&ap, A_PROG, P_MANUAL); ap.lead_on = 0; ap.cgen = 5;
        AmbIn in = { 0, 0, 0, 0, 0, 0 }; amb_init(&A, 3);
        for (int b = 0; b < 40; b++) amb_render(&A, &ap, &in, ab, 512);
        int d0 = A.cur_deg_ui; for (int b = 0; b < 400; b++) amb_render(&A, &ap, &in, ab, 512);
        int still = A.cur_deg_ui == d0;
        ap.cdeg = 4; ap.cgen++; for (int b = 0; b < 6; b++) amb_render(&A, &ap, &in, ab, 512);
        int pad_ok = A.cur_deg_ui == 4;
        int o1[4], o2[4]; amb_chord_tones(&ap, 4, o1); ap.cmod = CM_7 | CM_FLIP; amb_chord_tones(&ap, 4, o2);
        int mod_ok = o1[3] != o2[3] || o1[1] != o2[1];
        char lb[12]; amb_chord_label(&ap, 0, lb, sizeof lb);
        printf("acordes em tempo real: MANUAL parado %s | pad muda grau %s | modificadores %s | rotulo do acorde 0 = %s\n", still ? "ok" : "FALHOU", pad_ok ? "ok" : "FALHOU", mod_ok ? "ok" : "FALHOU", lb);
        if (!still || !pad_ok || !mod_ok) bad++;
        AmbPatch g; amb_default(&g); amb_dice_chords(&g, 1234); int n1 = amb_plen(&g);
        AmbPatch g2 = g; amb_set_slot(&g2, 2, 5);
        printf("DICE de acordes: %d slots, slot editado -> %s\n", n1, amb_prog_name(amb_ival(&g2, A_PROG)));
        if (amb_ival(&g2, A_PROG) != P_SLOTS) bad++;
        int h = 0; for (int p = 0; p < 16; p++) h += amb_euclid_hit(5, 16, 0, p);
        printf("euclid 5/16: %d hits %s\n", h, h == 5 ? "ok" : "FALHOU"); if (h != 5) bad++;
    }
    {   /* liga/desliga das camadas: tudo desligado = silencio; analogico nao pode gerar NaN */
        AmbPatch ap; amb_default(&ap); ap.drone_on = 0; ap.lead_on = 0; AmbIn in = { 0, 0, 0, 0, 1.0f, -1.0f };
        amb_init(&A, 9); double sq = 0; int n = 0;
        for (int b = 0; b < 400; b++) { amb_render(&A, &ap, &in, ab, 512); if (b > 100) for (int i = 0; i < 512; i++) { sq += (double)ab[i * 2] * ab[i * 2]; n++; } }
        double rms = sqrt(sq / n) / 32768.0; printf("camadas desligadas: rms %.5f %s\n", rms, rms < 0.002 ? "ok" : "(AINDA SOA)");
        if (rms >= 0.002) bad++;
    }
    {   /* estresse: parametros aleatorios e extremos, pads e modificadores mudando durante o som */
        int fails = 0; clock_t t0 = clock(); long fr = 0;
        for (int t = 0; t < 600; t++) {
            AmbPatch ap; amb_default(&ap);
            for (int i = 0; i < A_COUNT; i++) ap.p[i] = (rand() & 1) ? (float)(rand() & 1) : rand() / (float)RAND_MAX;
            ap.mask = (uint16_t)rand();
            for (int i = 0; i < 8; i++) { ap.seq[i] = (int8_t)((rand() % 11) - 1); ap.prog[i] = (int8_t)((rand() % 8) - 1); }
            amb_init(&A, 7 + t);
            for (int b = 0; b < 10; b++) {
                if (b == 4) { amb_set_from_pos(&ap, A_LMODE, (rand() % 5 + .5f) / 5); ap.drone_on = rand() & 1; ap.lead_on = rand() & 1; }
                if (b % 3 == 0) { ap.cdeg = (uint8_t)(rand() % 7); ap.cmod = (uint8_t)(rand() & 31); ap.cgen++; }
                AmbIn in = { b & 1, rand() / (float)RAND_MAX, rand() / (float)RAND_MAX, b == 6, (rand() / (float)RAND_MAX) * 2 - 1, (rand() / (float)RAND_MAX) * 2 - 1 };
                amb_render(&A, &ap, &in, ab, 512); fr += 512;
                for (int i = 0; i < A_COUNT; i++) if (!(A.sm[i] == A.sm[i])) fails++;
                if (!(A.gain == A.gain) || !(A.lpl[0] == A.lpl[0]) || A.cur_deg_ui < 0 || A.cur_deg_ui > 6) fails++;
            }
        }
        double dt = (double)(clock() - t0) / CLOCKS_PER_SEC;
        printf("estresse ambient: falhas %d | custo medio no PC: %.0f ns/amostra\n", fails, dt / fr * 1e9); bad += fails;
    }

    {   /* HOWL / MIC e DAMAGE */
        static int16_t hb[512 * 2], ref[512 * 2];
        for (int mic = 0; mic < 2; mic++) {
            static Synth H; float hv[P_COUNT]; patch_default(hv);
            hv[P_OSCLVL] = 0; hv[P_NAMT] = 0; hv[P_FB] = 0; hv[P_LFOD] = 0; hv[P_DRIVE] = 0; hv[P_VOL] = .5f;
            double rms[2];
            for (int on = 0; on < 2; on++) {
                synth_init(&H, 5); hv[P_HOWL] = on ? .8f : 0; hv[P_HMODE] = mic ? .75f : .25f;
                double e = 0; int pk = 0;
                for (int b = 0; b < 200; b++) { synth_render(&H, hv, 0, 0, 0, 0, hb, 512);
                    if (b >= 100) for (int i = 0; i < 1024; i++) { e += (double)hb[i] * hb[i]; int q = abs(hb[i]); if (q > pk) pk = q; } }
                rms[on] = sqrt(e / (100 * 1024)) / 32768.0;
            }
            int ok = rms[1] > 0.05 && rms[1] > rms[0] * 4;
            printf("HOWL %s: rms sem %.4f | com %.4f -> %s\n", mic ? "MIC" : "STRING", rms[0], rms[1], ok ? "ok" : "FALHOU");
            if (!ok) bad++;
        }
        static Damage D;
        for (int c = 0; c < 3; c++) {
            damage_init(&D); double ei = 0, eo = 0; int same = 1; float ph = 0;
            for (int b = 0; b < 400; b++) {
                for (int i = 0; i < 512; i++) { ph += 220.0f / SYN_RATE; if (ph >= 1) ph -= 1; hb[2*i] = hb[2*i+1] = (int16_t)(9000 * sinf(6.2831853f * ph)); }
                memcpy(ref, hb, sizeof hb);
                damage_process(&D, hb, 512, 0.0f, c * .5f);
                if (memcmp(ref, hb, sizeof hb)) same = 0;
            }
            int diff = 0;
            for (int b = 0; b < 400; b++) {
                for (int i = 0; i < 512; i++) { ph += 220.0f / SYN_RATE; if (ph >= 1) ph -= 1; hb[2*i] = hb[2*i+1] = (int16_t)(9000 * sinf(6.2831853f * ph)); }
                memcpy(ref, hb, sizeof hb);
                damage_process(&D, hb, 512, 0.9f, c * .5f);
                diff |= memcmp(ref, hb, sizeof hb) != 0;
                if (b > 20) for (int i = 0; i < 1024; i++) { ei += (double)ref[i] * ref[i]; eo += (double)hb[i] * hb[i]; }
            }
            double ratio = sqrt(eo / ei);
            int ok = same && diff && ratio < 1.1 && ratio > 0.2;
            printf("DAMAGE char %.1f: zero intacto %s | nivel x%.2f -> %s\n", c * .5f, same ? "sim" : "NAO", ratio, ok ? "ok" : "FALHOU");
            if (!ok) bad++;
        }
    }

    {   /* GROOVE: kits, swing, ratchet, humores */
        static int16_t gb[512 * 2]; static Groove G; GrvPat p; static GrvBank BK;
        int fails = 0;
        for (int kit = 0; kit < GRV_KITS; kit++) {
            grv_init(&G); grv_default(&p); p.on = 1; p.kit = (uint8_t)kit;
            double e = 0; int pk = 0;
            for (int b = 0; b < 200; b++) { memset(gb, 0, sizeof gb); grv_render(&G, &p, &BK, 0, 0, gb, 512);
                for (int i = 0; i < 1024; i++) { e += (double)gb[i] * gb[i]; int q = abs(gb[i]); if (q > pk) pk = q; } }
            double rms = sqrt(e / (200 * 1024)) / 32768.0;
            printf("GROOVE %-8s rms %.3f pico %.3f\n", grv_kit_name(kit), rms, pk / 32768.0);
            if (rms < 0.02 || pk >= 32767) fails++;
        }
        /* relogio: comeco de cada passo (swing) e ratchet */
        for (int sw = 0; sw < 2; sw++) {
            grv_init(&G); grv_clear(&p); p.on = 1; p.len = 16; p.bpm = (120.0f - 60.0f) / 140.0f; p.swing = sw ? 0.5f : 0.0f;
            int st[3] = {-1, -1, -1};
            for (int i = 0; i < 12000; i++) { gb[0] = gb[1] = 0; grv_render(&G, &p, &BK, 0, 0, gb, 1);
                if (G.step_ui >= 0 && G.step_ui < 3 && st[G.step_ui] < 0) st[G.step_ui] = i; }
            int d0 = st[1] - st[0], d1 = st[2] - st[1];
            printf("GROOVE swing %d%%: passos %d / %d amostras\n", sw * 50, d0, d1);
            if (sw ? !(d0 > d1 + 1000 && abs(d0 + d1 - 8182) < 4) : abs(d0 - d1) > 1) fails++;
        }
        for (int r = 1; r <= 4; r++) {
            grv_init(&G); grv_clear(&p); p.on = 1; p.swing = 0; p.cell[2][0] = (uint8_t)r;
            int hits = 0; float prev = 0;
            for (int i = 0; i < 5000; i++) { gb[0] = gb[1] = 0; grv_render(&G, &p, &BK, 0, 0, gb, 1); if (G.v[2].env > prev + 0.5f) hits++; prev = G.v[2].env; if (G.step_ui > 0) break; }
            if (hits != r) { fails++; printf("  ratchet x%d deu %d\n", r, hits); }
        }
        {   /* 1.1: CHAIN toca P1 -> P2 -> (P3 vazio pula) -> P1; SAVE/LOAD guarda os 4 padroes */
            memset(&BK, 0, sizeof BK); for (int i = 0; i < GRV_PATS; i++) BK.len[i] = 16;
            BK.cell[0][0][0] = 1; BK.cell[1][1][0] = 1; BK.cell[3][2][0] = 2;      /* P1 kick, P2 caixa, P4 chimbal x2; P3 vazio */
            grv_init(&G); grv_default(&p); p.on = 1; p.bpm = 1.0f;
            int seq[8], ns = 0, last = -1;
            for (int i = 0; i < 400000 && ns < 8; i++) { gb[0] = gb[1] = 0; grv_render(&G, &p, &BK, 0, 1, gb, 1); if (G.cur_ui != last) { last = G.cur_ui; seq[ns++] = last; } }
            printf("CHAIN ordem: %d %d %d %d %d\n", seq[0], seq[1], seq[2], seq[3], seq[4]);
            if (!(seq[0] == 0 && seq[1] == 1 && seq[2] == 3 && seq[3] == 0 && seq[4] == 1)) { fails++; printf("  CHAIN errada\n"); }
            GrvPat q; grv_default(&q); q.kit = 5; q.bpm = 0.25f; q.swing = 0.5f; q.level = 0.75f;
            GrvBank b2; memset(&b2, 0, sizeof b2); b2.len[2] = 14;
            int bad = grv_save("build/drum_test.dat", &q, &BK) != 0;
            bad |= grv_load("build/drum_test.dat", &p, &b2) != 0;
            bad |= memcmp(b2.cell, BK.cell, sizeof BK.cell) != 0 || p.kit != 5 || fabsf(p.bpm - 0.25f) > 1e-6f || fabsf(p.swing - 0.5f) > 1e-6f;
            bad |= grv_load("build/nao_existe.dat", &p, &b2) == 0;
            printf("SAVE/LOAD da bateria: %s\n", bad ? "FALHOU" : "ok");
            if (bad) fails++;
        }
        for (int m = 0; m < GRV_MOODS; m++) {
            grv_default(&p); p.mood = (int8_t)m; grv_dice(&p, 99 + m);
            int n = 0; for (int l = 0; l < GRV_LANES; l++) for (int i = 0; i < GRV_STEPS; i++) { n += p.cell[l][i] != 0; if (p.cell[l][i] > 4) fails++; }
            if (!n || p.len != (m == 2 ? 14 : 16)) { fails++; printf("  humor %d: n=%d len=%d\n", m, n, p.len); }
            AmbPatch am; amb_default(&am); amb_dice_mood(&am, m, 5, 0); amb_dice_mood(&am, m, 5, 1);
            for (int i = 0; i < AMB_PSLOTS; i++) if (am.prog[i] >= amb_nd(&am)) { fails++; printf("  humor %d: grau fora\n", m); }
            static Amb A; static int16_t ab[512 * 2]; amb_init(&A, 3); double e = 0; AmbIn in = {0};
            for (int b = 0; b < 400; b++) { amb_render(&A, &am, &in, ab, 512); for (int i = 0; i < 1024; i++) e += (double)ab[i] * ab[i]; }
            if (e <= 0) { fails++; printf("  humor %d: ambient mudo\n", m); }
        }
        printf("GROOVE (kits, swing, ratchet, 8 humores): %s\n", fails ? "FALHOU" : "ok");
        bad += fails;
    }

    {   /* os 30 demonios nos dois motores */
        static Amb A; static int16_t db[512 * 2]; static AmbPatch all[30]; int fails = 0;
        for (int p = 0; p < N_PRESETS; p++) if (demon_preset(preset_demon(p)) != p) fails++;
        for (int p = 0; p < A_NPRESETS; p++) { AmbPatch q; amb_default(&q); if (amb_load_demon(&q, amb_preset_demon(p), 128) != p) fails++; }
        float lo = 9, hi = 0;
        for (int k = 0; k < 30; k++) {
            amb_default(&all[k]);
            amb_load_demon(&all[k], k, goetia_demon_depth(&goetia_registry[k]));
            for (int j = 0; j < k; j++) if (!memcmp(all[j].p, all[k].p, sizeof all[k].p)) { fails++; printf("  demonios %d e %d iguais\n", j, k); }
            amb_init(&A, 3); AmbIn in = {0}; double e = 0; int pk = 0;
            for (int b = 0; b < 500; b++) { amb_render(&A, &all[k], &in, db, 512); if (b >= 100) for (int i = 0; i < 1024; i++) { e += (double)db[i] * db[i]; int q = abs(db[i]); if (q > pk) pk = q; } }
            float rms = (float)(sqrt(e / (400 * 1024)) / 32768.0);
            if (rms < lo) lo = rms;
            if (rms > hi) hi = rms;
            if (rms < 0.05f || pk >= 32767) { fails++; printf("  demonio %s: rms %.3f pico %d\n", goetia_registry[k].name, rms, pk); }
        }
        printf("30 demonios no ambient: rms %.3f..%.3f -> %s\n", lo, hi, fails ? "FALHOU" : "ok");
        bad += fails;
    }

    printf("\n== GRAVADOR WAV ==\n");
    {
        static int16_t ring[REC_RING_FRAMES * 2]; static Rec R; int16_t blk[512 * 2];
        snprintf(path, sizeof path, "%s", outdir);
        if (rec_open(&R, path, SYN_RATE, ring) != 0) { printf("falha ao abrir\n"); bad++; }
        else {
            long expect = 0; uint32_t crc_in = 0;
            for (int b = 0; b < 200; b++) {                    /* produtor/consumidor intercalados */
                for (int i = 0; i < 512; i++) { blk[i * 2] = (int16_t)(b * 31 + i); blk[i * 2 + 1] = (int16_t)(-(b * 17 + i)); crc_in += (uint16_t)blk[i * 2] + 3u * (uint16_t)blk[i * 2 + 1]; }
                rec_push(&R, blk, 512); expect += 512;
                if (b % 7 == 0) rec_pump(&R);
            }
            int rc = rec_close(&R);
            uint32_t rate; float secs; int wi = rec_wav_info(R.path, &rate, &secs);
            FILE *f = fopen(R.path, "rb"); uint8_t h[44]; fread(h, 1, 44, f);
            uint32_t crc_out = 0; int16_t fr2[2]; long n = 0;
            while (fread(fr2, 2, 2, f) == 2) { crc_out += (uint16_t)fr2[0] + 3u * (uint16_t)fr2[1]; n++; }
            fclose(f);
            uint32_t dsz = h[40] | h[41] << 8 | h[42] << 16 | (uint32_t)h[43] << 24;
            int ok = rc == 0 && wi == 0 && n == expect && crc_in == crc_out && dsz == (uint32_t)expect * 4 && R.dropped == 0;
            printf("%s: %ld quadros, %.3f s, rate %u, dropped %u, conteudo %s -> %s\n", R.path, n, secs, rate, R.dropped,
                   crc_in == crc_out ? "identico" : "DIFERENTE", ok ? "OK" : "FALHOU");
            {   /* 1.1: reproducao da mesma gravacao: tem que sair bit a bit igual, e parar sozinha no fim */
                static Player PL; static int16_t pring[PLAY_RING_FRAMES * 2]; static int16_t po[512 * 2];
                uint32_t crc_p = 0; long np = 0; int pok = play_open(&PL, R.path, pring) == 0;
                for (int it = 0; pok && it < 5000 && PL.active; it++) {
                    play_pump(&PL);
                    int g = play_pull(&PL, po, 512);
                    for (int i = 0; i < g; i++) { crc_p += (uint16_t)po[i * 2] + 3u * (uint16_t)po[i * 2 + 1]; np++; }
                }
                int pgood = pok && !PL.active && np == expect && crc_p == crc_in && fabsf(play_length(&PL) - secs) < 0.001f;
                play_close(&PL);
                printf("REPRODUCAO: %ld quadros, %s, parou sozinha: %s -> %s\n", np, crc_p == crc_in ? "identico" : "DIFERENTE", PL.active ? "nao" : "sim", pgood ? "OK" : "FALHOU");
                if (!pgood) bad++;
                static const char *ruim = "build/nao_wav.wav"; FILE *tf = fopen(ruim, "wb"); fwrite("lixo", 1, 4, tf); fclose(tf);
                if (play_open(&PL, ruim, pring) == 0) { printf("arquivo invalido aceito!\n"); bad++; }
                remove(ruim);
            }
            remove(R.path); if (!ok) bad++;
            /* anel cheio: deve contar perdas sem corromper */
            rec_open(&R, path, SYN_RATE, ring);
            for (int b = 0; b < 80; b++) rec_push(&R, blk, 512);
            printf("anel cheio sem consumidor: dropped=%u (esperado > 0)\n", R.dropped); if (!R.dropped) bad++;
            rec_close(&R); remove(R.path);
        }
    }

    printf("\n== UI ==\n");
    static uint16_t top[400 * 240], bot[320 * 240];
    Surf st = { top, 400, 240 }, sb = { bot, 320, 240 };
    float v[P_COUNT]; patch_default(v); patch_preset(v, 2);
    synth_init(&S, 7);
    for (int b = 0; b < 8; b++) synth_render(&S, v, 0, 0, 0, 0, buf, 512);
    static float snap[SCOPE_N]; int w = S.scope_w;
    for (int i = 0; i < SCOPE_N; i++) snap[i] = S.scope[(w - SCOPE_N + i) & (SCOPE_N - 1)];
    int trig = 0; for (int i = 0; i < SCOPE_N / 2; i++) if (snap[i] < 0 && snap[i+1] >= 0) { trig = i; break; }
    static FileEnt fl[2] = {{"REC_0002.wav", 75.0f, 32728}, {"REC_0001.wav", 12.4f, 32728}};
    AmbPatch ap; amb_default(&ap); float ve[AMB_V] = {.3f,.2f,.5f,0,.1f,0,0,0,.4f,0};
    int xi, yi; patch_xy(2, &xi, &yi);
    UiState u = { .page = 0, .sub = 0, .v = v, .module = 0, .row = 1, .preset = 2, .modified = 1, .ap = &ap, .venv = ve,
                  .notes_pc = 0x205, .cur_note = 57, .level = 0.7f, .scope = snap + trig, .spp = 2, .frame = 41,
                  .xid = xi, .yid = yi, .touching = 1, .tx = .3f, .ty = .7f, .rec_on = 1, .rec_secs = 75.0f,
                  .set_v = {.35f,1,0,3,1,0,0,0}, .files = fl, .nfiles = 2 };
    static GrvPat gp; grv_default(&gp); gp.on = 1; gp.mood = 3; grv_dice(&gp, 77); u.grv = &gp; u.grv_step = 5;
    static GrvBank gbk; for (int i = 0; i < GRV_PATS; i++) gbk.len[i] = 16; memcpy(gbk.cell[0], gp.cell, sizeof gp.cell); gbk.cell[2][1][3] = 1;
    u.gbank = &gbk; u.gsel = 0; u.gchain = 1; u.gcur = 2; u.hon = 1; u.aon = 1; u.batt = 3; u.batt_chg = 1;
    /* telas de referencia (para inspecao visual) */
    for (int pg = 0; pg < PG_COUNT; pg++) for (int sub = 0; sub < 2; sub++) {
        if (sub && pg != PG_HARSH && pg != PG_AMB) continue;
        u.page = pg; u.sub = sub; u.engine = (pg == PG_AMB || pg == PG_CDXA);
        ui_draw_bottom(&sb, &u); snprintf(path, sizeof path, "%s/bot_%d_%d.ppm", outdir, pg, sub); save_ppm(path, &sb);
    }
    u.page = 0; u.engine = 0; ui_draw_top(&st, &u); snprintf(path, sizeof path, "%s/top_harsh.ppm", outdir); save_ppm(path, &st);
    u.engine = 1; ui_draw_top(&st, &u); snprintf(path, sizeof path, "%s/top_amb.ppm", outdir); save_ppm(path, &st);
    ui_draw_error(&sb, 0); snprintf(path, sizeof path, "%s/err.ppm", outdir); save_ppm(path, &sb);

    /* BUG VISUAL: nenhum visual pode escrever fora da janela 132x132 (x 8..140, y 26..158) */
    {
        int leaks = 0;
        for (int id = 0; id < ART_SCENES; id++) for (int t = 0; t < 400; t += 7) for (int lvi = 0; lvi < 3; lvi++) {
            static uint16_t sent[400 * 240]; for (int i = 0; i < 400 * 240; i++) sent[i] = 0x1234;
            Surf ss = { sent, 400, 240 }; ui_vis_test(&ss, id, t, lvi * 0.5f);
            for (int x = 0; x < 400; x++) for (int y = 0; y < 240; y++) {
                int inside = (x >= 8 && x < 140 && y >= 26 && y < 158);
                if (!inside && sent[x * 240 + (239 - y)] != 0x1234) { leaks++; break; }
            }
        }
        printf("visuais fora da janela (vazamentos de desenho): %d\n", leaks); bad += leaks;
    }
    /* todas as combinacoes de pagina nao podem escrever fora do framebuffer (guarda de memoria) */
    {
        static uint16_t guard[(320 * 240) + 2 * 4096]; int over = 0;
        for (int pg = 0; pg < PG_COUNT; pg++) for (int sub = 0; sub < 2; sub++) for (int pr = -1; pr < N_PRESETS; pr++) {
            for (size_t i = 0; i < sizeof guard / 2; i++) guard[i] = 0xABCD;
            Surf sg = { guard + 4096, 320, 240 };
            u.page = pg; u.sub = sub; u.preset = pr; u.module = pr < 0 ? 0 : pr % N_MODULES; u.engine = (pg == PG_AMB || pg == PG_CDXA); u.pane = pr & 1; u.holdy = pr * 5;
            if (u.engine && pr >= 0 && (pr & 2)) u.module = pr % A_NGROUPS;
            ui_draw_bottom(&sg, &u);
            for (int i = 0; i < 4096; i++) if (guard[i] != 0xABCD || guard[4096 + 320 * 240 + i] != 0xABCD) over++;
            static uint16_t g2[400 * 240 + 2 * 4096];
            for (size_t i = 0; i < sizeof g2 / 2; i++) g2[i] = 0xABCD;
            Surf s2 = { g2 + 4096, 400, 240 }; u.frame = pr * 13 + 5; ui_draw_top(&s2, &u);
            for (int i = 0; i < 4096; i++) if (g2[i] != 0xABCD || g2[4096 + 400 * 240 + i] != 0xABCD) over++;
        }
        printf("escritas fora do framebuffer: %d\n", over); bad += over;
    }
    /* telas dos 16 presets (miniaturas) */
    for (int pr = 0; pr < N_PRESETS; pr++) {
        float vv[P_COUNT]; patch_default(vv); patch_preset(vv, pr);
        u.v = vv; u.page = 0; u.sub = 0; u.engine = 0; u.preset = pr; u.modified = 0; u.frame = 47; u.level = 0.55f;
        ui_draw_top(&st, &u); snprintf(path, sizeof path, "%s/vis_%d.ppm", outdir, pr); save_ppm(path, &st);
    }
    u.v = v;
    for (int sc = 0; sc < 4; sc++) {
        u.page = PG_AMB; u.engine = 1; u.apreset = sc * 4; u.frame = 61 + sc * 9; u.level = 0.5f; ap.scene = (uint8_t)sc; amb_preset(&ap, sc * 4);
        ui_draw_top(&st, &u); snprintf(path, sizeof path, "%s/vis_amb%d.ppm", outdir, sc); save_ppm(path, &st);
    }
    amb_default(&ap); u.engine = 0; u.page = PG_HARSH;

    /* telas extras: ambient em cada modo do lead (pane LEAD), pane DRONE com acordes/modificadores, presets de ambient */
    u.cur_deg = 3; u.cur_slot = 1; u.step_ui = 2; u.nsteps_ui = 8; ap.cmod = CM_7;
    for (int md = 0; md < A_NLMODE; md++) {
        amb_set_index(&ap, A_LMODE, md); u.page = PG_AMB; u.sub = 0; u.engine = 1; u.touching = md == L_KEYS; u.pane = 1;
        ui_draw_bottom(&sb, &u); snprintf(path, sizeof path, "%s/bot_amb_lead%d.ppm", outdir, md); save_ppm(path, &sb);
    }
    u.pane = 0; u.touching = 0; u.write_on = 1;
    ui_draw_bottom(&sb, &u); snprintf(path, sizeof path, "%s/bot_amb_drone.ppm", outdir); save_ppm(path, &sb);
    u.write_on = 0; ap.cmod = 0;
    u.page = PG_CDXH; u.codex_sel = 7; ui_draw_bottom(&sb, &u); snprintf(path, sizeof path, "%s/bot_codex.ppm", outdir); save_ppm(path, &sb); u.codex_sel = 0;
    u.page = PG_CDXA; u.codex_sel = 3; ui_draw_bottom(&sb, &u); snprintf(path, sizeof path, "%s/bot_cdxa.ppm", outdir); save_ppm(path, &sb); u.codex_sel = 0;
    u.page = PG_DRM; ui_draw_bottom(&sb, &u); snprintf(path, sizeof path, "%s/bot_drm.ppm", outdir); save_ppm(path, &sb);
    u.pane = 0; u.page = PG_AMB; u.apreset = 0;
    amb_set_index(&ap, A_LMODE, L_DRIFT); u.touching = 0;
    for (int g = 0; g < A_NGROUPS; g++) {
        u.page = PG_AMB; u.sub = 1; u.module = g; u.row = 1;
        ui_draw_bottom(&sb, &u); snprintf(path, sizeof path, "%s/bot_amb_edit%d.ppm", outdir, g); save_ppm(path, &sb);
    }
    for (int m = 0; m < N_MODULES; m++) {
        u.page = PG_HARSH; u.sub = 1; u.module = m; u.row = 0;
        ui_draw_bottom(&sb, &u); snprintf(path, sizeof path, "%s/bot_harsh_edit%d.ppm", outdir, m); save_ppm(path, &sb);
    }
    u.module = 0; u.engine = 0;
    {
        static FileEnt fl2[3] = {{"A_VERY_LONG_RECORDING_NAME_X.wav", 75.0f, 32728}, {"REC_0001.wav", 12.4f, 32728}, {"NOISE-2.wav", 3.2f, 32728}};
        u.playing = 1; u.play_idx = 1; u.play_pos = 5.0f; u.play_len = 12.4f;
        u.page = PG_FILES; u.sub = 0; u.files = fl2; u.nfiles = 3; u.fsel = 1; u.del_arm = 0;
        ui_draw_bottom(&sb, &u); snprintf(path, sizeof path, "%s/bot_files_armed.ppm", outdir); save_ppm(path, &sb);
        /* TEXTO NAO PODE VAZAR: entre o fim do nome (x 210) e a duracao (x >= 262) a linha precisa ficar lisa */
        int leak = 0; uint16_t ref = sb.px[230 * 240 + (239 - 50)];
        for (int x = 214; x < 262; x++) for (int y = 47; y < 63; y++) if (sb.px[x * 240 + (239 - y)] != ref) leak++;
        printf("texto da lista de arquivos vazando da coluna: %d px\n", leak); bad += leak;
        u.files = fl; u.nfiles = 2; u.del_arm = 0;
    }
    u.page = PG_HARSH; u.sub = 0; u.module = 0; u.preset = 2;

    /* toque: cada controle responde onde o desenho esta */
    {
        int fails = 0; UiHit h;
        #define EXPECT(cond, msg) do { if (!(cond)) { fails++; printf("  falhou: %s\n", msg); } } while (0)
        u.page = PG_HARSH; u.sub = 0; u.ap = &ap;
        h = ui_hit(&u, 106, 100); EXPECT(h.kind == H_PAD && h.fx > 0.4f && h.fx < 0.6f, "pad harsh centro");
        h = ui_hit(&u, 6, 169);   EXPECT(h.kind == H_PAD && h.fy < 0.02f, "pad harsh canto");
        h = ui_hit(&u, 236, 45);  EXPECT(h.kind == H_PREV, "prev");
        h = ui_hit(&u, 290, 45);  EXPECT(h.kind == H_NEXT, "next");
        h = ui_hit(&u, 260, 100); EXPECT(h.kind == H_MUTATE, "mutate");
        h = ui_hit(&u, 260, 130); EXPECT(h.kind == H_RANDOM, "random");
        h = ui_hit(&u, 260, 158); EXPECT(h.kind == H_EDIT, "edit");
        h = ui_hit(&u, 170, 185); EXPECT(h.kind == H_VOL && h.fx > 0.4f && h.fx < 0.5f, "volume");
        h = ui_hit(&u, 303, 12);  EXPECT(h.kind == H_REC, "rec");
        h = ui_hit(&u, 268, 12);  EXPECT(h.kind == H_PAUSE, "pause");
        h = ui_hit(&u, 40, 186);  EXPECT(h.kind == H_HPLAY, "harsh: botao play/pause");
        h = ui_hit(&u, 2 * 36 + 14, 10);  EXPECT(h.kind == H_TAB && h.arg == PG_AMB, "aba amb");
        h = ui_hit(&u, 1 * 36 + 14, 10);  EXPECT(h.kind == H_TAB && h.arg == PG_CDXH, "aba cdx-h logo depois do harsh");
        h = ui_hit(&u, 3 * 36 + 14, 10);  EXPECT(h.kind == H_TAB && h.arg == PG_CDXA, "aba cdx-a depois do ambient");
        h = ui_hit(&u, 4 * 36 + 14, 10);  EXPECT(h.kind == H_TAB && h.arg == PG_DRM, "aba drm");
        h = ui_hit(&u, 5 * 36 + 14, 10);  EXPECT(h.kind == H_TAB && h.arg == PG_FILES, "aba wav");
        h = ui_hit(&u, PG_SET * 36 + 14, 10);  EXPECT(h.kind == H_TAB && h.arg == PG_SET, "aba set");
        /* editor harsh */
        u.sub = 1; u.module = 0;
        h = ui_hit(&u, 90, 40);   EXPECT(h.kind == H_MODTILE && h.arg == 1, "tile modulo 1");
        h = ui_hit(&u, 230, 80);  EXPECT(h.kind == H_PILL && h.arg == P_WAVE && h.arg2 == 2, "WAVE = pills");
        h = ui_hit(&u, 150, 102); EXPECT(h.kind == H_SLIDER && h.arg == P_PITCH, "PITCH = slider");
        u.module = 1;
        h = ui_hit(&u, 300, 80);  EXPECT(h.kind == H_STEPR && h.arg == P_NTYPE, "NOISE TYPE (5 opcoes) = seta");
        h = ui_hit(&u, 40, 80);   EXPECT(h.kind == H_NONE, "toque no rotulo nao e controle");
        /* ambient: tocar - camada DRONE (acordes em tempo real) */
        u.page = PG_AMB; u.sub = 0; u.module = 0; u.pane = 0; amb_set_index(&ap, A_LMODE, L_DRIFT); ap.lead_on = 1; amb_set_index(&ap, A_SCALE, 1);
        h = ui_hit(&u, 50, 40);   EXPECT(h.kind == H_DRONE, "camada drone");
        h = ui_hit(&u, 150, 40);  EXPECT(h.kind == H_LEAD, "camada lead");
        h = ui_hit(&u, 25, 65);   EXPECT(h.kind == H_TIMB && h.arg == 0, "chip de timbre 0 (sempre visivel)");
        h = ui_hit(&u, 290, 65);  EXPECT(h.kind == H_TIMB && h.arg == 7, "chip de timbre 7");
        h = ui_hit(&u, 20, 100);  EXPECT(h.kind == H_CPAD && h.arg == 0, "pad de acorde I");
        h = ui_hit(&u, 300, 100); EXPECT(h.kind == H_CPAD && h.arg == 6, "pad de acorde VII");
        h = ui_hit(&u, 20, 142);  EXPECT(h.kind == H_CMOD && h.arg == CM_7, "modificador 7TH");
        h = ui_hit(&u, 290, 142); EXPECT(h.kind == H_CMOD && h.arg == CM_INV, "modificador INV");
        h = ui_hit(&u, 20, 175);  EXPECT(h.kind == H_CSLOT && h.arg == 0, "slot de acorde 1");
        h = ui_hit(&u, 290, 175); EXPECT(h.kind == H_CSLOT && h.arg == 7, "slot de acorde 8");
        h = ui_hit(&u, 185, 40);  EXPECT(h.kind == H_APLAY, "ambient: botao play/pause");
        h = ui_hit(&u, 235, 40);  EXPECT(h.kind == H_CWRITE, "WRITE");
        h = ui_hit(&u, 290, 40);  EXPECT(h.kind == H_CDICE, "DICE acordes");
        /* camada LEAD: modos sao abas, cada um com seu controle */
        u.pane = 1;
        h = ui_hit(&u, 235, 40);  EXPECT(h.kind == H_LDICE, "DICE lead");
        h = ui_hit(&u, 290, 40);  EXPECT(h.kind == H_LXTRA, "extra do modo");
        h = ui_hit(&u, 30, 88);   EXPECT(h.kind == H_LMODE && h.arg == 0, "aba DRIFT");
        h = ui_hit(&u, 280, 88);  EXPECT(h.kind == H_LMODE && h.arg == 4, "aba KEYS");
        h = ui_hit(&u, 100, 130); EXPECT(h.kind == H_APAD, "pad do DRIFT");
        h = ui_hit(&u, 15, 190);  EXPECT(h.kind == H_KEY && h.arg == 0, "tecla de nota 0");
        h = ui_hit(&u, 300, 190); EXPECT(h.kind == H_KEY && h.arg == 11, "tecla de nota 11");
        amb_set_index(&ap, A_LMODE, L_ARP);
        h = ui_hit(&u, 40, 110);  EXPECT(h.kind == H_PILL && h.arg == A_LDIR && h.arg2 == 0, "ARP: direcao UP");
        h = ui_hit(&u, 290, 110); EXPECT(h.kind == H_PILL && h.arg == A_LDIR && h.arg2 == 4, "ARP: direcao WALK");
        h = ui_hit(&u, 100, 150); EXPECT(h.kind == H_APAD, "pad do ARP");
        amb_set_index(&ap, A_LMODE, L_SEQ);
        h = ui_hit(&u, 100, 110); EXPECT(h.kind == H_SEQCELL && h.arg == 2 && h.arg2 == 9, "SEQ: passo 3, nota alta");
        h = ui_hit(&u, 100, 199); EXPECT(h.kind == H_SEQCELL && h.arg == 2 && h.arg2 == -1, "SEQ: linha de pausa");
        { int st, vv; EXPECT(ui_seq_cell(6, 102, &st, &vv) && st == 0 && vv == 9, "ui_seq_cell canto"); EXPECT(!ui_seq_cell(5, 102, &st, &vv), "ui_seq_cell fora"); }
        amb_set_index(&ap, A_LMODE, L_EUCL);
        h = ui_hit(&u, 200, 112); EXPECT(h.kind == H_EPAR && h.arg == A_EHITS, "EUCLID: HITS");
        h = ui_hit(&u, 200, 160); EXPECT(h.kind == H_EPAR && h.arg == A_EROT, "EUCLID: ROTATE");
        h = ui_hit(&u, 280, 190); EXPECT(h.kind == H_PILL && h.arg == A_ECONT && h.arg2 == 4, "EUCLID: contorno RND");
        amb_set_index(&ap, A_LMODE, L_KEYS);
        h = ui_hit(&u, 100, 130); EXPECT(h.kind == H_APAD, "KEYS: teclado");
        ap.lead_on = 1; amb_set_index(&ap, A_LMODE, L_DRIFT); u.pane = 0;
        /* ambient: editor */
        u.sub = 1; u.module = 0;
        h = ui_hit(&u, 110, 40);  EXPECT(h.kind == H_MODTILE && h.arg == 1, "grupo LEAD");
        h = ui_hit(&u, 300, 80);  EXPECT(h.kind == H_STEPR && h.arg == A_DSND, "som do drone = seta");
        u.module = 1;
        h = ui_hit(&u, 200, 124); EXPECT(h.kind == H_PILL && h.arg == A_LOCT && h.arg2 == 1, "REGISTER (3 opcoes) = pills");
        h = ui_hit(&u, 150, 146); EXPECT(h.kind == H_SLIDER && h.arg == A_SPEED, "SPEED = slider");
        /* regra geral: todo parametro tem o controle certo, e a linha cabe na tela */
        for (int m = 0; m < N_MODULES; m++) {
            u.page = PG_HARSH; u.sub = 1; u.module = m;
            for (int r = 0; r < modules[m].count; r++) {
                int id = modules[m].first + r, n = param_discrete(id), y = 70 + r * 22 + 10;
                int xs = n == 0 ? 150 : (n <= 4 ? 100 + 0 : 300); if (n >= 2 && n <= 4) xs = 94;
                int want = n == 0 ? H_SLIDER : (n <= 4 ? H_PILL : H_STEPR);
                h = ui_hit(&u, xs, y);
                if (h.kind != want || h.arg != id || y + 10 > 240) { fails++; printf("  harsh %s: controle errado (kind %d)\n", param_name(id), h.kind); }
            }
        }
        for (int g = 0; g < A_NGROUPS; g++) {
            u.page = PG_AMB; u.sub = 1; u.module = g;
            for (int r = 0; r < amb_groups[g].count; r++) {
                int id = amb_groups[g].first + r, n = amb_discrete(id), y = 70 + r * 22 + 10;
                int xs = n == 0 ? 150 : (n <= 4 ? 94 : 300);
                int want = n == 0 ? H_SLIDER : (n <= 4 ? H_PILL : H_STEPR);
                h = ui_hit(&u, xs, y);
                if (h.kind != want || h.arg != id || y + 10 > 240) { fails++; printf("  amb %s: controle errado (kind %d)\n", amb_pname(id), h.kind); }
            }
        }
        u.page = PG_CDXH; h = ui_hit(&u, 4 + 52 * 2 + 5, 30 + 41 * 3 + 5); EXPECT(h.kind == H_CODEX && h.arg == 20, "cdx-h: tile 20");
        h = ui_hit(&u, 4 + 5, 30 + 5); EXPECT(h.kind == H_CODEX && h.arg == 0, "cdx-h: tile 0");
        u.page = PG_CDXA; h = ui_hit(&u, 4 + 52 * 5 + 5, 30 + 41 * 4 + 5); EXPECT(h.kind == H_CODEXA && h.arg == 29, "cdx-a: tile 29 (ambient)");
        u.page = PG_AMB; u.sub = 0; h = ui_hit(&u, 20, 225); EXPECT(h.kind == H_FREEZE, "botao FREEZE");
        h = ui_hit(&u, 290, 225); EXPECT(h.kind == H_THROW, "botao THROW");
        h = ui_hit(&u, 150, 225); EXPECT(h.kind == H_AVOL, "slider master continua");
        u.page = PG_DRM;
        h = ui_hit(&u, 30, 40);   EXPECT(h.kind == H_GPLAY, "drm: play");
        h = ui_hit(&u, 80, 40);   EXPECT(h.kind == H_GKIT && h.arg == -1, "drm: kit anterior");
        h = ui_hit(&u, 200, 40);  EXPECT(h.kind == H_GKIT && h.arg == 1, "drm: kit seguinte");
        h = ui_hit(&u, 240, 40);  EXPECT(h.kind == H_GDICE, "drm: dice");
        h = ui_hit(&u, 295, 40);  EXPECT(h.kind == H_GCLR, "drm: clr");
        h = ui_hit(&u, 40, 60);   EXPECT(h.kind == H_GCELL && h.arg == 0 && h.arg2 == 0, "drm: celula 1");
        h = ui_hit(&u, 36 + 15 * 17 + 3 + 4, 57 + 3 * 22 + 5); EXPECT(h.kind == H_GCELL && h.arg == 3 && h.arg2 == 15, "drm: ultima celula");
        h = ui_hit(&u, 4 + 39 * 7 + 5, 155); EXPECT(h.kind == H_GMOOD && h.arg == 7, "drm: humor HUSH");
        h = ui_hit(&u, 4 + 34 * 3 + 5, 178); EXPECT(h.kind == H_GPAT && h.arg == 3, "drm: padrao P4");
        h = ui_hit(&u, 20, 178);  EXPECT(h.kind == H_GPAT && h.arg == 0, "drm: padrao P1");
        h = ui_hit(&u, 170, 178); EXPECT(h.kind == H_GCHAIN, "drm: CHAIN");
        h = ui_hit(&u, 230, 178); EXPECT(h.kind == H_GSAVE, "drm: SAVE");
        h = ui_hit(&u, 290, 178); EXPECT(h.kind == H_GLOAD, "drm: LOAD");
        h = ui_hit(&u, 163, 191 + 16 + 8); EXPECT(h.kind == H_GSLIDE && h.arg == 1 && h.fx > 0.45f && h.fx < 0.55f, "drm: swing");
        u.page = PG_FILES; u.nfiles = 2; u.fscroll = 0;
        h = ui_hit(&u, 100, 55);  EXPECT(h.kind == H_FILE && h.arg == 0, "arquivo 0");
        h = ui_hit(&u, 100, 75);  EXPECT(h.kind == H_FILE && h.arg == 1, "arquivo 1");
        h = ui_hit(&u, 100, 95);  EXPECT(h.kind == H_NONE, "linha sem arquivo");
        h = ui_hit(&u, 30, 220);  EXPECT(h.kind == H_FUP, "arquivos: sobe");
        h = ui_hit(&u, 55, 220);  EXPECT(h.kind == H_FDN, "arquivos: desce");
        h = ui_hit(&u, 110, 220); EXPECT(h.kind == H_FPLAY, "arquivos: play");
        h = ui_hit(&u, 190, 220); EXPECT(h.kind == H_REN, "renomear");
        h = ui_hit(&u, 260, 220); EXPECT(h.kind == H_DEL, "apagar");
        u.page = PG_SET;
        h = ui_hit(&u, 150, 44);  EXPECT(h.kind == H_SLIDER && h.arg == 0, "SMOOTH slider");
        h = ui_hit(&u, 250, 34 + 19 * 2 + 9); EXPECT(h.kind == H_PILL && h.arg == 2 && h.arg2 == 1, "LITE ON");
        h = ui_hit(&u, 150, 34 + 19 * 8 + 9); EXPECT(h.kind == H_SLIDER && h.arg == 8, "DAMAGE slider");
        h = ui_hit(&u, 150, 34 + 19 * 9 + 9); EXPECT(h.kind == H_SLIDER && h.arg == 9, "DMG CHAR slider");
        h = ui_hit(&u, 40, 34 + 19 * 3 + 9); EXPECT(h.kind == H_NONE, "rotulo STICKS");
        h = ui_hit(&u, 300, 34 + 19 * 3 + 9); EXPECT(h.kind == H_PILL && h.arg == 3 && h.arg2 == 3, "STICKS: BOTH");
        h = ui_hit(&u, 200, 34 + 19 * 4 + 9); EXPECT(h.kind == H_PILL && h.arg == 4 && h.arg2 == 1, "HOLD Y: 2 S");
        h = ui_hit(&u, 300, 34 + 19 * 5 + 9); EXPECT(h.kind == H_PILL && h.arg == 5 && h.arg2 == 3, "AUTO MUT: 40 S");
        h = ui_hit(&u, 300, 34 + 19 * 6 + 9); EXPECT(h.kind == H_PILL && h.arg == 6 && h.arg2 == 3, "JOURNEY: 120 S");
        h = ui_hit(&u, 300, 34 + 19 * 7 + 9); EXPECT(h.kind == H_PILL && h.arg == 7 && h.arg2 == 3, "ALIVE: HIGH");
        /* nenhum pixel da tela devolve um hit invalido */
        for (int pg = 0; pg < PG_COUNT; pg++) for (int sb2 = 0; sb2 < 2; sb2++) for (int md = 0; md < A_NLMODE; md++) for (int pn = 0; pn < 2; pn++) {
            u.page = pg; u.sub = sb2; u.module = md % 4; u.pane = pn; amb_set_index(&ap, A_LMODE, md);
            for (int x = 0; x < 320; x++) for (int y = 0; y < 240; y++) {
                h = ui_hit(&u, x, y);
                if (h.kind < 0 || h.kind > H_FPLAY || (h.kind == H_GCELL && (h.arg < 0 || h.arg >= GRV_LANES || h.arg2 < 0 || h.arg2 >= GRV_STEPS)) || ((h.kind == H_CODEX || h.kind == H_CODEXA) && (h.arg < 0 || h.arg > 29)) || (h.kind == H_GPAT && (h.arg < 0 || h.arg >= GRV_PATS)) || h.fx < 0 || h.fx > 1 || h.fy < 0 || h.fy > 1) fails++;
                if ((h.kind == H_KEY && (h.arg < 0 || h.arg > 11)) || (h.kind == H_SEQCELL && (h.arg < 0 || h.arg > 7 || h.arg2 < -1 || h.arg2 > 9)) || (h.kind == H_CPAD && (h.arg < 0 || h.arg >= amb_nd(&ap))) || (h.kind == H_CSLOT && (h.arg < 0 || h.arg >= AMB_PSLOTS)) || (h.kind == H_TIMB && (h.arg < 0 || h.arg >= A_NSND)) || 
                    (h.kind == H_FILE && (h.arg < 0 || h.arg >= u.nfiles))) fails++;
            }
        }
        printf("mapa de toque: %d falhas\n", fails); bad += fails;
    }
    printf("\nRESULTADO: %s\n", bad ? "PROBLEMAS" : "TUDO OK");
    return bad != 0;
}
