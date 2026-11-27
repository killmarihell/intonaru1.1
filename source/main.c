/* INTONARUMORI 1.1 - instrumento de noise / ambient para Nintendo 3DS (Old 3DS + Luma3DS).
 * Toda a performance funciona com botoes + toque. Analogico (Circle Pad e/ou C-Stick, SET > STICKS):
 * harsh X = bend, Y = drive; ambient X = bend do lead, Y = brilho. Y segurado 2-3 s = PAUSE. */
#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>
#include <strings.h>
#include <time.h>
#include "synth.h"
#include "amb.h"
#include "rec.h"
#include "damage.h"
#include "groove.h"
#include "ui.h"
#include "art.h"
#include "goetia.h"

#define NBUF   4          /* 4 buffers x 512 quadros = ~62 ms de fila de audio */
#define FRAMES 512
#define REC_DIR "sdmc:/3ds/intonarumori/rec"

static Synth      synth;
static Amb        amb;
static float      patch[P_COUNT];
static AmbPatch   ap;
static volatile int g_burst, g_mute, g_engine;       /* g_engine: motor em foco na tela de cima (0 = harsh, 1 = ambient) */
static volatile int g_hon, g_aon;                /* 1.1: harsh e ambient ligados (cada um pausa sozinho; a bateria usa gpat.on) */
static volatile int g_pause;                         /* 1 = pausado: nao renderiza, fade e silencio */
static volatile int g_freeze, g_throw, g_alive;
static Damage     dmg;
static Groove     grv;
static GrvPat     gpat;
static GrvBank    gbank;                             /* 1.1: 4 padroes */
static volatile int g_gsel, g_gchain;                /* padrao editado, CHAIN */
#define DRUM_FILE "sdmc:/3ds/intonarumori/drum.dat"
static volatile float g_dmg, g_dmgc;                 /* DAMAGE no master (SET) */
static volatile int g_atouch; static volatile float g_ax, g_ay;
static volatile float g_cx, g_cy;                    /* C-Stick (-1..1) */
static float      pgain = 1.0f;                      /* ganho do fade de pausa (so a thread de audio mexe) */

static Player     player;                            /* 1.1: reproducao das gravacoes (aba WAV) */
static int16_t    play_ring[PLAY_RING_FRAMES * 2];
static Rec        rec;
static int16_t    rec_ring[REC_RING_FRAMES * 2];
static LightLock  rec_lock;
static volatile bool quit_writer;

static ndspWaveBuf wbuf[NBUF];
static s16        *abuf;                 /* PRECISA ser memoria linear (linearAlloc) */
static LightEvent  audio_ev;
static volatile bool quit_audio;
static int         next_buf, h_act, a_act;       /* h_act/a_act: motor estava ligado no bloco anterior (fade ao ligar/desligar) */
static s16         tmpb[FRAMES * 2];               /* so a thread de audio usa */

/* ---------------- audio ---------------- */
static void fade(s16 *d, int frames, float from, float to) {
    for (int i = 0; i < frames; i++) {
        float g = from + (to - from) * ((float)i / (float)frames);
        d[i * 2] = (s16)(d[i * 2] * g); d[i * 2 + 1] = (s16)(d[i * 2 + 1] * g);
    }
}

static void fill_buf(int i) {
    s16 *dst = abuf + i * FRAMES * 2;
    int want_pause = g_pause;
    if (want_pause && pgain <= 0.0f) {                  /* pausado: silencio (ou so a gravacao que esta tocando), sem renderizar e sem gravar */
        if (player.active) play_pull(&player, dst, FRAMES); else memset(dst, 0, FRAMES * 2 * sizeof(s16));
        DSP_FlushDataCache(dst, FRAMES * 2 * sizeof(s16));
        ndspChnWaveBufAdd(0, &wbuf[i]);
        return;
    }
    /* 1.1: harsh, ambient e bateria tocam JUNTOS. Cada motor liga/desliga sozinho (com fade de 1 bloco) e so renderiza
     * enquanto esta ligado (poupa CPU). A soma dos dois motores passa por um limitador suave; a bateria soma por cima. */
    {
        int h_on = g_hon, a_on = g_aon, n_src = 0;
        memset(dst, 0, FRAMES * 2 * sizeof(s16));
        if (h_on || h_act) {
            float pv[P_COUNT];
            memcpy(pv, patch, sizeof pv);               /* copia consistente do patch */
            synth_render(&synth, pv, g_cx, g_cy, g_burst, g_mute, dst, FRAMES);
            if (!h_on) fade(dst, FRAMES, 1.0f, 0.0f); else if (!h_act) fade(dst, FRAMES, 0.0f, 1.0f);
            h_act = h_on; n_src++;
        }
        if (a_on || a_act) {
            AmbPatch a = ap;
            AmbIn in = { g_atouch, g_ax, g_ay, g_mute, g_cx, g_cy, g_freeze, g_throw, g_alive };
            amb_render(&amb, &a, &in, tmpb, FRAMES);
            if (!a_on) fade(tmpb, FRAMES, 1.0f, 0.0f); else if (!a_act) fade(tmpb, FRAMES, 0.0f, 1.0f);
            a_act = a_on; n_src++;
            for (int k = 0; k < FRAMES * 2; k++) {
                float o = ((float)dst[k] + (float)tmpb[k]) * (1.0f / 32768.0f), q = fabsf(o);
                if (n_src > 1 && q > 0.8f) o = (o > 0 ? 1.0f : -1.0f) * (0.8f + 0.19f * (q - 0.8f) / (q - 0.8f + 0.19f));
                dst[k] = (s16)(o * 32767.0f);
            }
        }
    }
    {
        GrvPat gp = gpat;                               /* copia consistente do padrao */
        GrvBank gb = gbank;
        if (g_mute) gp.level = 0.0f;
        grv_render(&grv, &gp, &gb, g_gsel, g_gchain, dst, FRAMES);
    }
    damage_process(&dmg, dst, FRAMES, g_dmg, g_dmgc);
    float tgt = want_pause ? 0.0f : 1.0f;
    if (pgain != tgt) { fade(dst, FRAMES, pgain, tgt); pgain = tgt; }
    rec_push(&rec, dst, FRAMES);                        /* grava o mix FINAL (sem a gravacao que esta tocando) */
    if (player.active) {                                /* 1.1: ouvir gravacoes: soma por cima, sem DAMAGE nem pausa */
        play_pull(&player, tmpb, FRAMES);
        for (int k = 0; k < FRAMES * 2; k++) {
            int v = (int)dst[k] + (int)tmpb[k]; dst[k] = (s16)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
        }
    }
    DSP_FlushDataCache(dst, FRAMES * 2 * sizeof(s16));  /* DSP le direto da RAM */
    ndspChnWaveBufAdd(0, &wbuf[i]);
}

static void audio_cb(void *unused) { (void)unused; if (!quit_audio) LightEvent_Signal(&audio_ev); }

static void audio_thread(void *arg) {
    (void)arg;
    while (!quit_audio) {
        while (wbuf[next_buf].status == NDSP_WBUF_DONE) {   /* respeita a ordem da fila */
            fill_buf(next_buf);
            next_buf = (next_buf + 1) % NBUF;
        }
        LightEvent_Wait(&audio_ev);
    }
}

/* escrita no SD fora da thread de audio */
static void writer_thread(void *arg) {
    (void)arg;
    while (!quit_writer) {
        LightLock_Lock(&rec_lock);
        rec_pump(&rec);
        play_pump(&player);
        LightLock_Unlock(&rec_lock);
        svcSleepThread(10 * 1000 * 1000LL);
    }
}

/* ---------------- gravacao / arquivos ---------------- */
static int rec_start(void) {
    mkdir("sdmc:/3ds", 0777); mkdir("sdmc:/3ds/intonarumori", 0777); mkdir(REC_DIR, 0777);
    LightLock_Lock(&rec_lock);
    int r = rec_open(&rec, REC_DIR, SYN_RATE, rec_ring);
    LightLock_Unlock(&rec_lock);
    return r;
}
static void rec_stop(void) {
    LightLock_Lock(&rec_lock);
    rec_close(&rec);
    LightLock_Unlock(&rec_lock);
}

static void play_stop(void) {
    LightLock_Lock(&rec_lock);
    play_close(&player);
    LightLock_Unlock(&rec_lock);
}
/* toca a gravacao `name`; tocar a que ja esta tocando para. 0 = comecou */
static char play_name[FNAME_MAX];
static int play_toggle(const char *name) {
    int same = player.fp && strcmp(play_name, name) == 0;
    play_stop();
    if (same) return 1;
    char p[96]; snprintf(p, sizeof p, REC_DIR "/%s", name);
    LightLock_Lock(&rec_lock);
    int r = play_open(&player, p, play_ring);
    LightLock_Unlock(&rec_lock);
    if (r == 0) snprintf(play_name, sizeof play_name, "%s", name);
    return r == 0 ? 0 : -1;
}

static FileEnt files[32]; static int nfiles;
static time_t fmtime[32];

static void file_path(char *out, size_t n, const char *name) { snprintf(out, n, REC_DIR "/%s", name); }

static void refresh_files(void) {
    nfiles = 0;
    DIR *d = opendir(REC_DIR);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) && nfiles < 32) {                 /* qualquer .wav da pasta (renomeados tambem) */
        size_t n = strlen(e->d_name);
        if (n < 5 || strcasecmp(e->d_name + n - 4, ".wav") || n >= sizeof files[0].name) continue;
        strcpy(files[nfiles].name, e->d_name);
        char p[96]; struct stat st; file_path(p, sizeof p, e->d_name);
        fmtime[nfiles] = stat(p, &st) == 0 ? st.st_mtime : 0;
        nfiles++;
    }
    closedir(d);
    for (int i = 1; i < nfiles; i++) {                        /* mais novo primeiro (por data; empate = nome) */
        FileEnt t = files[i]; time_t tm = fmtime[i]; int j = i - 1;
        while (j >= 0 && (fmtime[j] < tm || (fmtime[j] == tm && strcmp(files[j].name, t.name) < 0))) {
            files[j + 1] = files[j]; fmtime[j + 1] = fmtime[j]; j--;
        }
        files[j + 1] = t; fmtime[j + 1] = tm;
    }
    for (int i = 0; i < nfiles; i++) {
        char p[96]; file_path(p, sizeof p, files[i].name);
        if (rec_wav_info(p, &files[i].rate, &files[i].secs) != 0) { files[i].rate = 0; files[i].secs = 0; }
    }
}

static int is_recording_file(const char *name) {              /* nao mexe no arquivo que esta sendo gravado */
    if (!rec.active) return 0;
    char p[96]; file_path(p, sizeof p, name);
    return strcmp(p, rec.path) == 0;
}

/* teclado do sistema para renomear; devolve o indice do arquivo no fim (ou -1) */
static int rename_file(int idx) {
    if (idx < 0 || idx >= nfiles || is_recording_file(files[idx].name)) return -1;
    if (player.fp && strcmp(play_name, files[idx].name) == 0) play_stop();
    char base[FNAME_MAX]; snprintf(base, sizeof base, "%s", files[idx].name);
    size_t bl = strlen(base); if (bl > 4) base[bl - 4] = 0;              /* tira ".wav" */
    if (strlen(base) > 15) base[15] = 0;
    SwkbdState kb; char in[64] = "";
    swkbdInit(&kb, SWKBD_TYPE_NORMAL, 2, 15);
    swkbdSetInitialText(&kb, base);
    swkbdSetHintText(&kb, "NOME DA GRAVACAO");
    swkbdSetValidation(&kb, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    swkbdSetButton(&kb, SWKBD_BUTTON_LEFT, "CANCELAR", false);
    swkbdSetButton(&kb, SWKBD_BUTTON_RIGHT, "OK", true);
    if (swkbdInputText(&kb, in, sizeof in) != SWKBD_BUTTON_RIGHT) return -1;
    char nm[24]; size_t k = 0;                                           /* so ASCII seguro para o FAT */
    for (size_t i = 0; in[i] && k < 15; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c < 32 || c > 126) continue;
        if (strchr("\\/:*?\"<>|", c)) c = '_';
        nm[k++] = (char)c;
    }
    while (k > 0 && (nm[k - 1] == ' ' || nm[k - 1] == '.')) k--;
    size_t st0 = 0; while (st0 < k && nm[st0] == ' ') st0++;
    if (k - st0 == 0) return -1;
    memmove(nm, nm + st0, k - st0); k -= st0; nm[k] = 0;
    char np[96], op[96], fn[FNAME_MAX]; struct stat st;
    snprintf(fn, sizeof fn, "%s.wav", nm);
    if (strcmp(fn, files[idx].name) == 0) return idx;                    /* mesmo nome: nada a fazer */
    int same_ci = strcasecmp(fn, files[idx].name) == 0;                  /* so trocou maiuscula/minuscula */
    file_path(np, sizeof np, fn);
    for (int n = 2; !same_ci && stat(np, &st) == 0 && n < 100; n++) {    /* ja existe: nome-2, nome-3... */
        snprintf(fn, sizeof fn, "%s-%d.wav", nm, n); file_path(np, sizeof np, fn);
    }
    file_path(op, sizeof op, files[idx].name);
    if (rename(op, np) != 0) return -1;
    refresh_files();
    for (int i = 0; i < nfiles; i++) if (strcmp(files[i].name, fn) == 0) return i;
    return idx;
}

static int delete_file(int idx) {
    if (idx < 0 || idx >= nfiles || is_recording_file(files[idx].name)) return 0;
    if (player.fp && strcmp(play_name, files[idx].name) == 0) play_stop();
    char p[96]; file_path(p, sizeof p, files[idx].name);
    int ok = remove(p) == 0;
    refresh_files();
    return ok;
}

/* ---------------- util ---------------- */
static void error_screen(void) {
    u16 w, h;
    Surf top = { (uint16_t *)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &w, &h), 400, 240 };
    Surf bot = { (uint16_t *)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &w, &h), 320, 240 };
    memset(top.px, 0, 400 * 240 * 2);
    ui_draw_error(&bot, 0);
    gfxFlushBuffers();
    gfxSwapBuffers();
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) break;
        gspWaitForVBlank();
    }
}

static float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

/* DICE do ambient: com um humor escolhido na aba GRV, segue o humor */
static void adice(int pane) {
    uint32_t seed = (uint32_t)rand() | 1u;
    if (gpat.mood >= 0) amb_dice_mood(&ap, gpat.mood, seed, pane);
    else if (pane == 0) amb_dice_chords(&ap, seed);
    else amb_dice_lead(&ap, seed);
    if (pane == 0) { amb.prog_i = 0; amb.clk = 0; }
}

/* 1.0: demonio k (0..29). Se ha preset com o nome dele, carrega; senao rerola o vizinho com semente fixa. */
static void demon_load(float *patch, int k) {
    int p = demon_preset(k);
    if (p >= 0) { patch_preset(patch, p); return; }
    patch_seed((uint32_t)k * 7919u + 13u);
    patch_preset(patch, k / 2);
    patch_summon(patch, goetia_demon_depth(&goetia_registry[k]));
    patch_seed((uint32_t)rand() | 1u);
}
/* 1.1: padroes da bateria. gpat.cell/len = padrao editado; gbank guarda os 4 (espelhado a cada quadro) */
static void drum_sync(void) { memcpy(gbank.cell[g_gsel], gpat.cell, sizeof gpat.cell); gbank.len[g_gsel] = gpat.len == 14 ? 14 : 16; }
static void drum_select(int n) {
    if (n < 0 || n >= GRV_PATS) return;
    drum_sync(); g_gsel = n;
    memcpy(gpat.cell, gbank.cell[n], sizeof gpat.cell); gpat.len = gbank.len[n];
}
static int drum_save(void) {
    mkdir("sdmc:/3ds", 0777); mkdir("sdmc:/3ds/intonarumori", 0777);
    drum_sync();
    return grv_save(DRUM_FILE, &gpat, &gbank);
}
static int drum_load(void) {
    GrvBank b = gbank; GrvPat p = gpat;
    if (grv_load(DRUM_FILE, &p, &b) != 0) return -1;
    gbank = b; gpat.kit = p.kit; gpat.bpm = p.bpm; gpat.swing = p.swing; gpat.level = p.level;
    memcpy(gpat.cell, gbank.cell[g_gsel], sizeof gpat.cell); gpat.len = gbank.len[g_gsel];
    return 0;
}
static int demon_xy_slot(int k) { int p = demon_preset(k); return p >= 0 ? p : k / 2; }

int main(void) {
    gfxInit(GSP_RGB565_OES, GSP_RGB565_OES, false);
    gfxSetDoubleBuffering(GFX_BOTTOM, false);

    /* ---- NDSP: sem dspfirm.cdc o init falha ---- */
    if (R_FAILED(ndspInit())) { error_screen(); gfxExit(); return 0; }

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnReset(0);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, (float)SYN_RATE);
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);
    float mix[12];
    memset(mix, 0, sizeof mix);
    mix[0] = 1.0f; mix[1] = 1.0f;                       /* frente esq/dir */
    ndspChnSetMix(0, mix);

    abuf = (s16 *)linearAlloc(sizeof(s16) * 2 * FRAMES * NBUF);
    if (!abuf) { ndspExit(); gfxExit(); return 1; }

    u64 t = osGetTime();
    synth_init(&synth, (uint32_t)t ^ 0x9E3779B9u);
    amb_init(&amb, (uint32_t)(t >> 5) ^ 0x51ED270Bu);
    patch_seed((uint32_t)(t >> 3) | 1u);
    patch_default(patch);
    amb_default(&ap);
    damage_init(&dmg);
    grv_init(&grv);
    grv_default(&gpat);
    for (int i = 0; i < GRV_PATS; i++) { gbank.len[i] = 16; if (i == 0) memcpy(gbank.cell[0], gpat.cell, sizeof gpat.cell); }
    drum_load();                                         /* 1.1: se ja existe drum.dat no SD, carrega a bateria salva */
    int ptm_ok = R_SUCCEEDED(ptmuInit()); int batt = -1, batt_chg = 0;
    LightLock_Init(&rec_lock);
    memset(&rec, 0, sizeof rec);

    memset(wbuf, 0, sizeof wbuf);
    for (int i = 0; i < NBUF; i++) {
        wbuf[i].data_vaddr = abuf + i * FRAMES * 2;
        wbuf[i].nsamples   = FRAMES;
    }
    g_pause = 1; pgain = 0.0f;                          /* mudo durante a abertura (sem blip no boot) */
    for (int i = 0; i < NBUF; i++) fill_buf(i);         /* enche a fila antes de tocar */
    next_buf = 0;

    LightEvent_Init(&audio_ev, RESET_ONESHOT);
    ndspSetCallback(audio_cb, NULL);

    s32 prio = 0x30;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    Thread th  = threadCreate(audio_thread,  NULL, 32 * 1024, prio - 1, -1, false);
    Thread thw = threadCreate(writer_thread, NULL, 16 * 1024, prio + 1, -1, false);

    /* ---- estado da interface ---- */
    static const int set_n[SET_N] = { 0, 2, 2, 4, 3, 4, 4, 4, 0, 0 };      /* opcoes por linha do SET (0 = slider) */
    int page = PG_HARSH, sub = 0, preset = -1, apreset = 0, modified = 0, dirty = 2, hold = 0, pane = 0, write_on = 0;
    int hmod = 0, hrow = 0, amod = 0, arow = 0, asel = 0, setrow = 0, fsel = 0, fscroll = 0;
    /* SMOOTH, SPRING, LITE, STICKS (0 off,1 circle,2 c-stick,3 both), HOLD-Y (0 off,1 2s,2 3s), AUTO-MUT (0 off,1 10s,2 20s,3 40s) */
    float setv[SET_N] = { .35f, 0.0f, 0.0f, 3.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    u32 frame = 0, amt = 0;
    float level = 0.0f;
    static float snap[SCOPE_N];
    int drag = H_NONE, drag_arg = 0, was_touch = 0, last_sec = -1, del_arm = 0, yhold = 0, ylatch = 0, last_asig = -1; u32 del_t = 0;
    float sx = 0.5f, sy = 0.5f, save_x = 0, save_y = 0, save_a[2] = {0, 0}; int save_id[2] = {0, 0};
    float st_x = 0.0f, st_y = 0.0f;
    int csel = -1, csela = -1;       /* CODEX: sigilo selecionado no harsh / no ambient */
    int gmsg = 0; u32 gmsg_t = 0;    /* 1.1: aviso SAVED / LOADED da bateria */
    int ademon = -1;                 /* 1.0: demonio do ambient (0..29, -1 = o do preset) */
    int demon = -1, demon_t = 0;      /* 1.0: demonio invocado pelo SUMMON */
    int hid_x = P_PITCH, hid_y = P_DRIVE;
    patch_xy(15, &hid_x, &hid_y);
    patch_preset(patch, 15); preset = 15;
    amb_preset(&ap, 0);

    /* ---- abertura (painel VFD, ~3 s). Qualquer botao ou toque pula, depois de 0,4 s (um botao ainda segurado
     *      do Homebrew Launcher conta como "novo" no 1o scan e nao pode pular sozinho). Sem som: o app segue pausado ao fim. ---- */
    {
        u64 t0 = osGetTime(); int last = -1;
        for (;;) {
            hidScanInput();
            int ft = (int)((osGetTime() - t0) * 30 / 1000);
            if (ft >= ART_INTRO_FRAMES || !aptMainLoop()) break;
            if (ft > 12 && hidKeysDown()) break;
            if (ft != last) {
                u16 fw, fh;
                Surf top = { (uint16_t *)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &fw, &fh), 400, 240 };
                Surf bot = { (uint16_t *)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &fw, &fh), 320, 240 };
                art_intro(&top, &bot, ft);
                gfxFlushBuffers();
                gfxSwapBuffers();
                last = ft;
            }
            gspWaitForVBlank();
        }
        /* 1.1: o app comeca em SILENCIO porque os 3 motores (harsh, ambient, bateria) comecam PAUSADOS; cada um liga no seu botao
         * (ou ao escolher um demonio no CDX). A pausa geral abre solta para os botoes e as gravacoes tocarem direto. */
        g_pause = 0;
    }

    while (aptMainLoop() && th) {
        hidScanInput();
        u32 down = hidKeysDown(), held = hidKeysHeld();
        if (down & KEY_START) break;
        g_burst = (g_hon && (page == PG_HARSH || page == PG_CDXH || page == PG_DRM) && (held & KEY_X)) ? 1 : 0;
        g_mute  = (held & KEY_Y) ? 1 : 0;
        if (down & (KEY_ZL | KEY_ZR)) { g_pause ^= 1; dirty = 2; }

        /* ---- Y segurado 2 ou 3 s = PAUSE (Y continua mutando enquanto segura) ---- */
        int holdy_pct = 0;
        {
            int need = (int)(setv[4] + 0.5f) == 1 ? 120 : ((int)(setv[4] + 0.5f) == 2 ? 180 : 0);
            if (held & KEY_Y) yhold++; else { yhold = 0; ylatch = 0; }
            if (need && !ylatch && yhold >= need) { g_pause ^= 1; ylatch = 1; dirty = 2; }
            if (need && !ylatch && yhold > 12) holdy_pct = yhold * 100 / need > 100 ? 100 : yhold * 100 / need;
        }

        /* ---- analogico: Circle Pad e/ou C-Stick (SET > STICKS). X = bend, Y = drive (harsh) / brilho (ambient) ---- */
        {
            int sm = (int)(setv[3] + 0.5f);
            circlePosition cp = {0, 0}, cs = {0, 0};
            if (sm == 1 || sm == 3) hidCircleRead(&cp);
            if (sm == 2 || sm == 3) hidCstickRead(&cs);              /* sem Circle Pad Pro / New 3DS le 0 */
            float fx = (float)(cp.dx + cs.dx) / 140.0f, fy = (float)(cp.dy + cs.dy) / 140.0f;
            fx = fx > 1.0f ? 1.0f : (fx < -1.0f ? -1.0f : fx);
            fy = fy > 1.0f ? 1.0f : (fy < -1.0f ? -1.0f : fy);
            if (fabsf(fx) < 0.12f) fx = 0.0f;                         /* zona morta */
            if (fabsf(fy) < 0.12f) fy = 0.0f;
            if (sm == 0) { fx = 0.0f; fy = 0.0f; }
            st_x = fx; st_y = fy; g_cx = fx; g_cy = fy;
        }

        int lmode = ap.lead_on ? amb_ival(&ap, A_LMODE) : L_OFF;
        int amb_page = (page == PG_AMB || page == PG_CDXA);

        /* ---- paginas: L/R ---- */
        int newpage = page;
        if (down & KEY_L) newpage = (page + PG_COUNT - 1) % PG_COUNT;
        if (down & KEY_R) newpage = (page + 1) % PG_COUNT;

        /* ---- toque ---- */
        touchPosition tp = {0, 0};
        int touching = (held & KEY_TOUCH) ? 1 : 0;
        if (touching) hidTouchRead(&tp);
        UiState probe = { .page = page, .sub = sub, .module = page == PG_AMB ? amod : hmod, .ap = &ap, .v = patch,
                          .nfiles = nfiles, .fscroll = fscroll, .pane = pane };
        if (touching && (down & KEY_TOUCH)) {
            UiHit h = ui_hit(&probe, tp.px, tp.py);
            drag = h.kind; drag_arg = h.arg;
            if (h.kind != H_DEL) del_arm = 0;
            switch (h.kind) {
            case H_TAB:
                if (h.arg == page && (page == PG_HARSH || page == PG_AMB)) sub ^= 1; else newpage = h.arg;
                break;
            case H_PAUSE: g_pause ^= 1; break;
            case H_REC:
                if (rec.active) { rec_stop(); refresh_files(); } else if (rec_start() != 0) rec.error = 1;
                break;
            case H_PAD: sx = h.fx; sy = h.fy; save_x = patch[hid_x]; save_y = patch[hid_y]; break;
            case H_APAD:
                sx = h.fx; sy = h.fy;
                save_id[0] = A_SPEED; save_id[1] = A_DENS; save_a[0] = ap.p[A_SPEED]; save_a[1] = ap.p[A_DENS];
                break;
            case H_MODTILE: if (page == PG_AMB) { amod = h.arg; arow = 0; } else { hmod = h.arg; hrow = 0; } break;
            case H_VOL: patch[P_VOL] = h.fx; break;
            case H_AVOL: ap.p[A_VOL] = h.fx; break;
            case H_SLIDER: case H_PILL: case H_STEPL: case H_STEPR: {
                int id = h.arg, dir = h.kind == H_STEPR ? 1 : -1;
                if (page == PG_SET) {
                    if (h.kind == H_SLIDER) setv[id] = h.fx;
                    else if (h.kind == H_PILL) setv[id] = (float)h.arg2;
                    setrow = h.row;
                } else if (page == PG_AMB) {
                    if (h.kind == H_SLIDER) amb_set_from_pos(&ap, id, h.fx);
                    else if (h.kind == H_PILL) amb_set_index(&ap, id, h.arg2);
                    else amb_adjust(&ap, id, dir, 0.0f);
                    if (h.row >= 0) arow = h.row;
                } else {
                    int n = param_discrete(id);
                    if (h.kind == H_SLIDER) patch[id] = h.fx;
                    else if (h.kind == H_PILL) patch[id] = ((float)h.arg2 + 0.5f) / (float)n;
                    else param_adjust(patch, id, dir, 0.0f);
                    hrow = h.row; modified = 1;
                }
            } break;
            case H_FREEZE: g_freeze ^= 1; break;
            case H_HPLAY: g_hon ^= 1; break;
            case H_APLAY: g_aon ^= 1; break;
            case H_CODEX: {      /* 1.1 CDX-H: carrega o demonio no HARSH (e liga o harsh) */
                int k = h.arg % 30;
                csel = k; demon_load(patch, k); demon = k; demon_t = 90; preset = demon_preset(k); modified = 0; patch_xy(demon_xy_slot(k), &hid_x, &hid_y); g_hon = 1;
            } break;
            case H_CODEXA: {     /* 1.1 CDX-A: carrega o demonio no AMBIENT (e liga o ambient) */
                int k = h.arg % 30; uint8_t cg = ap.cgen; csela = k; ademon = k;
                asel = amb_load_demon(&ap, k, goetia_demon_depth(&goetia_registry[k]));
                ap.cgen = (uint8_t)(cg + 1); apreset = asel; amb.prog_i = 0; amb.clk = 0; g_aon = 1; demon_t = 90;
            } break;
            case H_PREV: case H_NEXT: {      /* 1.0: percorre os 30 demonios (pares = presets, impares = gerados por demonio) */
                int k = preset >= 0 ? preset_demon(preset) : (demon >= 0 ? demon : 0);
                k = (k + 30 + (h.kind == H_NEXT ? 1 : -1)) % 30;
                demon_load(patch, k); demon = k; demon_t = 90; preset = demon_preset(k); modified = 0; patch_xy(demon_xy_slot(k), &hid_x, &hid_y);
            } break;
            case H_MUTATE: patch_mutate(patch); modified = 1; break;
            case H_RANDOM: demon = (int)(rand() % goetia_count); demon_t = 90; patch_summon(patch, goetia_demon_depth(&goetia_registry[demon])); preset = -1; modified = 0; break;
            case H_EDIT: sub = 1; break;
            /* ---- ambient: camadas, acordes em tempo real, timbres, modos do lead ---- */
            case H_DRONE: if (pane == 0) ap.drone_on ^= 1; else pane = 0; break;
            case H_LEAD:  if (pane == 1) ap.lead_on ^= 1; else pane = 1; break;
            case H_KEY: ap.mask ^= (uint16_t)(1u << h.arg); break;
            case H_CPAD: ap.cdeg = (uint8_t)h.arg; ap.cgen++; if (write_on) amb_write_chord(&ap, h.arg); break;
            case H_CMOD: ap.cmod ^= (uint8_t)h.arg; break;
            case H_CSLOT: { int d = ap.prog[h.arg], nd = amb_nd(&ap); amb_set_slot(&ap, h.arg, d + 1 >= nd ? -1 : d + 1); } break;
            case H_CWRITE: write_on ^= 1; break;
            case H_CDICE: adice(0); break;
            case H_TIMB: amb_set_index(&ap, pane == 0 ? A_DSND : A_LSND, h.arg); break;
            case H_LMODE: amb_set_index(&ap, A_LMODE, h.arg); ap.lead_on = 1; break;
            case H_SEQCELL: ap.seq[h.arg] = (int8_t)h.arg2; break;
            case H_EPAR: amb_set_from_pos(&ap, h.arg, h.fx); break;
            case H_LDICE: adice(1); break;
            /* ---- GROOVE ---- */
            case H_GPLAY: gpat.on ^= 1; break;
            case H_GPAT: drum_select(h.arg); break;
            case H_GCHAIN: g_gchain ^= 1; break;
            case H_GSAVE: gmsg = drum_save() == 0 ? 1 : 3; gmsg_t = frame; break;
            case H_GLOAD: gmsg = drum_load() == 0 ? 2 : 3; gmsg_t = frame; break;
            case H_GKIT: gpat.kit = (uint8_t)((gpat.kit + GRV_KITS + h.arg) % GRV_KITS); break;
            case H_GDICE: grv_dice(&gpat, (uint32_t)rand() | 1u); break;
            case H_GCLR: grv_clear(&gpat); gpat.len = 16; break;
            case H_GCELL: gpat.cell[h.arg][h.arg2] = (uint8_t)((gpat.cell[h.arg][h.arg2] + 1) % 5); break;
            case H_GMOOD: gpat.mood = (int8_t)(gpat.mood == h.arg ? -1 : h.arg); if (gpat.mood >= 0) grv_dice(&gpat, (uint32_t)rand() | 1u); break;
            case H_GSLIDE: *(h.arg == 0 ? &gpat.bpm : (h.arg == 1 ? &gpat.swing : &gpat.level)) = h.fx; break;
            case H_LXTRA: {
                int lm = amb_ival(&ap, A_LMODE);
                if (lm == L_KEYS) amb_set_index(&ap, A_KHOLD, amb_ival(&ap, A_KHOLD) ? 0 : 1);
                else if (lm == L_SEQ) { int8_t f = ap.seq[0]; for (int i = 0; i < 7; i++) ap.seq[i] = ap.seq[i + 1]; ap.seq[7] = f; }
                else if (lm == L_EUCL) amb_adjust(&ap, A_EROT, 1, 0.0f);
                else amb_adjust(&ap, A_LOCT, 1, 0.0f);
            } break;
            case H_FUP: if (fsel > 0) fsel--; break;
            case H_FDN: if (fsel + 1 < nfiles) fsel++; break;
            case H_FILE: fsel = h.arg; break;
            case H_FPLAY: if (fsel < nfiles) play_toggle(files[fsel].name); break;
            case H_REN: { int r = rename_file(fsel); if (r >= 0) fsel = r; } break;
            case H_DEL:
                if (nfiles == 0) break;
                if (!del_arm) { del_arm = 1; del_t = frame; }                 /* 1o toque arma, 2o apaga */
                else { delete_file(fsel); del_arm = 0; if (fsel >= nfiles) fsel = nfiles ? nfiles - 1 : 0; }
                break;
            default: break;
            }
            dirty = 2;
        }
        if (del_arm && frame - del_t > 150) { del_arm = 0; dirty = 2; }        /* ~2,5 s sem confirmar: desarma */

        if (touching && !(down & KEY_TOUCH)) {                 /* arrasto continuo */
            if (drag == H_PAD || drag == H_APAD) {
                int rx = UI_PAD_X, ry = UI_PAD_Y, rw = UI_PAD_W, rh = UI_PAD_H, ok = 1;
                if (drag == H_APAD) ok = ui_apad_rect(&probe, &rx, &ry, &rw, &rh);
                if (ok) {
                    float fx = clamp01((float)(tp.px - rx) / (float)(rw - 1));
                    float fy = clamp01(1.0f - (float)(tp.py - ry) / (float)(rh - 1));
                    float al = 1.0f - 0.85f * setv[0];
                    sx += (fx - sx) * al; sy += (fy - sy) * al;
                    dirty = 2;
                }
            } else if (drag == H_SLIDER || drag == H_VOL || drag == H_AVOL) {
                float fx = clamp01((float)(tp.px - UI_SLD_X) / (float)UI_SLD_W);
                if (drag == H_VOL) patch[P_VOL] = fx;
                else if (drag == H_AVOL) ap.p[A_VOL] = fx;
                else if (page == PG_SET) { setv[drag_arg] = fx; }
                else if (page == PG_AMB) amb_set_from_pos(&ap, drag_arg, fx);
                else { patch[drag_arg] = fx; modified = 1; }
                dirty = 2;
            } else if (drag == H_GSLIDE) {
                float fx = clamp01((float)(tp.px - UI_GS_X) / (float)UI_GS_W);
                *(drag_arg == 0 ? &gpat.bpm : (drag_arg == 1 ? &gpat.swing : &gpat.level)) = fx; dirty = 2;
            } else if (drag == H_EPAR) {
                amb_set_from_pos(&ap, drag_arg, clamp01((float)(tp.px - UI_ES_X) / (float)UI_ES_W)); dirty = 2;
            } else if (drag == H_SEQCELL) {
                int st, v; if (ui_seq_cell(tp.px, tp.py, &st, &v)) { ap.seq[st] = (int8_t)v; dirty = 2; }
            }
        }
        /* aplica o pad enquanto tocado */
        g_atouch = 0;
        { int th = (touching && drag == H_THROW && page == PG_AMB && sub == 0) ? 1 : 0; if (th != g_throw) dirty = 2; g_throw = th; }   /* THROW: so enquanto segura */
        if (touching && drag == H_PAD) { patch[hid_x] = sx; patch[hid_y] = sy; modified = 1; }
        if (touching && drag == H_APAD) {
            if (lmode == L_KEYS) { g_ax = sx; g_ay = sy; g_atouch = 1; }
            else { ap.p[save_id[0]] = sx; ap.p[save_id[1]] = sy; }
        }
        if (!touching && was_touch) {                          /* soltou */
            if (setv[1] >= 0.5f) {                             /* modo SPRING */
                if (drag == H_PAD) { patch[hid_x] = save_x; patch[hid_y] = save_y; }
                if (drag == H_APAD && lmode != L_KEYS) { ap.p[save_id[0]] = save_a[0]; ap.p[save_id[1]] = save_a[1]; }
            }
            drag = H_NONE; dirty = 2;
        }
        was_touch = touching;

        /* ---- botoes fisicos ---- */
        if (down & KEY_SELECT) { if (page == PG_HARSH || page == PG_AMB) sub ^= 1; dirty = 2; }
        if (down & KEY_A) {
            if (page == PG_DRM) gpat.on ^= 1;
            else if (amb_page) { ap.cdeg = (uint8_t)((amb.cur_deg_ui + 1) % amb_nd(&ap)); ap.cgen++; }
            else if (page != PG_FILES && page != PG_SET) { patch_mutate(patch); modified = 1; }
            dirty = 2;
        }
        if (down & KEY_B) {
            if (page == PG_FILES) { if (fsel < nfiles) play_toggle(files[fsel].name); }
            else if (page == PG_DRM) grv_dice(&gpat, (uint32_t)rand() | 1u);
            else if (amb_page) adice(pane);
            else { demon = (int)(rand() % goetia_count); demon_t = 90; patch_summon(patch, goetia_demon_depth(&goetia_registry[demon])); preset = -1; modified = 0; }
            dirty = 2;
        }
        if ((down & KEY_X) && page == PG_AMB && sub == 0) { pane ^= 1; dirty = 2; }
        int dir = (held & KEY_DRIGHT) ? 1 : ((held & KEY_DLEFT) ? -1 : 0);
        if (down & (KEY_DLEFT | KEY_DRIGHT)) hold = 0;
        if (dir) hold++; else hold = 0;
        int step_now = dir && (hold == 1 || hold > 14);
        float stp = hold > 60 ? 0.03f : 0.01f;
        switch (page) {
        case PG_HARSH:
            if (sub == 0) {
                if (down & (KEY_DLEFT | KEY_DRIGHT)) {
                    int k = preset >= 0 ? preset_demon(preset) : (demon >= 0 ? demon : 0);
                    k = (k + 30 + ((down & KEY_DRIGHT) ? 1 : -1)) % 30;
                    demon_load(patch, k); demon = k; demon_t = 90; preset = demon_preset(k); modified = 0; patch_xy(demon_xy_slot(k), &hid_x, &hid_y); dirty = 2;
                }
                if (held & (KEY_DUP | KEY_DDOWN)) { patch[P_VOL] = clamp01(patch[P_VOL] + ((held & KEY_DUP) ? 0.005f : -0.005f)); dirty = 2; }
            } else {
                const Module *md = &modules[hmod];
                if (down & (KEY_DUP | KEY_DDOWN)) { hrow = (hrow + ((down & KEY_DDOWN) ? 1 : md->count - 1)) % md->count; dirty = 2; }
                if (step_now) {
                    int id = md->first + hrow;
                    if (param_discrete(id)) { if (hold == 1) param_adjust(patch, id, dir, 0.0f); }
                    else param_adjust(patch, id, dir, stp);
                    modified = 1; dirty = 2;
                }
            }
            break;
        case PG_AMB:
            if (sub == 0) {                                    /* <  > = acorde anterior/proximo (tempo real) · ^ v = timbre da camada */
                if (down & (KEY_DLEFT | KEY_DRIGHT)) {
                    int nd = amb_nd(&ap), c = amb.cur_deg_ui + ((down & KEY_DRIGHT) ? 1 : nd - 1);
                    ap.cdeg = (uint8_t)(c % nd); ap.cgen++; dirty = 2;
                }
                if (down & (KEY_DUP | KEY_DDOWN)) { amb_adjust(&ap, pane == 0 ? A_DSND : A_LSND, (down & KEY_DDOWN) ? 1 : -1, 0.0f); dirty = 2; }
            } else {
                const AGroup *g = &amb_groups[amod];
                if (down & (KEY_DUP | KEY_DDOWN)) { arow = (arow + ((down & KEY_DDOWN) ? 1 : g->count - 1)) % g->count; dirty = 2; }
                if (step_now) {
                    int id = g->first + arow;
                    if (amb_discrete(id)) { if (hold == 1) amb_adjust(&ap, id, dir, 0.0f); }
                    else amb_adjust(&ap, id, dir, stp);
                    dirty = 2;
                }
            }
            break;
        case PG_CDXH:
            if (down & (KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT)) {
                int d = (down & KEY_DDOWN) ? 6 : ((down & KEY_DUP) ? -6 : ((down & KEY_DRIGHT) ? 1 : -1));
                int k = csel >= 0 ? csel : (preset >= 0 ? preset_demon(preset) : (demon >= 0 ? demon : 0));
                k = (k + 30 + d) % 30; csel = k;
                demon_load(patch, k); demon = k; demon_t = 90; preset = demon_preset(k); modified = 0; patch_xy(demon_xy_slot(k), &hid_x, &hid_y); g_hon = 1; dirty = 2;
            }
            break;
        case PG_CDXA:
            if (down & (KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT)) {
                int d = (down & KEY_DDOWN) ? 6 : ((down & KEY_DUP) ? -6 : ((down & KEY_DRIGHT) ? 1 : -1));
                uint8_t cg = ap.cgen;
                int k = csela >= 0 ? csela : (ademon >= 0 ? ademon : amb_preset_demon(apreset));
                k = (k + 30 + d) % 30; ademon = k; csela = k;
                asel = amb_load_demon(&ap, k, goetia_demon_depth(&goetia_registry[k]));
                demon_t = 90;
                ap.cgen = (uint8_t)(cg + 1); apreset = asel; amb.prog_i = 0; amb.clk = 0; g_aon = 1; dirty = 2;
            }
            break;
        case PG_DRM:
            if (down & (KEY_DLEFT | KEY_DRIGHT)) { gpat.kit = (uint8_t)((gpat.kit + GRV_KITS + ((down & KEY_DRIGHT) ? 1 : -1)) % GRV_KITS); dirty = 2; }
            break;
        case PG_FILES:
            if ((down & KEY_DDOWN) && fsel + 1 < nfiles) { fsel++; del_arm = 0; dirty = 2; }
            if ((down & KEY_DUP) && fsel > 0) { fsel--; del_arm = 0; dirty = 2; }
            if (down & KEY_A) { int r = rename_file(fsel); if (r >= 0) fsel = r; dirty = 2; }
            break;
        case PG_SET:
            if (down & (KEY_DUP | KEY_DDOWN)) { setrow = (setrow + ((down & KEY_DDOWN) ? 1 : SET_N - 1)) % SET_N; dirty = 2; }
            if (dir && hold == 1) {
                if (set_n[setrow] == 0) setv[setrow] = clamp01(setv[setrow] + dir * 0.1f);
                else { int n = set_n[setrow], v = (int)(setv[setrow] + 0.5f) + dir; if (v < 0) v = n - 1; if (v >= n) v = 0; setv[setrow] = (float)v; }
                dirty = 2;
            }
            break;
        }
        if (fsel < fscroll) fscroll = fsel;
        if (fsel >= fscroll + MAX_FILES) fscroll = fsel - (MAX_FILES - 1);
        if (fscroll < 0) fscroll = 0;

        if (newpage != page) {
            page = newpage; del_arm = 0; dirty = 2; if (page == PG_FILES) refresh_files();
        }
        if (page == PG_AMB || page == PG_CDXA) g_engine = 1; else if (page == PG_HARSH || page == PG_CDXH) g_engine = 0;

        g_alive = (int)(setv[7] + 0.5f) & 3;
        g_dmg = setv[8]; g_dmgc = setv[9];
        /* ---- JOURNEY (1.0): o ambient viaja sozinho para o proximo preset; 12 s de planeio, a tonalidade troca no meio ---- */
        {
            static const u32 jr_fr[4] = { 0, 1800, 3600, 7200 };
            static int jt = -1, jap = 0, jdm = -1, jnext = 0; static u32 jdue = 0; static AmbPatch jfrom, jto;
            int jm = (int)(setv[6] + 0.5f); if (jm < 0 || jm > 3) jm = 0;
            if (!jm) { jt = -1; jdue = frame; }
            else if (jt < 0) {
                if (!g_aon || g_pause) jdue = frame;
                else if (frame - jdue >= jr_fr[jm]) {
                    jdue = frame; jnext = (apreset + 1) % A_NPRESETS; jfrom = ap; amb_preset(&jto, jnext); jt = 0; jap = apreset; jdm = ademon;
                }
            }
            if (jt >= 0) {
                if (apreset != jap || ademon != jdm) jt = -1;                 /* voce mexeu: cancela a viagem */
                else if (!g_pause) {
                    jt++; jfrom.p[A_VOL] = ap.p[A_VOL];
                    amb_travel(&ap, &jfrom, &jto, (float)jt / 720.0f);
                    if (jt == 360) { ap.cgen++; apreset = jap = jnext; asel = jnext; ademon = jdm = -1; amb.prog_i = 0; amb.clk = 0; dirty = 2; }
                    if (jt >= 720) { jt = -1; jdue = frame; }
                }
            }
        }

        /* ---- AUTO-MUT: harsh deriva sozinho; ambient sorteia um padrao novo para o lead ---- */
        {
            static const u32 am_fr[4] = { 0, 600, 1200, 2400 };
            int am = (int)(setv[5] + 0.5f); if (am < 0 || am > 3) am = 0;
            if (!am || g_pause) amt = frame;
            else if (frame - amt >= am_fr[am]) {
                amt = frame;
                if (g_hon) { patch_mutate(patch); modified = 1; }
                if (g_aon) amb_dice_lead(&ap, (uint32_t)rand() | 1u);
                dirty = 2;
            }
        }

        if (player.fp && !player.active) { play_stop(); dirty = 2; }        /* acabou sozinha */
        {   /* progresso da reproducao na aba WAV (4 vezes por segundo) */
            static int last_ps = -1; int ps = player.fp ? (int)(play_seconds(&player) * 4.0f) : -1;
            if (ps != last_ps) { last_ps = ps; if (page == PG_FILES) dirty = dirty > 0 ? dirty : 1; }
        }
        /* ---- erro de gravacao: fecha o arquivo ---- */
        if (rec.error && rec.active) { rec_stop(); refresh_files(); }
        int sec = rec.active ? (int)rec_seconds(&rec) : -1;
        if (sec != last_sec) { last_sec = sec; dirty = 2; }

        /* ---- a tela de baixo do ambient mostra playhead/acorde atual: redesenha quando mudam ---- */
        int plen = amb_plen(&ap);
        int cur_slot = amb.prog_i > 0 ? (amb.prog_i - 1) % plen : -1;
        {
            int asig = amb.cur_deg_ui | (amb.step_ui & 31) << 4 | (cur_slot + 1) << 10;
            if (page == PG_AMB && sub == 0 && asig != last_asig) dirty = dirty > 0 ? dirty : 1;
            last_asig = asig;
        }

        drum_sync();                                           /* 1.1: espelha o padrao editado no banco (a CHAIN toca do banco) */
        if (gmsg && frame - gmsg_t > 90) { gmsg = 0; dirty = 2; }
        if (ptm_ok && !(frame % 150)) {                        /* 1.1: bateria do aparelho (0..5) a cada ~2,5 s */
            u8 lv = 0, ch = 0;
            if (R_SUCCEEDED(PTMU_GetBatteryLevel(&lv))) { int nb = lv > 5 ? 5 : (int)lv; if (nb != batt) dirty = dirty > 0 ? dirty : 1; batt = nb; }
            if (R_SUCCEEDED(PTMU_GetBatteryChargeState(&ch))) batt_chg = ch ? 1 : 0;
        }
        {   /* GROOVE: redesenha o playhead */
            static int last_gs = -1, last_gc = -1;
            if (page == PG_DRM && (grv.step_ui != last_gs || grv.cur_ui != last_gc)) dirty = dirty > 0 ? dirty : 1;
            last_gs = grv.step_ui; last_gc = grv.cur_ui;
        }

        /* ---- desenho (30 fps na tela de cima; embaixo so quando muda) ---- */
        if (!(frame & (setv[2] >= 0.5f ? 3u : 1u))) {
            int amb_on = (g_engine == 1);
            unsigned w = amb_on ? (unsigned)amb.scope_w : (unsigned)synth.scope_w;
            const float *src = amb_on ? amb.scope : synth.scope;
            for (int i = 0; i < SCOPE_N; i++) snap[i] = src[(w - SCOPE_N + (unsigned)i) & (SCOPE_N - 1)];
            int spp = 2, trig = 0;
            if (!amb_on && param_hz(patch[P_PITCH]) > 600.0f) spp = 1;
            for (int i = 0; i < 1024; i++)
                if (snap[i] < 0.0f && snap[i + 1] >= 0.0f) { trig = i; break; }
            float pk = amb_on ? amb.peak : synth.peak;
            level = pk > level ? pk : level * 0.85f;

            float venv[AMB_V];
            for (int i = 0; i < AMB_V; i++) venv[i] = amb.venv[i];
            UiState ui = { .page = page, .sub = sub, .v = patch, .module = page == PG_AMB ? amod : hmod,
                           .row = page == PG_AMB ? arow : hrow, .preset = preset, .apreset = apreset, .pane = pane,
                           .cur_deg = amb.cur_deg_ui, .cur_slot = cur_slot, .step_ui = amb.step_ui, .nsteps_ui = amb.nsteps_ui,
                           .hit_ui = amb.hit_ui, .write_on = write_on,
                           .modified = modified, .mute = g_mute, .burst = g_burst, .paused = g_pause, .ap = &ap, .arow = arow,
                           .notes_pc = amb.notes_pc, .cur_note = amb.cur_note ? amb.cur_note : 57, .venv = venv,
                           .engine = amb_on, .level = level > 1.0f ? 1.0f : level, .scope = snap + trig, .spp = spp,
                           .frame = (int)(frame >> 1), .xid = hid_x, .yid = hid_y, .touching = touching && (drag == H_PAD || drag == H_APAD),
                           .tx = sx, .ty = sy, .rec_on = rec.active, .rec_err = rec.error,
                           .rec_secs = rec.active ? rec_seconds(&rec) : 0.0f, .rec_drop = rec.dropped,
                           .setrow = setrow, .files = files, .nfiles = nfiles, .fscroll = fscroll, .fsel = fsel,
                           .del_arm = del_arm, .stick_x = st_x, .stick_y = st_y, .holdy = holdy_pct,
                           .lite = setv[2] >= 0.5f ? 1 : 0, .demon = demon, .demon_t = demon_t, .ademon = ademon, .codex_sel = page == PG_CDXA ? csela : csel, .grv = &gpat, .grv_step = grv.step_ui, .freeze = g_freeze, .throwing = g_throw,
                           .gbank = &gbank, .gsel = g_gsel, .gchain = g_gchain, .gcur = grv.cur_ui, .gmsg = gmsg, .hon = g_hon, .aon = g_aon,
                           .batt = batt, .batt_chg = batt_chg,
                           .playing = player.fp != NULL, .play_pos = player.fp ? play_seconds(&player) : 0.0f, .play_len = player.fp ? play_length(&player) : 0.0f, .play_idx = -1 };
            if (player.fp) for (int i = 0; i < nfiles; i++) if (strcmp(files[i].name, play_name) == 0) ui.play_idx = i;
            memcpy(ui.set_v, setv, sizeof setv);
            u16 fw, fh;
            Surf top = { (uint16_t *)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &fw, &fh), 400, 240 };
            ui_draw_top(&top, &ui);
            if (dirty > 0) {                 /* 2 vezes: cobre qualquer troca de buffer da tela de baixo */
                Surf bot = { (uint16_t *)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &fw, &fh), 320, 240 };
                ui_draw_bottom(&bot, &ui);
                dirty--;
            }
            gfxFlushBuffers();
            gfxSwapBuffers();
        }
        frame++; if (demon_t > 0) demon_t--;
        gspWaitForVBlank();
    }

    /* ---- encerramento limpo ---- */
    if (rec.active) rec_stop();
    play_stop();
    quit_writer = true; quit_audio = true;
    if (thw) { threadJoin(thw, U64_MAX); threadFree(thw); }
    if (th) { LightEvent_Signal(&audio_ev); threadJoin(th, U64_MAX); threadFree(th); }
    ndspChnReset(0);
    ndspExit();
    if (ptm_ok) ptmuExit();
    linearFree(abuf);
    gfxExit();
    return 0;
}
